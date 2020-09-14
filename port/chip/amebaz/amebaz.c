/****************************************************************************
 * apps/external/zblue/port/chip/amebaz/amebaz.c
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


#include <nuttx/config.h>
#include <stdio.h>
#include <queue.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "netutils/netlib.h"

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04

#define AMEBAZ_COMMAND_FRAGMENT_SIZE    (252)
#define AMEBAZ_COMMAND_DONE             (0)
#define AMEBAZ_COMMAND_VALID            (1)

#ifdef CONFIG_MIIO_WIRELESS_NAME
#define AMEBAZ_WIRELESS_NAME CONFIG_MIIO_WIRELESS_NAME
#else
#define AMEBAZ_WIRELESS_NAME "wlan0"
#endif

static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
static struct k_thread        rx_thread_data;
static int                    g_fd = -1;
static struct file            g_filep;

static unsigned char rtl_vendor_init_config[] =
{
	0x55, 0xab, 0x23, 0x87,                                 /* header */
	0x32, 0x00,                                             /* Config length: header + len + preload */
	0x30, 0x00, 0x06, 0x99, 0x88, 0x77, 0x44, 0x55, 0x66,   /* BT MAC address */
	0x0c, 0x00, 0x04, 0x04, 0x50, 0xF7, 0x05,               /* Baudrate 921600 */
	0x18, 0x00, 0x01, 0x5c,                                 /* flow control */
	0x94, 0x01, 0x06, 0x0a, 0x08, 0x00, 0x00, 0x2e, 0x07,   /* phy flatk */
	0x9f, 0x01, 0x05, 0x2a, 0x2a, 0x2a, 0x2a, 0x1c,         /* unknow 1 */
	0xA4, 0x01, 0x04, 0xfe, 0xfe, 0xfe, 0xfe,               /* unknow 2 */
};

extern const unsigned char rtl_vendor_command[];
extern unsigned int        rtl_vendor_command_size;

static int data_recv_internal(FAR uint8_t *buf, size_t count, bool wait)
{
	ssize_t ret, nread = 0;

	while (count != nread) {
		ret = file_read(&g_filep, buf + nread, count - nread);
		if (ret < 0) {
			if (ret == -EAGAIN) {
				if (wait) {
					usleep(1 * 1000);
					continue;
				} else
					return nread;
			} else
				return ret;
		}

		nread += ret;
	}

	return nread;
}

static int data_send_internal(FAR uint8_t *buf, size_t count, bool wait)
{
	ssize_t ret, nwritten = 0;

	while (nwritten != count) {
		ret = file_write(&g_filep, buf + nwritten, count - nwritten);
		if (ret < 0) {
			if (ret == -EAGAIN) {
				if (wait) {
					usleep(1 * 1000);
					continue;
				} else
					return nwritten;
			} else
				return ret;
		}

		nwritten += ret;
	}

	return nwritten;
}

static int data_recv(FAR uint8_t *buf, size_t count)
{
	return data_recv_internal(buf, count, true);
}

static int data_recv_nowait(FAR uint8_t *buf, size_t count)
{
	return data_recv_internal(buf, count, false);
}

static int data_send(FAR uint8_t *buf, size_t count)
{
	return data_send_internal(buf, count, true);
}

static int data_send_nowait(FAR uint8_t *buf, size_t count)
{
	return data_send_internal(buf, count, false);
}

static int fetch_command(uint8_t *command)
{
	unsigned int config_size = sizeof(rtl_vendor_init_config);
	static unsigned int command_offset;
	int fragment_size = 0;
	int index;

	if (command_offset >= config_size + rtl_vendor_command_size)
		return AMEBAZ_COMMAND_DONE;

	if (command_offset < rtl_vendor_command_size) {
		if (command_offset + AMEBAZ_COMMAND_FRAGMENT_SIZE > rtl_vendor_command_size)
			fragment_size = rtl_vendor_command_size - command_offset;
		else
			fragment_size = AMEBAZ_COMMAND_FRAGMENT_SIZE;

		memcpy(command + 5, rtl_vendor_command + command_offset, fragment_size);
		command_offset += fragment_size;
	}

	if (command_offset >= rtl_vendor_command_size) {
		int config_offset = command_offset - rtl_vendor_command_size;
		int config_len = config_size - config_offset;

		if (fragment_size < AMEBAZ_COMMAND_FRAGMENT_SIZE) {
			int free = AMEBAZ_COMMAND_FRAGMENT_SIZE - fragment_size;
			int copy_size;

			if (config_len > free)
				copy_size = free;
			else
				copy_size = config_len;

			memcpy(command + 5 + fragment_size,
					rtl_vendor_init_config + config_offset, copy_size);
			command_offset += copy_size;
			fragment_size += copy_size;
		}
	}

	index = (command_offset / AMEBAZ_COMMAND_FRAGMENT_SIZE) - 1;
	if (command_offset % AMEBAZ_COMMAND_FRAGMENT_SIZE > 0)
		index++;

	if (command_offset >= config_size + rtl_vendor_command_size)
		index |= 0x80;

	command[0] = H4_CMD;
	command[1] = 0x20;
	command[2] = 0xFC;
	command[3] = fragment_size + 1;
	command[4] = index;

	return AMEBAZ_COMMAND_VALID;
}

static int amebaz_load_firmware(void)
{
	int header_size = CONFIG_BT_HCI_RESERVE + sizeof(uint16_t) + sizeof(uint8_t);
	uint8_t command[AMEBAZ_COMMAND_FRAGMENT_SIZE + header_size];
	uint8_t mac[IFHWADDRLEN];
	int buffer_size;
	int i, ret;

	netlib_getmacaddr(AMEBAZ_WIRELESS_NAME, mac);

	for (i = 0; i < IFHWADDRLEN; i++)
		rtl_vendor_init_config[9 + i] = mac[(IFHWADDRLEN - 1) - i];
	rtl_vendor_init_config[9] -= 1;

	while (fetch_command(command) != AMEBAZ_COMMAND_DONE) {
		buffer_size = header_size + command[3];
		usleep(10);
		ret = data_send(command, buffer_size);
		if (ret != buffer_size)
			return ret;

		data_recv(command, 1);
		if (H4_EVT == command[0]) {
			data_recv(command + 1, 2);
			data_recv(command + 3, command[2]);
		} else
			return -EIO;
	}

	return OK;
}

static int update_baudrate(uint32_t baudrate)
{
	typedef struct {
		uint32_t    bt_baudrate;
		uint32_t    uart_baudrate;
	} baudrate_map;

	const baudrate_map maps[] = {
		{0x0000701d, B115200},
		{0x0252C00A, B230400},
		{0x05F75004, B921600},
		{0x00005004, B1000000},
		{0x04928002, B1500000},
		{0x00005002, B2000000},
		{0x0000B001, B2500000},
		{0x04928001, B3000000},
	};

	unsigned char command[8];
	struct termios toptions;
	int i;

	for (i = 0; i < sizeof(maps); i++) {
		if (baudrate == maps[i].bt_baudrate)
			break;
	}

	if (i == sizeof(maps))
		return -EINVAL;

	command[0] = H4_CMD;
	command[1] = 0x17;
	command[2] = 0xfc;
	command[3] = sizeof(uint32_t);
	memcpy(&command[4], (uint32_t *)&baudrate, sizeof(uint32_t));

	if (data_send(command, sizeof(command)) != sizeof(command))
		return -EIO;

	data_recv(command, 1);
	if (command[0] != H4_EVT)
		return -EIO;

	data_recv(command + 1, sizeof(struct bt_hci_evt_hdr));
	data_recv(command + 3, command[2]);

	file_ioctl(&g_filep, TCGETS, (unsigned long)&toptions);

	cfsetispeed(&toptions, maps[i].uart_baudrate);
	cfsetospeed(&toptions, maps[i].uart_baudrate);

	return file_ioctl(&g_filep, TCSETS, (unsigned long)&toptions);
}

static int update_efuse_iqk(void)
{
	unsigned char command[16];
	int i;

	for (i = 0; i < 12; i++)
		hal_efuse_read(0x100 + i, command + 4 + i, 0);

	command[0] = H4_CMD;
	command[1] = 0x91;
	command[2] = 0xfd;
	command[3] = 12;

	if (data_send(command, sizeof(command)) != sizeof(command))
		return -EIO;

	data_recv(command, 1);
	if (command[0] != H4_EVT)
		return -EIO;

	data_recv(command + 1, sizeof(struct bt_hci_evt_hdr));
	data_recv(command + 3, command[2]);

	return 0;
}

static void rx_thread(void *p1, void *p2, void *p3)
{
	int hdr_len, data_len, ret;
	struct net_buf *buf;
	uint8_t type;
	union {
		struct bt_hci_evt_hdr evt;
		struct bt_hci_acl_hdr acl;
	} hdr;

	for (;;) {
		ret = data_recv(&type, 1);
		if (ret != 1)
			break;

		if (type != H4_EVT && type != H4_ACL)
			continue;

		hdr_len = (type == H4_EVT) ?
			sizeof(struct bt_hci_evt_hdr) :
			sizeof(struct bt_hci_acl_hdr);

		ret = data_recv((uint8_t *)&hdr, hdr_len);
		if (ret != hdr_len)
			break;

		if (type == H4_EVT) {
			buf = bt_buf_get_evt(hdr.evt.evt, false, K_FOREVER);
			data_len = hdr.evt.len;
		} else {
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
			data_len = hdr.acl.len;
		}

		if (buf == NULL)
			break;

		if (data_len > buf->size) {
			net_buf_unref(buf);
			continue;
		}

		memcpy(buf->data, &hdr, hdr_len);

		bt_buf_set_type(buf, type == H4_EVT ? BT_BUF_EVT : BT_BUF_ACL_IN);

		ret = data_recv(buf->data + hdr_len, data_len);
		if (ret != data_len)
			break;

		net_buf_add(buf, hdr_len + data_len);

		bt_recv(buf);
	}

	printf("INVALID BUF FORMAT\n");
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
	if (ret < 0) {
		close(g_fd);
		g_fd = -1;
		return ret;
	}

	if (update_baudrate(*(uint32_t *)&rtl_vendor_init_config[18]) < 0)
		goto bail;

	ret = amebaz_load_firmware();
	if (ret < 0)
		goto bail;

	ret = update_efuse_iqk();
	if (ret < 0)
		goto bail;

	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack),
			rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	k_thread_name_set(&rx_thread_data, "BT Driver");

	return 0;

bail:
	file_close(&g_filep);
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
		default:
			ret = -EINVAL;
			goto bail;
	}

	ret = data_send(buf->data, buf->len);
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

int amebaz_bt_stack_initialize(void)
{
	bt_hci_driver_register(&driver);

	bt_enable(NULL);

	return 0;
}
