/****************************************************************************
 *
 *   Copyright (C) 2020 Xiaomi InC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <common/log.h>

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04
#define H4_ISO  0x05

//#define HCI_DEBUG

static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
static struct k_thread        rx_thread_data;
static struct file            g_filep;

#ifdef CONFIG_BT_H4_DEBUG
static void h4_data_dump(const char *tag, uint8_t type, uint8_t *data, uint32_t len)
{
	struct iovec bufs[2];

	bufs[0].iov_base = &type;
	bufs[0].iov_len = 1;
	bufs[1].iov_base = data;
	bufs[1].iov_len = len;

	lib_dumpvbuffer(tag, bufs, 2);
}
#endif

static int h4_recv_data(uint8_t *buf, size_t count)
{
	ssize_t ret, nread = 0;

	while (count != nread) {
		ret = file_read(&g_filep, buf + nread, count - nread);
		if (ret < 0) {
			return ret;
		}

		nread += ret;
	}

	return nread;
}

static int h4_send_data(uint8_t *buf, size_t count)
{
	ssize_t ret, nwritten = 0;

	while (nwritten != count) {
		ret = file_write(&g_filep, buf + nwritten, count - nwritten);
		if (ret < 0) {
			return ret;
		}

		nwritten += ret;
	}

	return nwritten;
}

static void h4_rx_thread(void *p1, void *p2, void *p3)
{
	unsigned char buffer[1];
	int hdr_len, data_len, ret;
	struct net_buf *buf;
	bool discardable;
	uint8_t type;
	uint8_t event;
	union {
		struct bt_hci_evt_hdr evt;
		struct bt_hci_acl_hdr acl;
		struct bt_hci_iso_hdr iso;
	} hdr;

	for (;;) {
		ret = h4_recv_data(&type, 1);
		if (ret != 1)
			break;

		if (type != H4_EVT &&
				type != H4_ACL &&
				type != H4_ISO)
			continue;

		if (type == H4_EVT)
			hdr_len = sizeof(struct bt_hci_evt_hdr);
		else if (type == H4_ACL)
			hdr_len = sizeof(struct bt_hci_acl_hdr);
		else if (IS_ENABLED(CONFIG_BT_ISO) && type == H4_ISO)
			hdr_len = sizeof(struct bt_hci_iso_hdr);
		else
			continue;

		ret = h4_recv_data((uint8_t *)&hdr, hdr_len);
		if (ret != hdr_len)
			break;

		if (type == H4_EVT) {
			data_len = hdr.evt.len;
			discardable = false;
			if (hdr.evt.evt == BT_HCI_EVT_LE_META_EVENT) {
				ret = h4_recv_data(&event, 1);
				if (ret != 1)
					break;

				if (event == BT_HCI_EVT_LE_ADVERTISING_REPORT)
					discardable = true;
			}

			buf = bt_buf_get_evt(hdr.evt.evt, discardable,
					discardable ? K_NO_WAIT : K_FOREVER);
			if (buf == NULL) {
				if (discardable && data_len) {
					while(--data_len) {
						h4_recv_data(buffer, 1);
					}

					continue;
				} else
					break;
			}

			memcpy(buf->data, &hdr, hdr_len);

			if (hdr.evt.evt == BT_HCI_EVT_LE_META_EVENT) {
				buf->data[sizeof(struct bt_hci_evt_hdr)] = event;
				hdr_len += 1;
				data_len -= 1;
			}

			bt_buf_set_type(buf, BT_BUF_EVT);
		} else if (type == H4_ACL) {
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
			if (buf == NULL)
				break;

			data_len = hdr.acl.len;
			bt_buf_set_type(buf, BT_BUF_ACL_IN);
			memcpy(buf->data, &hdr, hdr_len);
		} else if (IS_ENABLED(CONFIG_BT_ISO) && type == H4_ISO) {
			buf = bt_buf_get_rx(BT_BUF_ISO_IN, K_FOREVER);
			if (buf == NULL)
				break;

			data_len = hdr.iso.len;
			bt_buf_set_type(buf, BT_BUF_ISO_IN);
			memcpy(buf->data, &hdr, hdr_len);
		} else
			break;


		if (data_len + hdr_len > buf->size) {
			net_buf_unref(buf);
			continue;
		}

		ret = h4_recv_data(buf->data + hdr_len, data_len);
		if (ret != data_len)
			break;

		net_buf_add(buf, hdr_len + data_len);

#ifdef CONFIG_BT_H4_DEBUG
		h4_data_dump("BT RX", type, buf->data, hdr_len + data_len);
#endif
		bt_recv(buf);
	}

	BT_ASSERT(false);
}

static int h4_open(void)
{
	int ret;

	ret = file_open(&g_filep, CONFIG_BT_UART_ON_DEV_NAME, O_RDWR | O_BINARY);
	if (ret < 0)
		goto bail;

	ret = (int)k_thread_create(&rx_thread_data, rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack),
			h4_rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	if (ret < 0)
		goto bail;
	else
		ret = 0;

	k_thread_name_set(&rx_thread_data, "BT Driver");

	return 0;

bail:
	file_close(&g_filep);

	return ret;
}

static int h4_send(struct net_buf *buf)
{
	uint8_t type;
	int ret;

	switch (bt_buf_get_type(buf)) {
		case BT_BUF_ACL_OUT:
			type = H4_ACL;
			break;
		case BT_BUF_CMD:
			type = H4_CMD;
			break;
		case BT_BUF_ISO_OUT:
			type = H4_ISO;
			break;
		default:
			ret = -EINVAL;
			goto bail;
	}

#ifdef CONFIG_BT_H4_DEBUG
	h4_data_dump("BT TX", type, buf->data, buf->len);
#endif

	ret = h4_send_data(&type, 1);
	if (ret != 1) {
		ret = -EINVAL;
	}

	ret = h4_send_data(buf->data, buf->len);
	if (ret != buf->len) {
		ret = -EINVAL;
	}

bail:
	net_buf_unref(buf);

	return ret < 0 ? ret : 0;
}

static struct bt_hci_driver driver = {
	.name   = "H:4",
	.bus    = BT_HCI_DRIVER_BUS_UART,
	.open   = h4_open,
	.send   = h4_send,
#if defined(CONFIG_BT_QUIRK_NO_RESET)
	.quirks = BT_QUIRK_NO_RESET,
#endif
};

static int bt_uart_init(const struct device *dev)
{
	return bt_hci_driver_register(&driver);
}

SYS_INIT(bt_uart_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
