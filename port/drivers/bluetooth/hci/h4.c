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
#include <common/log.h>

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04
#define H4_ISO  0x05

static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
static struct k_thread        rx_thread_data;
static struct file            g_filep;
static int                    g_fd = -1;

static int h4_recv_data(uint8_t *buf, size_t count)
{
	ssize_t ret, nread = 0;

	while (count != nread) {
		ret = file_read(&g_filep, buf + nread, count - nread);
		if (ret < 0) {
			if (ret == -EAGAIN) {
				usleep(500);
				continue;
			} else
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
			if (ret == -EAGAIN) {
				usleep(500);
				continue;
			} else
				return ret;
		}

		nwritten += ret;
	}

	return nwritten;
}

static void h4_rx_thread(void *p1, void *p2, void *p3)
{
	int hdr_len, data_len, ret;
	struct net_buf *buf;
	uint8_t type;
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
			buf = bt_buf_get_evt(hdr.evt.evt, false, K_FOREVER);
			data_len = hdr.evt.len;
			type = BT_BUF_EVT;
		} else if (type == H4_ACL) {
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
			data_len = hdr.acl.len;
			type = BT_BUF_ACL_IN;
		} else if (IS_ENABLED(CONFIG_BT_ISO) && type == H4_ISO) {
			buf = bt_buf_get_rx(BT_BUF_ISO_IN, K_FOREVER);
			data_len = hdr.iso.len;
			type = BT_BUF_ISO_IN;
		} else
			break;

		if (buf == NULL)
			break;

		if (data_len > buf->size) {
			net_buf_unref(buf);
			continue;
		}

		memcpy(buf->data, &hdr, hdr_len);

		bt_buf_set_type(buf, type);

		ret = h4_recv_data(buf->data + hdr_len, data_len);
		if (ret != data_len)
			break;

		net_buf_add(buf, hdr_len + data_len);

		bt_recv(buf);
	}

	BT_ASSERT(false);
}

static int h4_open(void)
{
	int ret;

	if (g_fd > 0)
		return OK;

	g_fd = open(CONFIG_BT_UART_ON_DEV_NAME, O_RDWR | O_BINARY);
	if (g_fd < 0)
		return g_fd;

	ret = file_detach(g_fd, &g_filep);
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
	close(g_fd);
	g_fd = -1;

	return ret;
}

static int h4_send(struct net_buf *buf)
{
	uint8_t *type;
	int ret;

	type = net_buf_push(buf, 1);

	switch (bt_buf_get_type(buf)) {
		case BT_BUF_ACL_OUT:
			*type = H4_ACL;
			break;
		case BT_BUF_CMD:
			*type = H4_CMD;
			break;
		case BT_BUF_ISO_OUT:
			*type = H4_ISO;
			break;
		default:
			ret = -EINVAL;
			goto bail;
	}

	ret = h4_send_data(buf->data, buf->len);
	if (ret != buf->len)
		ret = -EINVAL;

bail:
	net_buf_unref(buf);

	return ret < 0 ? ret : 0;
}

static struct bt_hci_driver driver = {
	.name = "H:4",
	.bus  = BT_HCI_DRIVER_BUS_UART,
	.open = h4_open,
	.send = h4_send,
};

int bt_uart_init(void)
{
	return bt_hci_driver_register(&driver);
}
