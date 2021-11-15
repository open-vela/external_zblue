/** @file
 * @brief Bluetooth shell module
 *
 * Provide some Bluetooth shell commands that can be useful to applications.
 */

/*
 * Copyright (c) 2021 Xiaomi Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>

#include <shell/shell.h>
#include "bt.h"

enum {
	CMD_BROADCAST,
	CMD_OBSERVER,
	CMD_PERIPHERAL,
	CMD_PERIPHERAL_CONN,
	CMD_PERIPHERAL_CONN_TERMINATED,
        CMD_PERIPHERAL_CANCEL_PENDING,
	CMD_CENTRAL,
	CMD_CENTRAL_CONN,
	CMD_CENTRAL_CONN_TERMINATED,
	CMD_CENTRAL_CANCEL_PENDING,
};

static atomic_t states;

#define MIBLE_SCAN_WIN_DEF	30
#define MIBLE_SCAN_INT_DEF	30

#define BD_TEST_COUNT_DEF 1000
#define ADV_INT_FAST_MS 20
#define BT_ADV_SCAN_UNIT(_ms) ((_ms) * 8 / 5)

static uint32_t bd_count;
static uint32_t mfg_data;
#define BD_NAME_PREFIX	"Xiaomi-IOT"
static uint8_t bd_name[] = BD_NAME_PREFIX"-00";

static uint16_t write_cmd_handle = 0x21;
static void central_handler(struct k_work *work);
static k_timeout_t central_throughput_interval = K_FOREVER;

static K_WORK_DELAYABLE_DEFINE(central_work, central_handler);

static void peripheral_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(peripheral_work, peripheral_handler);
static k_timeout_t peripheral_throughput_interval = K_FOREVER;

static struct {
	uint32_t connected_count;
	uint32_t tx_octers;
	uint32_t rx_octers;
} central_status;

static struct {
	uint32_t connected_count;
	uint32_t tx_octers;
	uint32_t rx_octers;
} peripheral_status;

/* Custom Service Variables */
#define BT_UUID_CUSTOM_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

static struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
	BT_UUID_CUSTOM_SERVICE_VAL);

static struct bt_uuid_128 vnd_ntf_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

#define VND_MAX_LEN 18

static uint8_t vnd_value[VND_MAX_LEN + 1] = { 'V', 'e', 'n', 'd', 'o', 'r'};
static uint8_t vnd_wwr_value[VND_MAX_LEN + 1] = { 'V', 'e', 'n', 'd', 'o', 'r' };

static void update_beacon_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(update_beacon, update_beacon_handler);
static int update_beacon_interval = ADV_INT_FAST_MS;

static ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static ssize_t write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset,
			 uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > VND_MAX_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);
	value[offset + len] = 0;

	return len;
}

static void vnd_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
}

static const struct bt_uuid_128 vnd_write_cmd_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4));

static ssize_t write_without_rsp_vnd(struct bt_conn *conn,
				     const struct bt_gatt_attr *attr,
				     const void *buf, uint16_t len, uint16_t offset,
				     uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (!(flags & BT_GATT_WRITE_FLAG_CMD)) {
		/* Write Request received. Reject it since this Characteristic
		 * only accepts Write Without Response.
		 */
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
	}

	peripheral_status.rx_octers += len;

	memcpy(value + offset, buf, len);
	value[offset + len] = 0;

	return len;
}

/* Vendor Primary Service Declaration */
BT_GATT_SERVICE_DEFINE(mible_svc,
	BT_GATT_PRIMARY_SERVICE(&vnd_uuid),
	BT_GATT_CHARACTERISTIC(&vnd_ntf_uuid.uuid,
			       BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
			       read_vnd, write_vnd, vnd_value),
	BT_GATT_CCC(vnd_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&vnd_write_cmd_uuid.uuid,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE, NULL,
			       write_without_rsp_vnd, &vnd_wwr_value),
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, bd_name, sizeof(bd_name) - 1),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, &mfg_data, sizeof(mfg_data)),
};

static void update_beacon_handler(struct k_work *work)
{
	int err;
	struct k_work_delayable *d_work;

	if (!atomic_test_bit(&states, CMD_BROADCAST)) {
		return;
	}

	if (mfg_data++ > bd_count) {
		err = bt_le_adv_stop();
		if (err) {
			shell_error(ctx_shell, "Unable to stop advertiser (err %d)", err);
		}

		atomic_clear_bit(&states, CMD_BROADCAST);

		mfg_data = 0;

		shell_print(ctx_shell, "Broadcaster test completed");
		return;
	}

	err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		shell_error(ctx_shell, "Unable to update data err:%d", err);
		return;
	}

	d_work = CONTAINER_OF(work, struct k_work_delayable, work);
	k_work_reschedule(d_work, K_MSEC(update_beacon_interval));
}

static void central_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			         struct net_buf_simple *ad);

static struct bt_conn *peripheral_conn;
static struct bt_conn *central_conn;
static void le_connected(struct bt_conn *conn, uint8_t err)
{
	int ret;
	struct bt_conn_info info;
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_ADV_SCAN_UNIT(MIBLE_SCAN_INT_DEF),
		.window = BT_ADV_SCAN_UNIT(MIBLE_SCAN_WIN_DEF),
	};

	if (err) {
		shell_error(ctx_shell, "Failed to connect (%u)\n", err);

		if (central_conn) {
			bt_conn_unref(central_conn);
			central_conn = NULL;
		}

		if (atomic_test_bit(&states, CMD_PERIPHERAL)) {
			return;
		}

		if (atomic_test_and_clear_bit(&states, CMD_CENTRAL_CONN_TERMINATED)) {
			return;
		}

		ret = bt_le_scan_start(&param, central_device_found);
		if (ret) {
			shell_error(ctx_shell, "Scanning failed to start (err %d)", ret);
		}

		return;
	}

	bt_conn_get_info(conn, &info);
	if (info.role == BT_CONN_ROLE_SLAVE) {
		peripheral_conn = bt_conn_ref(conn);
		peripheral_status.connected_count++;
		atomic_set_bit(&states, CMD_PERIPHERAL_CONN);
                k_work_reschedule(&peripheral_work, peripheral_throughput_interval);
		return;
	}

	central_status.connected_count++;
	atomic_set_bit(&states, CMD_CENTRAL_CONN);

	k_work_reschedule(&central_work, central_throughput_interval);
}

static void le_disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	struct bt_conn_info info;
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_ADV_SCAN_UNIT(MIBLE_SCAN_INT_DEF),
		.window = BT_ADV_SCAN_UNIT(MIBLE_SCAN_WIN_DEF),
	};

	bt_conn_get_info(conn, &info);
	if (info.role == BT_CONN_ROLE_SLAVE) {
		if (peripheral_conn) {
			bt_conn_unref(peripheral_conn);
			peripheral_conn = NULL;
		}

		if (atomic_test_and_clear_bit(&states, CMD_PERIPHERAL_CONN_TERMINATED)) {
			err = bt_le_adv_stop();
			if (err) {
				shell_error(ctx_shell, "Unable to stop advertiser (err %d)", err);
			}
		}

		atomic_clear_bit(&states, CMD_PERIPHERAL_CONN);
                atomic_clear_bit(&states, CMD_PERIPHERAL_CANCEL_PENDING);
		return;
	}

	bt_conn_unref(central_conn);
	central_conn = NULL;

	atomic_clear_bit(&states, CMD_CENTRAL_CONN);
	atomic_clear_bit(&states, CMD_CENTRAL_CANCEL_PENDING);

	if (atomic_test_and_clear_bit(&states, CMD_CENTRAL_CONN_TERMINATED)) {
		return;	
	}

	err = bt_le_scan_start(&param, central_device_found);
	if (err) {
		shell_error(ctx_shell, "Scanning failed to start (err %d)", err);
	}
}

static struct bt_conn_cb conn_cb = {
	.connected = le_connected,
	.disconnected = le_disconnected,
};

static uint8_t find_next(const struct bt_gatt_attr *attr, uint16_t handle,
			 void *user_data)
{
	struct bt_gatt_attr **next = user_data;

	*next = (struct bt_gatt_attr *)attr;

	return BT_GATT_ITER_STOP;
}

static struct bt_gatt_attr *bt_gatt_find_by_uuid(const struct bt_gatt_attr *attr,
					  uint16_t attr_count,
					  const struct bt_uuid *uuid)
{
	struct bt_gatt_attr *found = NULL;
	uint16_t start_handle = bt_gatt_attr_value_handle(attr);
	uint16_t end_handle = start_handle && attr_count ?
			      start_handle + attr_count : 0xffff;

	bt_gatt_foreach_attr_type(start_handle, end_handle, uuid, NULL, 1,
				  find_next, &found);

	return found;
}

static int cmd_init(const struct shell *shell, size_t argc, char *argv[])
{
	int err;
	struct bt_gatt_attr *vnd_ntf_attr, *vnd_cmd_attr;

	err = bt_enable(NULL);
	if (err) {
		shell_error(shell, "bt init failed err: %d", err);
	}

	ctx_shell = shell;

	vnd_ntf_attr = bt_gatt_find_by_uuid(mible_svc.attrs, mible_svc.attr_count,
					    &vnd_ntf_uuid.uuid);
	shell_print(shell, "Notify VND handle 0x%04x",
		    bt_gatt_attr_get_handle(vnd_ntf_attr));

	vnd_cmd_attr = bt_gatt_find_by_uuid(mible_svc.attrs, mible_svc.attr_count,
					    &vnd_write_cmd_uuid.uuid);
	shell_print(shell, "Write Command VND handle 0x%04x",
		    bt_gatt_attr_get_handle(vnd_cmd_attr));

	bt_conn_cb_register(&conn_cb);

	shell_print(shell, "Bluetooth initialized");

	return 0;
}

static int cmd_broadcast(const struct shell *shell, size_t argc, char *argv[])
{
	int err;
	bool start, force = false;
	struct bt_le_adv_param param = {};

	if (!strcmp(argv[1], "on")) {
		start = true;
	} else if (!strcmp(argv[1], "off")) {
		start = false;
	} else {
		shell_help(shell);
		return 0;
	}

	if (!start) {
		if (!atomic_test_and_clear_bit(&states, CMD_BROADCAST)) {
			return 0;
		}

		err = bt_le_adv_stop();
		if (err) {
			shell_error(shell, "Unable to stop advertiser (err %d)", err);
		}

		mfg_data = 0;
		k_work_cancel_delayable(&update_beacon);

		shell_print(shell, "Stoped");

		return 0;
	}

	if (argc > 2) {
		bd_count = strtoul(argv[2], NULL, 10);
	} else {
		bd_count = BD_TEST_COUNT_DEF;
	}

	if (argc > 3) {
		if (!strcmp(argv[3], "force")) {
			force = true;
		} else {
			shell_help(shell);
			return 0;
		}
	}

	if (atomic_test_bit(&states, CMD_PERIPHERAL)) {
		if (!atomic_test_bit(&states, CMD_PERIPHERAL_CONN)) {
			shell_error(shell, "Busy peripheral advertising");
			return 0;
		} else if (!force) {
			shell_help(shell);
			return 0;
		}
	}

	if (atomic_test_and_set_bit(&states, CMD_BROADCAST)) {
		shell_error(shell, "Busy");
		return 0;
	}

	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_ADV_SCAN_UNIT(ADV_INT_FAST_MS);
	param.interval_max = param.interval_min;
	param.options = BT_LE_ADV_OPT_USE_IDENTITY;

	err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		shell_error(ctx_shell, "Advertising failed to start (err %d)", err);
		return 0;
	}

	shell_print(shell, "Broadcaster started with cycles %d", bd_count);

	k_work_reschedule(&update_beacon, K_MSEC(ADV_INT_FAST_MS));

	return 0;
}

static int cmd_broadcast_id(const struct shell *shell, size_t argc, char *argv[])
{
	uint16_t id;

	id = strtoul(argv[1], NULL, 16);
	bd_name[sizeof(bd_name) - 3] = (uint8_t)'0' + ((id & 0xf0) >> 4);
	bd_name[sizeof(bd_name) - 2] = (uint8_t)'0' + (id & 0x0f);

	if (argc > 2) {
		update_beacon_interval = strtoul(argv[2], NULL, 10);
	}

	shell_print(shell, "Broadcaster id set successfully");

	k_work_reschedule(&update_beacon, K_MSEC(update_beacon_interval));

	return 0;
}

struct mible_scan_dump {
	uint32_t total_count;
	uint32_t found_count;
	int8_t rssi_min;
	int8_t rssi_max;
};

static struct mible_scan_dump scan_result = {
	.rssi_min = INT8_MAX,
	.rssi_max = INT8_MIN,
};

static void obs_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			     struct net_buf_simple *ad)
{
	scan_result.total_count++;

	if (rssi > scan_result.rssi_max) {
		scan_result.rssi_max = rssi;
	}

	if (rssi < scan_result.rssi_min) {
		scan_result.rssi_min = rssi;
	}

	if (ad->len < 20) {
		return;
	}

	if (ad->data[4] != BT_DATA_NAME_COMPLETE) {
		return;
	}

	if (ad->data[3] != sizeof(bd_name)) {
		return;
	}

	if (memcmp(&ad->data[5], bd_name, sizeof(BD_NAME_PREFIX) - 1)) {
		return;
	}

	scan_result.found_count++;
}

static int cmd_observer(const struct shell *shell, size_t argc, char *argv[])
{
	int err;
	bool start;
	uint16_t interval, window;
	struct bt_le_scan_param param = {};

	if (!strcmp(argv[1], "on")) {
		start = true;
	} else if (!strcmp(argv[1], "off")) {
		start = false;
	} else {
		shell_help(shell);
		return 0;
	}

	if (!start) {
		if (!atomic_test_and_clear_bit(&states, CMD_OBSERVER)) {
			return 0;
		}

		err = bt_le_scan_stop();
		if (err) {
			shell_error(shell, "Unable to stop obserser (err %d)", err);
		}

		shell_print(shell, "Stoped");

		return 0;
	}

	if (argc > 3) {
		window = strtoul(argv[2], NULL, 10);
		interval = strtoul(argv[3], NULL, 10);
	} else if (argc > 2){
		window = strtoul(argv[2], NULL, 10);
		interval = MIBLE_SCAN_INT_DEF;
	} else {
		window = MIBLE_SCAN_WIN_DEF;
		interval = MIBLE_SCAN_INT_DEF;
	}

	if (atomic_test_bit(&states, CMD_CENTRAL) ||
	    atomic_test_and_set_bit(&states, CMD_OBSERVER)) {
		shell_error(shell, "Busy");
		return 0;
	}

	param.window = BT_ADV_SCAN_UNIT(window);
	param.interval = BT_ADV_SCAN_UNIT(interval);
	err = bt_le_scan_start(&param, obs_device_found);
	if (err) {
		shell_error(ctx_shell, "Scanning failed to start (err %d)", err);
		return 0;
	}

	shell_print(shell, "Scanning successfully started with win/int = %d/%d(ms)",
		    window, interval);

	k_work_reschedule(&update_beacon, K_MSEC(update_beacon_interval));

	return 0;
}

static void peripheral_notify_cb(struct bt_conn *conn, void *user_data)
{
	peripheral_status.tx_octers += ARRAY_SIZE(vnd_wwr_value);
	k_work_reschedule(&peripheral_work, peripheral_throughput_interval);
}

static void peripheral_handler(struct k_work *work)
{
	int err;
	struct bt_gatt_notify_params params = {
		.data = vnd_wwr_value,
		.len = ARRAY_SIZE(vnd_wwr_value),
		.func = peripheral_notify_cb,
	};

	params.attr = &mible_svc.attrs[2];

	if (!peripheral_conn) {
		return;
	}

	err = bt_gatt_notify_cb(peripheral_conn, &params);
	if (err && err != -ENOTCONN) {
		shell_error(ctx_shell, "Unable send notify (err %d)", err);
	}
}

static int cmd_peripheral(const struct shell *shell, size_t argc, char *argv[])
{
	int err;
	bool start;
	struct bt_le_adv_param param = {};

	if (!strcmp(argv[1], "on")) {
		start = true;
	} else if (!strcmp(argv[1], "off")) {
		start = false;
	} else {
		shell_help(shell);
		return 0;
	}

	if (!start) {
		if (!atomic_test_and_clear_bit(&states, CMD_PERIPHERAL)) {
			return 0;
		}

		if (!atomic_test_bit(&states, CMD_PERIPHERAL_CONN)) {
			err = bt_le_adv_stop();
			if (err) {
				shell_error(shell, "Unable to stop advertiser (err %d)", err);
			}

		} else if (peripheral_conn) {
			err = bt_conn_disconnect(peripheral_conn, 0x13);
			if (err) {
				shell_error(shell, "Unable to disconnect (err %d)", err);
			}

			atomic_set_bit(&states, CMD_PERIPHERAL_CONN_TERMINATED);
                        atomic_set_bit(&states, CMD_PERIPHERAL_CANCEL_PENDING);
		}

		shell_print(shell, "Stoped");

		return 0;
	}

	if (atomic_test_bit(&states, CMD_BROADCAST) ||
	    atomic_test_and_set_bit(&states, CMD_PERIPHERAL)) {
		shell_error(shell, "Busy");
		return 0;
	}

	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_ADV_SCAN_UNIT(ADV_INT_FAST_MS);
	param.interval_max = param.interval_min;
	param.options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_IDENTITY;

	err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		shell_error(ctx_shell, "Advertising failed to start (err %d)", err);
		return 0;
	}

	shell_print(shell, "Advertising started");

	return 0;
}

static int cmd_peripheral_id(const struct shell *shell, size_t argc, char *argv[])
{
	uint16_t id;

	id = strtoul(argv[1], NULL, 16);
	bd_name[sizeof(bd_name) - 3] = (uint8_t)'0' + ((id & 0xf0) >> 4);
	bd_name[sizeof(bd_name) - 2] = (uint8_t)'0' + (id & 0x0f);

	shell_print(shell, "Peripheral id set successfully");

	return 0;
}

static int cmd_peripheral_throughput(const struct shell *shell, size_t argc, char *argv[])
{
	uint32_t interval;

	interval = strtoul(argv[1], NULL, 10);
	if (!interval) {
		peripheral_throughput_interval = K_NO_WAIT;
	} else if (interval < UINT32_MAX) {
		peripheral_throughput_interval = K_MSEC(interval);
	} else {
		peripheral_throughput_interval = K_FOREVER;
	}

	k_work_reschedule(&peripheral_work, peripheral_throughput_interval);

	shell_print(shell, "Peripheral throughput interval set successfully");

	return 0;
}

static bt_addr_t peer;

static void central_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			         struct net_buf_simple *ad)
{
	int err;
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_ADV_SCAN_UNIT(MIBLE_SCAN_INT_DEF),
		.window = BT_ADV_SCAN_UNIT(MIBLE_SCAN_WIN_DEF),
	};

	if (type != BT_GAP_ADV_TYPE_ADV_IND) {
		return;
	}

	if (!bt_addr_cmp(&peer, BT_ADDR_ANY)) {
		if (ad->len < 20) {
			return;
		}

		if (ad->data[4] != BT_DATA_NAME_COMPLETE) {
			return;
		}

		if (ad->data[3] != sizeof(bd_name)) {
			return;
		}

		if (memcmp(&ad->data[5], bd_name, sizeof(bd_name) - 1)) {
			return;
		}
	} else if (bt_addr_cmp(&peer, &addr->a)) {
		return;
	}

	if (bt_le_scan_stop()) {
		return;
	}

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &central_conn);
	if (err) {
		shell_error(ctx_shell, "Create conn failed (err %d)", err);

		err = bt_le_scan_start(&param, central_device_found);
		if (err) {
			shell_error(ctx_shell, "Scanning failed to start (err %d)", err);
		}

		return;
	}

	shell_print(ctx_shell, "Try to create connection");
}

static void central_write_cmd_cb(struct bt_conn *conn, void *user_data)
{
	central_status.tx_octers += ARRAY_SIZE(vnd_wwr_value);
	k_work_reschedule(&central_work, central_throughput_interval);
}

static void central_handler(struct k_work *work)
{
	int err;

	if (!central_conn) {
		return;
	}

	err = bt_gatt_write_without_response_cb(central_conn, write_cmd_handle,
					        vnd_wwr_value, ARRAY_SIZE(vnd_wwr_value),
						false, central_write_cmd_cb, NULL);
	if (err && err != -ENOTCONN) {
		shell_error(ctx_shell, "Unable send write command (err %d)", err);
	}
}

static int cmd_central(const struct shell *shell, size_t argc, char *argv[])
{
	int err;
	bool start;
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_ADV_SCAN_UNIT(MIBLE_SCAN_INT_DEF),
		.window = BT_ADV_SCAN_UNIT(MIBLE_SCAN_WIN_DEF),
	};

	if (!strcmp(argv[1], "on")) {
		start = true;
	} else if (!strcmp(argv[1], "off")) {
		start = false;
	} else {
		shell_help(shell);
		return 0;
	}

	if (!start) {
		if (!atomic_test_and_clear_bit(&states, CMD_CENTRAL)) {
			return 0;
		}

		if (!atomic_test_bit(&states, CMD_CENTRAL_CONN)) {
			err = bt_le_scan_stop();
			if (err) {
				shell_error(shell, "Unable to stop scanner (err %d)", err);
			}
		} else if (central_conn) {
			err = bt_conn_disconnect(central_conn, 0x13);
			if (err) {
				shell_error(shell, "Unable to disconnect (err %d)", err);
			}

			atomic_set_bit(&states, CMD_CENTRAL_CONN_TERMINATED);
                        atomic_set_bit(&states, CMD_CENTRAL_CANCEL_PENDING);
		}

		shell_print(shell, "Stoped");

		return 0;
	}

	if (atomic_test_bit(&states, CMD_OBSERVER) ||
	    atomic_test_and_set_bit(&states, CMD_CENTRAL)) {
		shell_error(shell, "Busy");
		return 0;
	}

	err = bt_le_scan_start(&param, central_device_found);
	if (err) {
		shell_error(ctx_shell, "Scanning failed to start (err %d)", err);
		return 0;
	}

	shell_print(shell, "Scanning successfully started");

	return 0;
}

static int cmd_central_target(const struct shell *shell, size_t argc, char *argv[])
{
	if (bt_addr_from_str(argv[1], &peer)) {
		shell_error(ctx_shell, "Invalid MAC");
		return 0;
	}

	if (argc > 2) {
		write_cmd_handle = strtoul(argv[2], NULL, 16);
	}

	shell_print(shell, "Target successfully seted");

	return 0;
}

static int cmd_central_throughput(const struct shell *shell, size_t argc, char *argv[])
{
	uint32_t interval;

	interval = strtoul(argv[1], NULL, 10);
	if (!interval) {
		central_throughput_interval = K_NO_WAIT;
	} else if (interval < UINT32_MAX) {
		central_throughput_interval = K_MSEC(interval);
	} else {
		central_throughput_interval = K_FOREVER;
	}

	k_work_reschedule(&central_work, central_throughput_interval);

	shell_print(shell, "Central throughput interval set successfully");

	return 0;
}

typedef int (*do_action_t)(const struct shell *shell, size_t argc, char *argv[]);

static uint32_t cmd_show_timeout;
static void cmd_show_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(cmd_show, cmd_show_handler);

static void cmd_show_handler(struct k_work *work)
{
	if (atomic_test_bit(&states, CMD_OBSERVER)) {
		shell_print(ctx_shell, "[OBSERVER] total %d found %d rssi min/max (%d/%d)", 
			    scan_result.total_count, scan_result.found_count,
	                    scan_result.rssi_min, scan_result.rssi_max);
	}

        if (atomic_test_bit(&states, CMD_CENTRAL)) {
	        shell_print(ctx_shell, "[Central]  CONN %d TX %d", 
		            central_status.connected_count,
			    central_status.tx_octers);
        }

        if (atomic_test_bit(&states, CMD_PERIPHERAL)) {
	        shell_print(ctx_shell, "[PERIPHERAL]  CONN %d TX %d RX %d", 
		            peripheral_status.connected_count,
			    peripheral_status.tx_octers, peripheral_status.rx_octers);
        }

	if (cmd_show_timeout) {
		k_work_reschedule(&cmd_show, K_SECONDS(cmd_show_timeout));
	}
}

static int cmd_log_show(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc > 1) {
		cmd_show_timeout = strtoul(argv[1], NULL, 10);
	} else {
                cmd_show_timeout = 0;
        }

	if (atomic_test_bit(&states, CMD_OBSERVER)) {
		shell_print(ctx_shell, "[OBSERVER] total %d found %d rssi min/max (%d/%d)", 
			    scan_result.total_count, scan_result.found_count,
	                    scan_result.rssi_min, scan_result.rssi_max);
	}

        if (atomic_test_bit(&states, CMD_CENTRAL)) {
	        shell_print(ctx_shell, "[Central]  CONN %d TX %d", 
		            central_status.connected_count,
			    central_status.tx_octers);
        }

        if (atomic_test_bit(&states, CMD_PERIPHERAL)) {
	        shell_print(ctx_shell, "[PERIPHERAL]  CONN %d TX %d RX %d", 
		            peripheral_status.connected_count,
			    peripheral_status.tx_octers, peripheral_status.rx_octers);
        }

	if (cmd_show_timeout) {
		shell_print(ctx_shell, "Show with timeout %d seconds", cmd_show_timeout);
		k_work_reschedule(&cmd_show, K_SECONDS(cmd_show_timeout));
	} else {
		shell_print(ctx_shell, "Periodic Show stoped");
		(void)k_work_cancel_delayable(&cmd_show);
	}

	return 0;
}

static int cmd_log_clear(const struct shell *shell, size_t argc, char *argv[])
{
	memset(&scan_result, 0, sizeof(scan_result));
	scan_result.rssi_max = INT8_MIN;
	scan_result.rssi_min = INT8_MAX;

	memset(&central_status, 0, sizeof(central_status));
	memset(&peripheral_status, 0, sizeof(peripheral_status));

	shell_print(shell, "Cleared");

	return 0;
}

static uint32_t cmd_peri_disc_timeout;
static void cmd_peri_disc_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cmd_per_disc, cmd_peri_disc_handler);

static void cmd_peri_disc_handler(struct k_work *work)
{
	int err;

	if (atomic_test_bit(&states, CMD_PERIPHERAL_CONN)) {
		shell_print(ctx_shell, "[Periodic] Peripheral disconnecting");

		err = bt_conn_disconnect(peripheral_conn, 0x13);
		if (err) {
			shell_error(ctx_shell, "Unable to disconnect (err %d)", err);
		}

                atomic_set_bit(&states, CMD_PERIPHERAL_CANCEL_PENDING);
	} else {
		shell_print(ctx_shell, "[Periodic] Peripheral not disconnected, skiped");
	}

	if (cmd_peri_disc_timeout) {
		k_work_reschedule(&cmd_per_disc, K_SECONDS(cmd_peri_disc_timeout));
	}
}

static int cmd_peripheral_periodic_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc > 1) {
		cmd_peri_disc_timeout = strtoul(argv[1], NULL, 10);
	} else {
                cmd_peri_disc_timeout = 0;
        }

	if (cmd_peri_disc_timeout) {
		shell_print(ctx_shell, "Periodic Peripheral disconnect action timeout %d seconds",
			    cmd_peri_disc_timeout);
		k_work_reschedule(&cmd_per_disc, K_SECONDS(cmd_peri_disc_timeout));
	} else {
		shell_print(ctx_shell, "Periodic Peripheral disconnect action stoped");
		(void)k_work_cancel_delayable(&cmd_per_disc);
	}

	return 0;
}

static uint32_t cmd_cen_disc_timeout;
static void cmd_cen_disc_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cmd_c_disc, cmd_cen_disc_handler);

static void cmd_cen_disc_handler(struct k_work *work)
{
	int err;

	if (atomic_test_bit(&states, CMD_CENTRAL_CONN)) {
		shell_print(ctx_shell, "[Periodic] Central disconnecting");

		(void)k_work_cancel_delayable(&central_work);
		err = bt_conn_disconnect(central_conn, 0x13);
		if (err) {
			shell_error(ctx_shell, "Unable to disconnect (err %d)", err);
		}

                atomic_set_bit(&states, CMD_CENTRAL_CANCEL_PENDING);
	} else {
		shell_print(ctx_shell, "[Periodic] Central not disconnected, skiped");
	}

	if (cmd_cen_disc_timeout) {
		k_work_reschedule(&cmd_c_disc, K_SECONDS(cmd_cen_disc_timeout));
	}
}

static int cmd_central_periodic_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc > 1) {
		cmd_cen_disc_timeout = strtoul(argv[1], NULL, 10);
	} else {
                cmd_cen_disc_timeout = 0;
        }

	if (cmd_cen_disc_timeout) {
		shell_print(ctx_shell, "Periodic Central disconnect action timeout %d seconds",
			    cmd_cen_disc_timeout);
		k_work_reschedule(&cmd_c_disc, K_SECONDS(cmd_cen_disc_timeout));
	} else {
		shell_print(ctx_shell, "Periodic Central disconnect action stoped");
		(void)k_work_cancel_delayable(&cmd_c_disc);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(mible_cmds,
	SHELL_CMD_ARG(init, NULL, NULL, cmd_init, 1, 0),
	SHELL_CMD_ARG(broadcast, NULL, "<value on, off> [cycles force]", cmd_broadcast, 2, 2),
	SHELL_CMD_ARG(broadcast_id, NULL, "<id> [interval(ms)]", cmd_broadcast_id, 2, 1),
	SHELL_CMD_ARG(observer, NULL, "<value on, off> [window interval(ms)]", cmd_observer, 2, 2),
	SHELL_CMD_ARG(peripheral, NULL, "<value on, off>", cmd_peripheral, 2, 0),
	SHELL_CMD_ARG(peripheral_periodic_disconnect, NULL, "periodic(s)", cmd_peripheral_periodic_disconnect, 2, 0),
	SHELL_CMD_ARG(peripheral_id, NULL, "<id>", cmd_peripheral_id, 2, 0),
	SHELL_CMD_ARG(peripheral_throughput, NULL, "<interval(ms)>", cmd_peripheral_throughput, 2, 0),
	SHELL_CMD_ARG(central, NULL, "<value on, off>", cmd_central, 2, 0),
	SHELL_CMD_ARG(central_periodic_disconnect, NULL, "periodic(s)", cmd_central_periodic_disconnect, 2, 0),
	SHELL_CMD_ARG(central_target, NULL, "<peer address> [handle]", cmd_central_target, 2, 1),
	SHELL_CMD_ARG(central_throughput, NULL, "<interval(ms)>", cmd_central_throughput, 2, 0),

	SHELL_CMD_ARG(log_show, NULL, "[periodic(s)]", cmd_log_show, 1, 1),
	SHELL_CMD_ARG(log_clear, NULL, NULL, cmd_log_clear, 1, 0),
	SHELL_SUBCMD_SET_END
);

static int cmd_mible(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		shell_help(shell);
		/* shell returns 1 when help is printed */
		return 1;
	}

	shell_error(shell, "%s unknown parameter: %s", argv[0], argv[1]);

	return -ENOEXEC;
}

SHELL_CMD_ARG_REGISTER(mible, &mible_cmds, "mible auto-test shell commands",
		       cmd_mible, 1, 1);
