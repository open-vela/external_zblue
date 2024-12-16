/******************************************************************************
 *
 * Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <debug.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/bluetooth.h>

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_driver);

#define DT_DRV_COMPAT zephyr_bt_hci_ttyHCI

struct h4_data {
	int fd;
	pthread_mutex_t mutex;
	bt_hci_recv_t recv;
};

#define HCI_DEBUG 0

static K_KERNEL_STACK_DEFINE(rx_thread_stack, 2048);
static struct k_thread rx_thread_data;

static void h4_data_dump(const char *tag, uint8_t type, uint8_t *data, uint32_t len)
{
#if HCI_DEBUG
	struct iovec bufs[2];

	bufs[0].iov_base = &type;
	bufs[0].iov_len = 1;
	bufs[1].iov_base = data;
	bufs[1].iov_len = len;

	lib_dumpvbuffer(tag, bufs, 2);
#endif
}

static int h4_send_data(struct h4_data *h4, uint8_t *buf, size_t count)
{
	ssize_t ret, nwritten = 0;

	while (nwritten != count) {
		ret = write(h4->fd, buf + nwritten, count - nwritten);
		if (ret < 0) {
			if (ret == -EAGAIN) {
				usleep(500);
				continue;
			} else {
				return ret;
			}
		}

		nwritten += ret;
	}

	return nwritten;
}

static struct net_buf *get_rx(const uint8_t *buf)
{
	bool discardable = false;
	k_timeout_t timeout = K_FOREVER;

	switch (buf[0]) {
	case BT_HCI_H4_EVT:
		if (buf[1] == BT_HCI_EVT_LE_META_EVENT &&
		    (buf[3] == BT_HCI_EVT_LE_ADVERTISING_REPORT)) {
			discardable = true;
			timeout = K_NO_WAIT;
		}

		return bt_buf_get_evt(buf[1], discardable, timeout);
	case BT_HCI_H4_ACL:
		return bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
	case BT_HCI_H4_ISO:
		if (IS_ENABLED(CONFIG_BT_ISO)) {
			return bt_buf_get_rx(BT_BUF_ISO_IN, K_FOREVER);
		}
		__fallthrough;
	default:
		LOG_ERR("Unknown packet type: %u", buf[0]);
	}

	return NULL;
}

/**
 * @brief Decode the length of an HCI H4 packet and check it's complete
 * @details Decodes packet length according to Bluetooth spec v5.4 Vol 4 Part E
 * @param buf	Pointer to a HCI packet buffer
 * @param buf_len	Bytes available in the buffer
 * @return Length of the complete HCI packet in bytes, -1 if cannot find an HCI
 *         packet, 0 if more data required.
 */
static int32_t hci_packet_complete(const uint8_t *buf, uint16_t buf_len)
{
	uint16_t payload_len = 0;
	const uint8_t type = buf[0];
	uint8_t header_len = sizeof(type);
	const uint8_t *hdr = &buf[sizeof(type)];

	switch (type) {
	case BT_HCI_H4_CMD: {
		if (buf_len < header_len + BT_HCI_CMD_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_cmd_hdr *cmd = (const struct bt_hci_cmd_hdr *)hdr;

		/* Parameter Total Length */
		payload_len = cmd->param_len;
		header_len += BT_HCI_CMD_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_ACL: {
		if (buf_len < header_len + BT_HCI_ACL_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_acl_hdr *acl = (const struct bt_hci_acl_hdr *)hdr;

		/* Data Total Length */
		payload_len = sys_le16_to_cpu(acl->len);
		header_len += BT_HCI_ACL_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_SCO: {
		if (buf_len < header_len + BT_HCI_SCO_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_sco_hdr *sco = (const struct bt_hci_sco_hdr *)hdr;

		/* Data_Total_Length */
		payload_len = sco->len;
		header_len += BT_HCI_SCO_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_EVT: {
		if (buf_len < header_len + BT_HCI_EVT_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_evt_hdr *evt = (const struct bt_hci_evt_hdr *)hdr;

		/* Parameter Total Length */
		payload_len = evt->len;
		header_len += BT_HCI_EVT_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_ISO: {
		if (buf_len < header_len + BT_HCI_ISO_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_iso_hdr *iso = (const struct bt_hci_iso_hdr *)hdr;

		/* ISO_Data_Load_Length parameter */
		payload_len = bt_iso_hdr_len(sys_le16_to_cpu(iso->len));
		header_len += BT_HCI_ISO_HDR_SIZE;
		break;
	}
	/* If no valid packet type found */
	default:
		LOG_WRN("Unknown packet type 0x%02x", type);
		return -1;
	}

	/* Request more data */
	if (buf_len < header_len + payload_len) {
		return 0;
	}

	return (int32_t)header_len + payload_len;
}

#if 1
static bool h4_ready(struct h4_data *h4)
{
	struct pollfd pollfd = { .fd = h4->fd, .events = POLLIN };

	return (poll(&pollfd, 1, 0) == 1);
}
#endif

static void h4_rx_thread(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	struct h4_data *h4 = dev->data;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_DBG("started");

	ssize_t frame_size = 0;

	while (1) {
		static uint8_t frame[512];
		struct net_buf *buf;
		size_t buf_tailroom;
		size_t buf_add_len;
		ssize_t len;
		const uint8_t *frame_start = frame;

#if 1
		if (!h4_ready(h4)) {
			usleep(1000);
			continue;
		}
#endif

		LOG_DBG("calling read()");

		len = read(h4->fd, frame + frame_size, sizeof(frame) - frame_size);
		if (len < 0) {
			if (errno == EINTR) {
				continue;
			}

			if (len == -EAGAIN) {
				usleep(500);
				continue;
			}

			LOG_ERR("Reading hci failed, errno %d", errno);
			close(h4->fd);
			h4->fd = -1;
			return;
		}

		frame_size += len;

		while (frame_size > 0) {
			const uint8_t *buf_add;
			const uint8_t packet_type = frame_start[0];
			const int32_t decoded_len = hci_packet_complete(frame_start, frame_size);

			if (decoded_len == -1) {
				LOG_ERR("HCI Packet type is invalid, length could not be decoded");
				frame_size = 0; /* Drop buffer */
				break;
			}

			if (decoded_len == 0) {
				if (frame_size == sizeof(frame)) {
					LOG_ERR("HCI Packet (%d bytes) is too big for frame (%d "
						"bytes)",
						decoded_len, sizeof(frame));
					frame_size = 0; /* Drop buffer */
					break;
				}
				if (frame_start != frame) {
					memmove(frame, frame_start, frame_size);
				}
				/* Read more */
				break;
			}

			buf_add = frame_start + sizeof(packet_type);
			buf_add_len = decoded_len - sizeof(packet_type);

			buf = get_rx(frame_start);

			frame_size -= decoded_len;
			frame_start += decoded_len;

			if (!buf) {
				LOG_DBG("Discard adv report due to insufficient buf");
				continue;
			}

			buf_tailroom = net_buf_tailroom(buf);
			if (buf_tailroom < buf_add_len) {
				LOG_ERR("Not enough space in buffer %zu/%zu", buf_add_len,
					buf_tailroom);
				net_buf_unref(buf);
				continue;
			}

			net_buf_add_mem(buf, buf_add, buf_add_len);

			LOG_DBG("Calling bt_recv(%p)", buf);

			h4_data_dump("BT RX", packet_type, buf->data, buf_add_len);
			h4->recv(dev, buf);
		}
	}
}

static int h4_send(const struct device *dev, struct net_buf *buf)
{
	struct h4_data *h4 = dev->data;
	int len;
	int ret;

	LOG_DBG("buf %p type %u len %u", buf, bt_buf_get_type(buf), buf->len);

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		net_buf_push_u8(buf, BT_HCI_H4_ACL);
		break;
	case BT_BUF_CMD:
		net_buf_push_u8(buf, BT_HCI_H4_CMD);
		break;
	case BT_BUF_ISO_OUT:
		if (IS_ENABLED(CONFIG_BT_ISO)) {
			net_buf_push_u8(buf, BT_HCI_H4_ISO);
			break;
		}
		__fallthrough;
	default:
		LOG_ERR("Unknown buffer type");
		return -EINVAL;
	}

	h4_data_dump("BT TX", buf->data[0], buf->data + 1, buf->len - 1);

	pthread_mutex_lock(&h4->mutex);
	len = buf->len;
	ret = h4_send_data(h4, buf->data, buf->len);
	if (ret != len) {
		ret = -EINVAL;
	}

	net_buf_unref(buf);
	pthread_mutex_unlock(&h4->mutex);

	return ret < 0 ? ret : 0;
}

static int h4_open(const struct device *dev, bt_hci_recv_t recv)
{
	int ret;
	struct h4_data *h4 = dev->data;

	ret = open(CONFIG_BT_UART_ON_DEV_NAME, O_RDWR | O_BINARY | O_CLOEXEC);
	if (ret < 0) {
		goto bail;
	}

	h4->fd = ret;
	LOG_DBG("H4: %s opened as fd %d", CONFIG_BT_UART_ON_DEV_NAME, h4->fd);

	ret = (int)k_thread_create(&rx_thread_data, rx_thread_stack,
				   K_THREAD_STACK_SIZEOF(rx_thread_stack), h4_rx_thread, (void *)dev, NULL,
				   NULL, K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	if (ret < 0) {
		goto bail;
	} else {
		ret = 0;
	}

	h4->recv = recv;

	k_thread_name_set(&rx_thread_data, "BT Driver");
	LOG_DBG("returning");

	return 0;

bail:
	close(h4->fd);
	h4->fd = -1;

	return ret;
}

static const struct bt_hci_driver_api h4_drv_api = {
	.open = h4_open,
	.send = h4_send,
};

static int h4_init(const struct device *dev)
{
	LOG_INF("Bluetooth H4 driver");
	return 0;
}

#define H4_DEVICE_INIT(inst)                                                                       \
	static struct h4_data h4_data_##inst = {                                                   \
		.fd = -1,                                                                             \
		.mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,                                   \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, h4_init, NULL, &h4_data_##inst, NULL, POST_KERNEL,             \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &h4_drv_api)

H4_DEVICE_INIT(0);