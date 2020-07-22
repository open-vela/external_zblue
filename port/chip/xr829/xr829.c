/****************************************************************************
 * apps/external/zblue/port/chip/xr829/xr829.c
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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <drivers/hal_gpio.h>
#include "xr829_bt.h"

#undef ARRAY_SIZE

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"

#define XR829_FIRMWARE              "/data/fw_xr829_bt.bin"
#define XR829_DEFAULT_BAUDRATE      (115200)
#define XR829_ONCHIP_BAUDRATE       (1500000)

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04

static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
static struct k_thread        rx_thread_data;
static struct file            g_filep;
static int                    g_fd = -1;

static unsigned short check_sum16(void *data, unsigned int len)
{
	unsigned short *p = data;
	unsigned short cs = 0;

	while (len > 1) {
		cs += *p++;
		len -= 2;
	}

	if (len)
		cs += *(unsigned char *)p;

	return cs;
}

static void bt_set_rfkill_state(bool on)
{
	if (on)
		hal_gpio_set_output(HAL_GPIO_132, HAL_GPIO_DATA_HIGH);
	else
		hal_gpio_set_output(HAL_GPIO_132, HAL_GPIO_DATA_LOW);
}

static void bt_rfkill_state_reset(void)
{
	bt_set_rfkill_state(false);
	usleep(10 * 1000);
	bt_set_rfkill_state(true);
}

static void bt_rf_probe(void)
{
	hal_gpio_set_pull_state(HAL_GPIO_132,HAL_GPIO_PULL_UP);
	hal_gpio_set_direction(HAL_GPIO_132, HAL_GPIO_DIRECTION_OUTPUT);
	usleep(1 * 1000);
	hal_gpio_set_output(HAL_GPIO_132, HAL_GPIO_DATA_LOW);

	hal_gpio_set_pull_state(HAL_GPIO_131,HAL_GPIO_PULL_UP);
	hal_gpio_set_direction(HAL_GPIO_131, HAL_GPIO_DIRECTION_OUTPUT);
	hal_gpio_set_output(HAL_GPIO_131, HAL_GPIO_DATA_HIGH);
}

static void bt_rf_reset(void)
{
	bt_rf_probe();
	bt_rfkill_state_reset();
}

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

static int hci_serial_set_baudrate(uint32_t baudrate)
{
	uint32_t uart_baudrate = B115200;
	struct termios toptions;

	file_ioctl(&g_filep, TCGETS, (unsigned long)&toptions);

	switch(baudrate) {
		case 9600:
			uart_baudrate = B9600;
			break;
		case 115200:
			uart_baudrate = B115200;
			break;
		case 1500000:
			uart_baudrate = B1500000;
			break;
		default:
			break;
	}

	cfsetispeed(&toptions, uart_baudrate);
	cfsetospeed(&toptions, uart_baudrate);

	return file_ioctl(&g_filep, TCSETS, (unsigned long)&toptions);
}

static int hci_serial_set_flowctrl(bool flowctrl)
{
	struct termios toptions;

	file_ioctl(&g_filep, TCGETS, (unsigned long)&toptions);

	if (flowctrl)
		toptions.c_cflag |= CRTSCTS;
	else
		toptions.c_cflag &= ~CRTSCTS;

	return file_ioctl(&g_filep, TCSETS, (unsigned long)&toptions);
}

static int make_nonblock(bool nonblock)
{
	int flags;

	flags = file_ioctl(&g_filep, F_GETFL, 0);

	if (nonblock)
		return file_ioctl(&g_filep, F_SETFL, flags | O_NONBLOCK);

	return file_ioctl(&g_filep, F_SETFL, flags & (~O_NONBLOCK));
}

static int brom_cmd_send_syncword(void)
{
	uint8_t syncword[3] = { CMD_SYNC_WORD };
	int ret;

	make_nonblock(true);

	do {
		ret = data_send(syncword, 1);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			continue;

		usleep(10 * 1000);

		ret = data_recv_nowait(syncword, 2);
		if (ret == 2 && ((syncword[0] == 'O') && (syncword[1] == 'K')))
			break;
	} while (1);

	make_nonblock(false);

	return ret == 2 ? 0 : ret;
}

static int brom_cmd_recv_ack(void)
{
	cmd_ack_t ack = {};
	int ret;

	ret = data_recv((uint8_t *)&ack, sizeof(ack) - 1);
	if (ret != sizeof(ack) - 1)
		return ret;

	/* check header */
	if (!HEADER_MAGIC_VALID(&ack.h))
		return -1;

	if (ack.h.flags & CMD_HFLAG_ERROR)
		return -ack.err;

	if ((ack.h.flags & CMD_HFLAG_ACK) == 0)
		return -1;

	/* convert network byte order to host byte order */
	ack.h.payload_len = SWAP32(ack.h.payload_len);
	ack.h.checksum    = SWAP16(ack.h.checksum);
	if (ack.h.payload_len != 0)
		return -1;

	if ((ack.h.flags & CMD_HFLAG_CHECK) &&
			(check_sum16(&ack, MB_CMD_HEADER_SIZE)) != 0xffff)
		return -1;

	return 0;
}

static int brom_cmd_send_baudrate(int speed)
{
	int payload_len = sizeof(cmd_sys_setuart_t) - sizeof(cmd_header_t);
	int lcr = speed | (3 << 24);
	cmd_sys_setuart_t cmd = {};
	int ret;

	/* fill header */
	FILL_HEADER_MAGIC(&cmd.h);
	cmd.h.flags = CMD_HFLAG_CHECK;
	cmd.h.payload_len = payload_len;

	/* fill command id */
	cmd.cmdid = CMD_ID_SETUART;
	cmd.lcr = lcr;
	cmd.h.checksum = ~check_sum16(&cmd, MB_CMD_HEADER_SIZE + payload_len);

	/* convert host byte order to network byte order */
	cmd.h.payload_len = SWAP32(cmd.h.payload_len);
	cmd.h.checksum    = SWAP16(cmd.h.checksum);
	cmd.lcr           = SWAP32(cmd.lcr);

	ret = data_send((uint8_t *)&cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return -EBADF;

	ret = brom_cmd_recv_ack();
	if (ret < 0)
		return ret;

	hci_serial_set_baudrate(speed);

	return brom_cmd_send_syncword();
}

static int brom_cmd_send_stream(void *addr, int len, void *data)
{
	int payload_len = sizeof(cmd_seq_wr_t) - sizeof(cmd_header_t);
	cmd_seq_wr_t cmd = {};
	int ret;

	/* fill header */
	FILL_HEADER_MAGIC(&cmd.h);
	cmd.h.flags = CMD_HFLAG_CHECK;
	cmd.h.payload_len = payload_len;

	/* fill command id */
	cmd.cmdid = CMD_ID_SEQWR;
	cmd.addr = (intptr_t)addr;
	cmd.dlen = len;
	cmd.dcs = ~check_sum16(data, len);
	cmd.h.checksum = ~check_sum16(&cmd, MB_CMD_HEADER_SIZE + payload_len);

	/* convert host byte order to network byte order */
	cmd.h.payload_len = SWAP32(cmd.h.payload_len);
	cmd.addr          = SWAP32(cmd.addr);
	cmd.dlen          = SWAP32(cmd.dlen);
	cmd.dcs           = SWAP16(cmd.dcs);
	cmd.h.checksum    = SWAP16(cmd.h.checksum);

	ret = data_send((uint8_t *)&cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return -EBADF;

	ret = brom_cmd_recv_ack();
	if (ret < 0)
		return ret;

	ret = data_send(data, len);
	if (ret != len)
		return -EBADF;

	return brom_cmd_recv_ack();
}

static int brom_set_pc(unsigned int pc)
{
	int payload_len = sizeof(cmd_sys_t) - sizeof(cmd_header_t);
	cmd_sys_t cmd = {};
	int ret;

	/* fill header */
	FILL_HEADER_MAGIC(&cmd.h);
	cmd.h.flags = CMD_HFLAG_CHECK;
	cmd.h.payload_len = payload_len;

	/* fill command id */
	cmd.cmdid = CMD_ID_SETPC;
	cmd.val = pc;
	cmd.h.checksum = ~check_sum16(&cmd, MB_CMD_HEADER_SIZE + payload_len);

	/* convert host byte order to network byte order */
	cmd.h.payload_len = SWAP32(cmd.h.payload_len);
	cmd.h.checksum    = SWAP16(cmd.h.checksum);
	cmd.val           = SWAP32(cmd.val);

	ret = data_send((uint8_t *)&cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return -EBADF;

	return brom_cmd_recv_ack();
}

static int brom_load_firmware(void)
{
	void *addr = (intptr_t)BT_LOAD_ADDR;
	size_t remain, req;
	struct stat buf;
	int fd, ret;
	void *data;

	fd = open(XR829_FIRMWARE, O_RDONLY);
	if (fd < 0)
		return fd;

	fstat(fd, &buf);

	remain = buf.st_size;

	if (remain <= 0)
		goto bail;

	data = malloc(SZ_1K);
	if (data == NULL) {
		ret = -ENOMEM;
		goto bail;
	}

	while (remain > 0) {
		req = remain > SZ_1K ? SZ_1K : remain;
		ret = read(fd, data, req);
		if (ret != req) {
			ret = -EIO;
			goto bail;
		}

		ret = brom_cmd_send_stream(addr, req, data);
		if (ret < 0)
			goto bail;

		addr += req;
		remain -= req;
	}

	brom_set_pc((intptr_t)BT_LOAD_ADDR);

bail:
	free(data);
	close(fd);

	return ret;
}

static int brom_hci_final_reset(void)
{
	uint8_t hci_reset[4] = "\x01\x03\x0c\x00";
	uint8_t hci_update_baud_rate[8] = "\x01\x18\xfc\x04\x60\xE3\x16\x00";
	uint8_t hci_write_bd_addr[13] = "\x01\x0a\xfc\x09\x02\x00\x06\xAE\xC9\x69\x75\x22\x22";
	uint8_t ack[7];
	int ret;

	ret = data_send(hci_reset, sizeof(hci_reset));
	if (ret < 0)
		return ret;

	ret = data_recv(ack, sizeof(ack));
	if (ret < 0)
		return ret;

	ret = data_send(hci_update_baud_rate, sizeof(hci_update_baud_rate));
	if (ret < 0)
		return ret;

	ret = data_recv(ack, sizeof(ack));
	if (ret < 0)
		return ret;

	hci_serial_set_baudrate(XR829_ONCHIP_BAUDRATE);
	usleep(50 * 1000);

	ret = data_send(hci_write_bd_addr, sizeof(hci_write_bd_addr));
	if (ret < 0)
		return ret;

	ret = data_recv(ack, sizeof(ack));
	if (ret < 0)
		return ret;

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

	bt_rf_reset();

	ret = brom_cmd_send_syncword();
	if (ret < 0)
		goto bail;

	ret = brom_cmd_send_baudrate(XR829_ONCHIP_BAUDRATE);
	if (ret < 0)
		goto bail;

	ret = brom_load_firmware();
	if (ret < 0)
		goto bail;

	hci_serial_set_baudrate(XR829_DEFAULT_BAUDRATE);
	hci_serial_set_flowctrl(true);

	brom_hci_final_reset();

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

int xr829_bt_stack_initialize(void)
{
	bt_hci_driver_register(&driver);

	return bt_enable(NULL);
}
