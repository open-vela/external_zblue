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
#include "../arch/sim/src/sim/up_hcisocket_host.h"

#include <logging/log.h>

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04
#define H4_ISO  0x05

static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
static struct k_thread        rx_thread_data;

static int uc_fd = -1;

static struct net_buf *get_rx(const uint8_t *buf)
{
	bool discardable = false;
	k_timeout_t timeout = K_FOREVER;

	switch (buf[0]) {
	case H4_EVT:
		if (buf[1] == BT_HCI_EVT_LE_META_EVENT &&
		    (buf[3] == BT_HCI_EVT_LE_ADVERTISING_REPORT)) {
			discardable = true;
			timeout = K_NO_WAIT;
		}

		return bt_buf_get_evt(buf[1], discardable, timeout);
	case H4_ACL:
		return bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
	case H4_ISO:
		if (IS_ENABLED(CONFIG_BT_ISO)) {
			return bt_buf_get_rx(BT_BUF_ISO_IN, K_FOREVER);
		}
		__fallthrough;
	default:
		LOG_ERR("Unknown packet type: %u", buf[0]);
	}

	return NULL;
}

static bool uc_ready(void)
{
	return bthcisock_host_avail(uc_fd);
}

static void rx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_DBG("started");

	while (1) {
		static uint8_t frame[512];
		struct net_buf *buf;
		size_t buf_tailroom;
		size_t buf_add_len;
		ssize_t len;

		if (!uc_ready()) {
			k_sleep(K_MSEC(1));
			continue;
		}

		LOG_DBG("calling read()");

		len = bthcisock_host_read(uc_fd, frame, sizeof(frame));
		if (len < 0) {
			if (errno == EINTR) {
				k_yield();
				continue;
			}

			LOG_ERR("Reading socket failed, errno %d", errno);
			bthcisock_host_close(uc_fd);
			uc_fd = -1;
			return;
		}

		buf = get_rx(frame);
		if (!buf) {
			LOG_DBG("Discard adv report due to insufficient buf");
			continue;
		}

		buf_tailroom = net_buf_tailroom(buf);
		buf_add_len = len - 1;
		if (buf_tailroom < buf_add_len) {
			LOG_ERR("Not enough space in buffer %zu/%zu",
			       buf_add_len, buf_tailroom);
			net_buf_unref(buf);
			continue;
		}

		net_buf_add_mem(buf, &frame[1], buf_add_len);

		LOG_DBG("Calling bt_recv(%p)", buf);

		bt_recv(buf);

		k_yield();
	}
}

static int uc_send(struct net_buf *buf)
{
	LOG_DBG("buf %p type %u len %u", buf, bt_buf_get_type(buf), buf->len);

	if (uc_fd < 0) {
		LOG_ERR("User channel not open");
		return -EIO;
	}

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		net_buf_push_u8(buf, H4_ACL);
		break;
	case BT_BUF_CMD:
		net_buf_push_u8(buf, H4_CMD);
		break;
	case BT_BUF_ISO_OUT:
		if (IS_ENABLED(CONFIG_BT_ISO)) {
			net_buf_push_u8(buf, H4_ISO);
			break;
		}
		__fallthrough;
	default:
		LOG_ERR("Unknown buffer type");
		return -EINVAL;
	}

	if (bthcisock_host_send(uc_fd, buf->data, buf->len) < 0) {
		return -errno;
	}

	net_buf_unref(buf);
	return 0;
}

static int uc_open(void)
{
	uc_fd = bthcisock_host_open(CONFIG_BT_HCI_DEVID);
	if (uc_fd < 0) {
		return uc_fd;
	}

	LOG_DBG("User Channel opened as fd %d", uc_fd);

	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_KERNEL_STACK_SIZEOF(rx_thread_stack),
			rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO) + 1,
			0, K_NO_WAIT);

	k_thread_name_set(&rx_thread_data, "BT Userchan");

	return 0;
}

static const struct bt_hci_driver drv = {
	.name		= "HCI User Channel",
	.bus		= BT_HCI_DRIVER_BUS_UART,
	.open		= uc_open,
	.send		= uc_send,
};

static int bt_userchan_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	bt_hci_driver_register(&drv);

	return 0;
}

SYS_INIT(bt_userchan_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
