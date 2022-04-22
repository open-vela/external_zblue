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

#define LOG_MODULE_NAME bt_h4
#include "common/log.h"

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04
#define H4_ISO  0x05

//#define HCI_DEBUG

static K_THREAD_STACK_DEFINE(tx_thread_stack, CONFIG_BT_UART_H4_TX_STACK_SIZE);
static struct k_thread        tx_thread_data;
static K_THREAD_STACK_DEFINE(thread_stack, CONFIG_BT_UART_H4_TX_STACK_SIZE);
static struct k_thread        thread_data;
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

static bool valid_type(uint8_t type)
{
	return (type == H4_CMD) | (type == H4_ACL) | (type == H4_ISO);
}

/* Function expects that type is validated and only CMD, ISO or ACL will be used. */
static uint32_t get_len(const uint8_t *hdr_buf, uint8_t type)
{
	switch (type) {
	case H4_CMD:
		return ((const struct bt_hci_cmd_hdr *)hdr_buf)->param_len;
	case H4_ISO:
		return bt_iso_hdr_len(
			((const struct bt_hci_iso_hdr *)hdr_buf)->len);
	case H4_ACL:
		return ((const struct bt_hci_acl_hdr *)hdr_buf)->len;
	default:
		LOG_ERR("Invalid type: %u", type);
		return 0;
	}
}

static int hdrlen(uint8_t type)
{
	switch (type) {
	case H4_CMD:
		return sizeof(struct bt_hci_cmd_hdr);
	case H4_ISO:
		return sizeof(struct bt_hci_iso_hdr);
	case H4_ACL:
		return sizeof(struct bt_hci_acl_hdr);
	default:
		LOG_ERR("Invalid type: %u", type);
		return 0;
	}
}

static void h4_tx_thread(void *p1, void *p2, void *p3)
{
	unsigned char buffer[1];
	int hdr_len, data_len, ret;
	struct net_buf *buf;
	bool discardable;
	uint8_t type;
	uint8_t event;
	union {
		struct bt_hci_cmd_hdr cmd;
		struct bt_hci_acl_hdr acl;
		struct bt_hci_iso_hdr iso;
	} hdr;

	for (;;) {
		ret = h4_recv_data(&type, 1);
		if (ret != 1) {
			LOG_ERR("Receiving type failed (err %d)", ret);
			break;
		}

		if (!valid_type(type)) {
			LOG_ERR("Invalid type received (type 0x%02x)", type);
			break;
		}

		hdr_len = hdrlen(type);

		ret = h4_recv_data((uint8_t *)&hdr, hdr_len);
		if (ret != hdr_len) {
			LOG_ERR("Receiving hdr failed (err %d)", ret);
			break;
		}

		data_len = get_len(&hdr, type);

		buf = bt_buf_get_tx(BT_BUF_H4, K_NO_WAIT,
				    &type, sizeof(type));
		if (!buf) {
			LOG_ERR("No available command buffers!");
			break;
		}

		net_buf_add_mem(buf, &hdr, hdr_len);

		ret = h4_recv_data(buf->data + hdr_len, data_len);
		if (ret != data_len) {
			LOG_ERR("Receiving payload failed (err %d)", ret);
			break;
		}

		net_buf_add(buf, data_len);

#ifdef CONFIG_BT_H4_DEBUG
		h4_data_dump("BT H4 RX", type, buf->data, hdr_len + data_len);
#endif
		ret = bt_send(buf);
		if (ret) {
			LOG_ERR("Unable to send (err %d)", ret);
			break;
		}

		k_yield();
	}

	BT_ASSERT(false);
}

static int h4_open(void)
{
	int ret;

	ret = file_open(&g_filep, CONFIG_BT_UART_H4_ON_DEV_NAME, O_RDWR | O_BINARY);
	if (ret < 0) {
		return ret;
	}

	ret = (int)k_thread_create(&tx_thread_data, tx_thread_stack,
			K_THREAD_STACK_SIZEOF(tx_thread_stack),
			h4_tx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	if (ret < 0) {
		file_close(&g_filep);
		return ret;
	}

	k_thread_name_set(&tx_thread_data, "BT H4 TX");

	return 0;
}

static int h4_send(struct net_buf *buf)
{
	uint8_t type;
	int ret;

#ifdef CONFIG_BT_H4_DEBUG
if (buf->data[1] != 0x3e)
	h4_data_dump("BT H4 TX", buf->data[0], buf->data + 1, buf->len - 1);
#endif

	ret = h4_send_data(buf->data, buf->len);
	if (ret != buf->len) {
		ret = -EINVAL;
	}

	net_buf_unref(buf);

	return ret < 0 ? ret : 0;
}

static void rx_thread(void *p1, void *p2, void *p3)
{
	static K_FIFO_DEFINE(rx_queue);
	int err;

	/* Enable the raw interface, this will in turn open the HCI driver */
	bt_enable_raw(&rx_queue);
	
	err = h4_open();
	if (err) {
		LOG_ERR("bt tx open failed (err %d)", err);
		return err;
    	}

	while(true) {
		struct net_buf *buf;

		buf = net_buf_get(&rx_queue, K_FOREVER);

		err = h4_send(buf);
		__ASSERT_NO_MSG(err == 0);

		k_yield();
	}

    	return 0;
}

int main(int argc, char *argv[])
{
	k_thread_create(&thread_data, thread_stack,
			K_THREAD_STACK_SIZEOF(thread_stack),
			rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	k_thread_name_set(&thread_data, "BT Thread");

	return 0;
}