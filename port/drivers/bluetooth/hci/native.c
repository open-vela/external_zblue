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

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"

#include <logging/log.h>

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04
#define H4_ISO  0x05

extern int bt_vadapter_init(void);
extern int bt_vadapter_send(uint8_t type, const uint8_t *data, uint16_t len);

#if !CONFIG_BT_THREAD_NO_PREEM
static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_NATIVE_THREAD_STACK_SIZE);
static struct k_thread        rx_thread_data;
static K_FIFO_DEFINE(rx_queue);
#endif /* !CONFIG_BT_THREAD_NO_PREEM */

static void h4_data_dump(const char *tag, uint8_t type, uint8_t *data, uint32_t len)
{
	struct iovec bufs[2];

	bufs[0].iov_base = &type;
	bufs[0].iov_len = 1;
	bufs[1].iov_base = data;
	bufs[1].iov_len = len;

	lib_dumpvbuffer(tag, bufs, 2);
}

int bt_vadapter_recv(uint8_t type, uint16_t hdr, const uint8_t *data, uint16_t len)
{
	struct net_buf *buf = NULL;

	switch (type)
	{
	case H4_EVT:
	{
		uint8_t evt = hdr & 0xff;
		bool discardable = false;
		k_timeout_t timeout = K_FOREVER;

		if (evt == BT_HCI_EVT_LE_META_EVENT &&
		    (data[0] == BT_HCI_EVT_LE_ADVERTISING_REPORT ||
		     data[0] == BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT)) {
			discardable = true;
			timeout = K_NO_WAIT;
		}

		buf = bt_buf_get_evt(evt, discardable, timeout);
		if (!buf) {
			return -ENOBUFS;
		}

		net_buf_add_u8(buf, evt);
		net_buf_add_u8(buf, len & 0xff);
		net_buf_add_mem(buf, data, len);

#ifdef CONFIG_BT_H4_DEBUG
		if (!discardable)
		h4_data_dump("BT RX", H4_EVT, (void *)data, len);
#endif

#if CONFIG_BT_THREAD_NO_PREEM
		return bt_recv(buf);
#else /* !CONFIG_BT_THREAD_NO_PREEM */
		net_buf_put(&rx_queue, buf);
		return 0;
#endif /* CONFIG_BT_THREAD_NO_PREEM */
	}
	case H4_ACL:
	{
		buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
		if (!buf) {
			return -ENOBUFS;
		}

		net_buf_add_le16(buf, hdr);
		net_buf_add_le16(buf, len);
		net_buf_add_mem(buf, data, len);

#ifdef CONFIG_BT_H4_DEBUG
		h4_data_dump("BT RX", H4_ACL, (void *)data, len);
#endif

#if CONFIG_BT_THREAD_NO_PREEM
		return bt_recv(buf);
#else /* !CONFIG_BT_THREAD_NO_PREEM */
		net_buf_put(&rx_queue, buf);
		return 0;
#endif /* CONFIG_BT_THREAD_NO_PREEM */
	}
	default:
		LOG_ERR("Unknown packet type: %u", buf[0]);
	}

	return 0;
}

static int native_send(struct net_buf *buf)
{
	int err;
	uint8_t type;

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		type = H4_ACL;
		break;
	case BT_BUF_CMD:
		type = H4_CMD;
		break;
	case BT_BUF_ISO_OUT:
		if (IS_ENABLED(CONFIG_BT_ISO)) {
			type = H4_ISO;
			break;
		}
		__fallthrough;
	default:
		LOG_ERR("Unknown buffer type");
		return -EINVAL;
	}

#ifdef CONFIG_BT_H4_DEBUG
	h4_data_dump("BT TX", type, buf->data, buf->len);
#endif
	err = bt_vadapter_send(type, buf->data, buf->len);
	if (err) {
		LOG_ERR("Unable send vadapter (err:%d)", err);
	}

	net_buf_unref(buf);

	return 0;
}

#if !CONFIG_BT_THREAD_NO_PREEM
static void rx_thread(void *p1, void *p2, void *p3)
{
	struct net_buf *buf;

	LOG_DBG("Started");

	while(1) {
		buf = net_buf_get(&rx_queue, K_FOREVER);

		(void)bt_recv(buf);

		k_yield();
	}
}
#endif /* !CONFIG_BT_THREAD_NO_PREEM */

static int native_open(void)
{
#if !CONFIG_BT_THREAD_NO_PREEM
	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack),
			rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	k_thread_name_set(&rx_thread_data, "BT Driver");
#endif /* !CONFIG_BT_THREAD_NO_PREEM */

	return bt_vadapter_init();
}

static const struct bt_hci_driver drv = {
	.name		= "HCI Native",
	.bus		= BT_HCI_DRIVER_BUS_VIRTUAL,
	.open		= native_open,
	.send		= native_send,
};

static int bt_native_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	bt_hci_driver_register(&drv);

	return 0;
}

SYS_INIT(bt_native_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
