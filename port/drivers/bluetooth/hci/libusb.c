/****************************************************************************
 * apps/external/zblue/port/drivers/bluetooth/hci/libusb.c
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
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <libusb-1.0/libusb.h>

#undef ARRAY_SIZE

#include "bluetooth/bluetooth.h"
#include "drivers/bluetooth/hci_driver.h"
#include "common/log.h"

#define H4_NONE              0x00
#define H4_CMD               0x01
#define H4_ACL               0x02
#define H4_SCO               0x03
#define H4_EVT               0x04

#define HCI_BUFSIZE          800

//#define HCI_DEBUG

static K_THREAD_STACK_DEFINE(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
static struct k_thread        rx_thread_data;

enum {
	TRANSFER_INPUT_CMD,
	TRANSFER_INPUT_ACL,
	TRANSFER_OUTPUT_CMD,
	TRANSFER_OUTPUT_ACL,
};

struct usb_handler {
	struct libusb_transfer *transfer;
	int                     address;
	sem_t                   sem;
	int                     type;
};
static struct usb_handler      g_uhandle[4];
static libusb_device_handle   *g_handle;
static struct libusb_transfer *g_command_transfer;
static struct libusb_transfer *g_acl_transfer;

static uint8_t hci_command_input_buffer[HCI_BUFSIZE];
static uint8_t hci_acl_input_buffer[HCI_BUFSIZE];
static uint8_t hci_command_send_buffer[3 + 256 + LIBUSB_CONTROL_SETUP_SIZE];

static void usb_data_dump(const char *tag, uint8_t *data, uint32_t len)
{
#ifdef HCI_DEBUG
	uint8_t *end = data + len;

	printf("%s[%03d]: ", tag, len);
	while (data != end)
		printf("%02x,", *data++);
	printf("\n");
#endif
}

static void usb_callback(struct libusb_transfer *transfer)
{
	struct usb_handler *uhandle = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		if (transfer->endpoint == 0 ||
				uhandle->type == TRANSFER_OUTPUT_ACL)
			sem_post(&uhandle->sem);
		else if (uhandle->type == TRANSFER_INPUT_CMD) {
			if (g_command_transfer == NULL)
				g_command_transfer = transfer;
		} else if (uhandle->type == TRANSFER_INPUT_ACL) {
			if (g_acl_transfer == NULL)
				g_acl_transfer = transfer;
		}
	} else if (transfer->status == LIBUSB_TRANSFER_STALL) {
		libusb_clear_halt(transfer->dev_handle, transfer->endpoint);
		libusb_submit_transfer(transfer);
	}

}

static int usb_open(libusb_device_handle **handle)
{
	struct libusb_device_descriptor desc;
	libusb_device_handle *_handle = NULL;
	libusb_device **devs;
	ssize_t num_devices;
	int ret, i;

	libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_WARNING);

	ret = libusb_get_device_list(NULL, &devs);
	if (ret < 0)
		return ret;

	num_devices = ret;

	for (i = 0; i < num_devices; i++) {
		ret = libusb_get_device_descriptor(devs[i], &desc);
		if (ret < 0)
			goto bail;

		if (desc.bDeviceClass    == 0xE0 &&
				desc.bDeviceSubClass == 0x01 &&
				desc.bDeviceProtocol == 0x01) {
			BT_INFO("%04x:%04x (bus %d, device %d) - class %x subclass %x protocol %x",
					desc.idVendor, desc.idProduct,
					libusb_get_bus_number(devs[i]), libusb_get_device_address(devs[i]),
					desc.bDeviceClass, desc.bDeviceSubClass, desc.bDeviceProtocol);
			break;
		}
	}


	if (i == num_devices) {
		ret = -EINVAL;
		goto bail;
	}

	ret = libusb_open(devs[i], &_handle);
	if (ret < 0)
		goto bail;

	ret = libusb_reset_device(_handle);

bail:
	if (ret < 0 && _handle)
		libusb_close(_handle);
	else
		*handle = _handle;

	libusb_free_device_list(devs, 1);

	return ret;
}

static int usb_prepare(libusb_device_handle *handle)
{
	const int configuration = 1;
	libusb_device *dev;
	int ret;

	dev = libusb_get_device(handle);
	if (dev == NULL)
		return -EINVAL;

	ret = libusb_kernel_driver_active(handle, 0);
	if (ret < 0)
		return ret;

	if (ret == 1) {
		ret = libusb_detach_kernel_driver(handle, 0);
		if (ret < 0)
			return ret;
	}

	ret = libusb_set_configuration(handle, configuration);
	if (ret < 0)
		goto bail;

	ret = libusb_claim_interface(handle, 0);
	if (ret < 0)
		goto bail;

bail:
	if (ret < 0)
		libusb_attach_kernel_driver(handle, 0);

	return ret;
}

static int usb_scan(libusb_device_handle *handle)
{
	const struct libusb_interface_descriptor *idescriptor;
	const struct libusb_endpoint_descriptor *endpoint;
	const struct libusb_interface *interface;
	struct libusb_config_descriptor *descriptor;
	int num_interfaces;
	int cmd_in_address;
	int acl_in_address;
	int acl_out_address;
	int sco_in_address;
	int sco_out_address;
	int i, j, ret;

	ret = libusb_get_active_config_descriptor(
			libusb_get_device(handle), &descriptor);
	if (ret < 0)
		return ret;

	num_interfaces = descriptor->bNumInterfaces;
	cmd_in_address = acl_in_address =
		acl_out_address = sco_in_address =
		sco_out_address = 0;

	for (i = 0; i < num_interfaces ; i++){
		interface   = &descriptor->interface[i];
		idescriptor = interface->altsetting;
		endpoint    = idescriptor->endpoint;

		for (j = 0; j < idescriptor->bNumEndpoints; j++, endpoint++) {
			switch (endpoint->bmAttributes & 0x3) {

				case LIBUSB_TRANSFER_TYPE_INTERRUPT:
					if (cmd_in_address)
						continue;

					cmd_in_address = endpoint->bEndpointAddress;
					BT_INFO("-> using 0x%2.2X for HCI Events", cmd_in_address);
					break;
				case LIBUSB_TRANSFER_TYPE_BULK:
					if (endpoint->bEndpointAddress & 0x80) {
						if (acl_in_address)
							continue;

						acl_in_address = endpoint->bEndpointAddress;
						BT_INFO("-> using 0x%2.2X for ACL Data In", acl_in_address);
					} else {
						if (acl_out_address)
							continue;

						acl_out_address = endpoint->bEndpointAddress;
						BT_INFO("-> using 0x%2.2X for ACL Data Out", acl_out_address);
					}
					break;

				case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
					if (endpoint->bEndpointAddress & 0x80) {
						if (sco_in_address)
							continue;

						sco_in_address = endpoint->bEndpointAddress;
						BT_INFO("-> using 0x%2.2X for SCO Data In", sco_in_address);
					} else {
						if (sco_out_address)
							continue;

						sco_out_address = endpoint->bEndpointAddress;
						BT_INFO("-> using 0x%2.2X for SCO Data Out", sco_out_address);
					}
					break;
				default:
					break;
			}
		}
	}

	g_uhandle[TRANSFER_INPUT_CMD].address  = cmd_in_address;
	g_uhandle[TRANSFER_INPUT_ACL].address  = acl_in_address;
	g_uhandle[TRANSFER_OUTPUT_ACL].address = acl_out_address;

	libusb_free_config_descriptor(descriptor);

	return 0;
}

static int usb_alloc(libusb_device_handle *handle)
{
	struct usb_handler *uhandle;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(g_uhandle); i++) {
		uhandle = &g_uhandle[i];
		uhandle->transfer = libusb_alloc_transfer(0);
		if (uhandle->transfer == NULL)
			return -ENOMEM;
		sem_init(&uhandle->sem, 0, 1);
		uhandle->type = i;
	}

	uhandle = &g_uhandle[TRANSFER_INPUT_CMD];

	libusb_fill_interrupt_transfer(uhandle->transfer,
			handle, uhandle->address, hci_command_input_buffer, HCI_BUFSIZE,
			usb_callback, uhandle, 0);

	ret = libusb_submit_transfer(uhandle->transfer);
	if (ret)
		return ret;

	uhandle = &g_uhandle[TRANSFER_INPUT_ACL];

	libusb_fill_bulk_transfer(uhandle->transfer,
			handle, uhandle->address, hci_acl_input_buffer,
			HCI_BUFSIZE, usb_callback, uhandle, 0) ;

	return libusb_submit_transfer(uhandle->transfer);
}

static void usb_close(libusb_device_handle *handle)
{
	struct usb_handler *uhandle;
	struct timeval tv = {};
	int i;

	for (i = 0; i < ARRAY_SIZE(g_uhandle); i++) {
		uhandle = &g_uhandle[i];
		if (uhandle->transfer == NULL)
			continue;

		if (uhandle->type == TRANSFER_INPUT_CMD ||
				uhandle->type == TRANSFER_INPUT_ACL)
			libusb_cancel_transfer(uhandle->transfer);
		libusb_free_transfer(uhandle->transfer);
	}

	libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_WARNING);
	libusb_handle_events_timeout(NULL, &tv);
	libusb_release_interface(handle, 0);
	libusb_close(handle);
}

static void rx_thread(void *p1, void *p2, void *p3)
{
	struct libusb_transfer *transfer;
	struct bt_hci_evt_hdr *evt;
	struct timeval tv = {};
	struct net_buf *buf;
	uint8_t *data;
	uint32_t len;
	uint8_t type;
	int ret;

	for (;;) {
		ret = libusb_handle_events_timeout(NULL, &tv);
		if (g_command_transfer) {
			transfer = g_command_transfer;
			g_command_transfer = NULL;
			type = H4_EVT;
		} else if (g_acl_transfer) {
			transfer = g_acl_transfer;
			g_acl_transfer = NULL;
			type = H4_ACL;
		} else {
			usleep(1);
			continue;
		}

		data = transfer->buffer;
		len = transfer->actual_length;

		usb_data_dump("R", data, len);

		if (type == H4_EVT) {
			evt = (struct bt_hci_evt_hdr *)data;
			buf = bt_buf_get_evt(evt->evt, false, K_FOREVER);
		} else
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);

		if (buf == NULL)
			break;

		if (len > buf->size) {
			net_buf_unref(buf);
			continue;
		}

		memcpy(buf->data, data, len);
		net_buf_add(buf, len);

		bt_buf_set_type(buf, type == H4_EVT ? BT_BUF_EVT : BT_BUF_ACL_IN);

		bt_recv(buf);

		ret = libusb_submit_transfer(transfer);
		if (ret < 0)
			break;
	}

	BT_ERR("INVALID BUF FORMAT\n");
}

static int h2_open(void)
{
	libusb_device_handle *handle = NULL;
	int ret;

	ret = libusb_init(NULL);
	if (ret < 0)
		return ret;

	ret = usb_open(&handle);
	if (ret < 0)
		goto bail;

	ret = usb_prepare(handle);
	if (ret < 0)
		goto bail;

	ret = usb_scan(handle);
	if (ret < 0)
		goto bail;

	ret = usb_alloc(handle);
	if (ret < 0)
		goto bail;

	g_handle = handle;

	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack),
			rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	k_thread_name_set(&rx_thread_data, "BT Driver");

	return 0;

bail:
	if (handle)
		usb_close(handle);

	libusb_exit(NULL);

	return ret;
}

static int h2_send(struct net_buf *buf)
{
	uint8_t type = bt_buf_get_type(buf);
	struct usb_handler *uhandle;
	uint8_t *data = buf->data;
	uint32_t len = buf->len;
	int ret;

	usb_data_dump("W", data, len);

	if (type == BT_BUF_CMD) {
		uhandle = &g_uhandle[TRANSFER_OUTPUT_CMD];
	} else if (type == BT_BUF_ACL_OUT) {
		uhandle = &g_uhandle[TRANSFER_OUTPUT_ACL];
	} else
		return -EINVAL;

	sem_wait(&uhandle->sem);

	if (type == BT_BUF_CMD) {
		libusb_fill_control_setup(hci_command_send_buffer,
				LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 0, 0, 0, len);
		memcpy(hci_command_send_buffer + LIBUSB_CONTROL_SETUP_SIZE, data, len);

		libusb_fill_control_transfer(uhandle->transfer, g_handle,
				hci_command_send_buffer, usb_callback, uhandle, 0);
		uhandle->transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER;

	} else if (type == BT_BUF_ACL_OUT) {
		libusb_fill_bulk_transfer(uhandle->transfer, g_handle,
				uhandle->address, data, len, usb_callback, uhandle, 0);
	}

	ret = libusb_submit_transfer(uhandle->transfer);
	if (ret < 0)
		return ret;

	net_buf_unref(buf);

	return ret < 0 ? ret : 0;
}

static struct bt_hci_driver driver = {
	.name   = "H:2",
	.bus    = BT_HCI_DRIVER_BUS_USB,
	.open   = h2_open,
	.send   = h2_send,
#if defined(CONFIG_BT_QUIRK_NO_RESET)
	.quirks = BT_QUIRK_NO_RESET,
#endif
};

int bt_uart_init(void)
{
	return bt_hci_driver_register(&driver);
}
