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
#include "../host/hci_core.h"

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

#define MIBLE_SCAN_WIN_DEF				30
#define MIBLE_SCAN_INT_DEF				30

#define BD_TEST_COUNT_DEF				250
#define ADV_INT_FAST_MS					20
#define ADV_INT_SLOW_MS					100
#define BT_ADV_SCAN_UNIT(_ms)				((_ms) * 8 / 5)

static uint32_t bd_count;
static uint32_t mfg_data;
#define BD_NAME_PREFIX					"Xiaomi-IOT"
static uint8_t bd_name[] =				BD_NAME_PREFIX"-00";

static uint16_t write_cmd_handle = 0x21;
static void central_handler(struct k_work *work);
static k_timeout_t central_throughput_interval;
static K_WORK_DELAYABLE_DEFINE(central_work, central_handler);

static uint32_t cmd_cen_disc_timeout;
static void cmd_cen_disc_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cmd_c_disc, cmd_cen_disc_handler);

static void peripheral_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(peripheral_work, peripheral_handler);
static k_timeout_t peripheral_throughput_interval;
const struct bt_gatt_attr *notify_attr;

static struct {
	uint32_t connecting_count;
	uint32_t connected_count;
	uint32_t disconncted_count;
	uint32_t tx_octers;
	uint32_t tx_checksum;
	uint32_t rx_octers;
	uint32_t rx_checksum;
	uint16_t reason[256];
} central_status;

static struct {
	uint32_t connected_count;
	uint32_t tx_octers;
	uint32_t tx_checksum;
	uint32_t rx_octers;
	uint32_t rx_checksum;
} peripheral_status;

/* Custom Service Variables */
#define BT_UUID_CUSTOM_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

static struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
	BT_UUID_CUSTOM_SERVICE_VAL);

static struct bt_uuid_128 vnd_ntf_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

#define VND_MAX_LEN (MIN(BT_L2CAP_RX_MTU, BT_L2CAP_TX_MTU) - 4)

static uint8_t vnd_value[] = { 'V', 'e', 'n', 'd', 'o', 'r', '\0'};
static uint8_t vnd_wwr_value[VND_MAX_LEN + 1 + 1] = { 'V', 'e', 'n', 'd', 'o', 'r' };
static void adv_timeout_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(adv_timeout, adv_timeout_handler);

static uint32_t cmd_peri_disc_timeout;
static void cmd_peri_disc_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cmd_per_disc, cmd_peri_disc_handler);

static uint32_t crc32_ieee_update(uint32_t crc, const uint8_t *data, size_t len)
{
	/* crc table generated from polynomial 0xedb88320 */
	static const uint32_t table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};

	crc = ~crc;

	for (size_t i = 0; i < len; i++) {
		uint8_t byte = data[i];

		crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
		crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
	}

	return (~crc);
}

#if defined(CONFIG_BT_PERIPHERAL)
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
	peripheral_status.rx_checksum += crc32_ieee_update(peripheral_status.rx_checksum,
							   buf, len);
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
#endif

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, bd_name, sizeof(bd_name) - 1),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, &mfg_data, sizeof(mfg_data)),
};

static void adv_timeout_handler(struct k_work *work)
{
	int err;

	if (!atomic_test_bit(&states, CMD_BROADCAST)) {
		return;
	}

	err = bt_le_adv_stop();
	if (err) {
		shell_error(ctx_shell, "Unable to stop advertiser (err %d)", err);
	}

	atomic_clear_bit(&states, CMD_BROADCAST);

	shell_print(ctx_shell, "Broadcaster test completed");
}

static void central_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			         struct net_buf_simple *ad);

static uint8_t bt_gatt_notify_func(struct bt_conn *conn,
				   struct bt_gatt_subscribe_params *params,
				   const void *data, uint16_t length)
{
	central_status.rx_octers += length;
	central_status.rx_checksum += crc32_ieee_update(central_status.rx_checksum,
							data, length);

	return BT_GATT_ITER_CONTINUE;
}

static struct bt_conn *peripheral_conn;
static struct bt_conn *central_conn;
static void le_connected(struct bt_conn *conn, uint8_t err)
{
	int ret;
	char buffer[BT_ADDR_LE_STR_LEN];
	struct bt_conn_info info;
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_ADV_SCAN_UNIT(MIBLE_SCAN_INT_DEF),
		.window = BT_ADV_SCAN_UNIT(MIBLE_SCAN_WIN_DEF),
	};
	static struct bt_gatt_subscribe_params params;

	params.notify = bt_gatt_notify_func;
	params.value = 0x0001;
	params.value_handle = bt_gatt_attr_get_handle(notify_attr);
	params.ccc_handle = params.value_handle + 1;

	if (err) {
		if (!IS_ENABLED(CONFIG_BT_CENTRAL)) {
			return;
		}

		shell_error(ctx_shell, "Failed to connect (%u)\n", err);

		central_status.reason[err]++;
		central_status.disconncted_count++;

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
	bt_addr_le_to_str(info.le.dst, buffer, sizeof(buffer));

	shell_print(ctx_shell, "%s connected with %s",
		    info.role == BT_CONN_ROLE_PERIPHERAL ? "Peripheral" : "Central", buffer);

	if (info.role == BT_CONN_ROLE_PERIPHERAL) {
		if (!IS_ENABLED(CONFIG_BT_PERIPHERAL)) {
			return;
		}

		peripheral_conn = bt_conn_ref(conn);
		peripheral_status.connected_count++;
		atomic_set_bit(&states, CMD_PERIPHERAL_CONN);
                k_work_reschedule(&peripheral_work, peripheral_throughput_interval);
		return;
	}

	if (!IS_ENABLED(CONFIG_BT_CENTRAL)) {
		return;
	}

	central_status.connected_count++;
	atomic_set_bit(&states, CMD_CENTRAL_CONN);


#if defined(CONFIG_BT_GATT_CLIENT)
	bt_gatt_subscribe(conn, &params);
#endif

	if (IS_ENABLED(CONFIG_BT_GATT_CLIENT)) {
		k_work_reschedule(&central_work, central_throughput_interval);
	}
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

	shell_print(ctx_shell, "%s disconnected (reason 0x%02x)",
		    info.role == BT_CONN_ROLE_PERIPHERAL ? "Peripheral" : "Central", reason);

	if (info.role == BT_CONN_ROLE_PERIPHERAL) {
		if (!IS_ENABLED(CONFIG_BT_PERIPHERAL)) {
			return;
		}

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

	if (!IS_ENABLED(CONFIG_BT_CENTRAL)) {
		return;
	}

	central_status.reason[reason]++;
	central_status.disconncted_count++;

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

#if defined(CONFIG_BT_EXT_ADV)
struct bt_le_ext_adv *advs[CONFIG_BT_EXT_ADV_MAX_ADV_SET];

static void ext_adv_sent(struct bt_le_ext_adv *instance,
			 struct bt_le_ext_adv_sent_info *info)
{
	shell_print(ctx_shell, "Broadcaster set terminated");

	for (int i = 1; i < CONFIG_BT_EXT_ADV_MAX_ADV_SET; i++) {
		if (atomic_test_bit(advs[i]->flags, BT_ADV_ENABLED)) {
			return;
		}
	}

	atomic_clear_bit(&states, CMD_BROADCAST);

	shell_print(ctx_shell, "Broadcaster test completed");
}
#endif

static int cmd_init(const struct shell *shell, size_t argc, char *argv[])
{
	int err;
	const struct bt_gatt_attr *vnd_ntf_attr, *vnd_cmd_attr;

	err = bt_enable(NULL);
	if (err) {
		shell_error(shell, "bt init failed err: %d", err);
	}

	ctx_shell = shell;

	central_throughput_interval = K_FOREVER;
	peripheral_throughput_interval = K_FOREVER;

#if defined(CONFIG_BT_EXT_ADV)
	static const struct bt_le_ext_adv_cb adv_cb = {
		.sent = ext_adv_sent,
	};

	struct bt_le_adv_param adv_param = {
		.id = BT_ID_DEFAULT,
		.interval_min = BT_ADV_SCAN_UNIT(ADV_INT_FAST_MS),
		.interval_max = BT_ADV_SCAN_UNIT(ADV_INT_FAST_MS),
	};

	for (int i = 1; i < CONFIG_BT_EXT_ADV_MAX_ADV_SET; i++) {
		err = bt_le_ext_adv_create(&adv_param, &adv_cb, &advs[i]);
		if (err) {
			return err;
		}
	}
#endif

#if defined(CONFIG_BT_CONN)
	vnd_cmd_attr = &mible_svc.attrs[5];
	write_cmd_handle = bt_gatt_attr_get_handle(vnd_cmd_attr);

	vnd_ntf_attr = notify_attr = &mible_svc.attrs[2];

	bt_conn_cb_register(&conn_cb);
#endif

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

#if defined(CONFIG_BT_EXT_ADV)
		for (int i = 1; i < CONFIG_BT_EXT_ADV_MAX_ADV_SET; i++) {
			if (!atomic_test_bit(advs[i]->flags, BT_ADV_ENABLED)) {
				continue;
			}

			err = bt_le_ext_adv_stop(advs[i]);
			if (err) {
				shell_error(shell, "Unable to stop advertiser (err %d)", err);
			}
		}
#else
		err = bt_le_adv_stop();
		if (err) {
			shell_error(shell, "Unable to stop advertiser (err %d)", err);
		}

		k_work_cancel_delayable(&adv_timeout);
#endif

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

#if !defined(CONFIG_BT_EXT_ADV)
	if (atomic_test_bit(&states, CMD_PERIPHERAL)) {
		if (!atomic_test_bit(&states, CMD_PERIPHERAL_CONN)) {
			shell_error(shell, "Busy peripheral advertising");
			return 0;
		} else if (!force) {
			shell_help(shell);
			return 0;
		}
	}
#endif

	if (atomic_test_and_set_bit(&states, CMD_BROADCAST)) {
		shell_error(shell, "Busy");
		return 0;
	}

#if defined(CONFIG_BT_EXT_ADV)
	struct bt_le_ext_adv_start_param ext_params = {
		.num_events = bd_count,
	};

	for (int i = 1; i < CONFIG_BT_EXT_ADV_MAX_ADV_SET; i++) {
		bd_name[sizeof(bd_name) - 3] = (uint8_t)'0' + ((i & 0xf0) >> 4);
		bd_name[sizeof(bd_name) - 2] = (uint8_t)'0' + (i & 0x0f);

		err = bt_le_ext_adv_set_data(advs[i], ad, ARRAY_SIZE(ad), NULL, 0);
		if (err) {
			shell_error(ctx_shell, "Failed setting adv data: %d", err);
			return err;
		}

		err = bt_le_ext_adv_start(advs[i], &ext_params);
		if (err) {
			shell_error(ctx_shell, "Advertising failed: err %d", err);
		}

		ext_params.num_events += 1;
	}
#else
	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_ADV_SCAN_UNIT(ADV_INT_FAST_MS);
	param.interval_max = param.interval_min;
	param.options = BT_LE_ADV_OPT_USE_IDENTITY;

	err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		shell_error(ctx_shell, "Advertising failed to start (err %d)", err);
		return 0;
	}

	k_work_reschedule(&adv_timeout, K_MSEC((ADV_INT_FAST_MS + 5) * bd_count));
#endif
	shell_print(shell, "Broadcaster started with cycles %d", bd_count);

	return 0;
}

static int cmd_broadcast_id(const struct shell *shell, size_t argc, char *argv[])
{
	uint16_t id;

	id = strtoul(argv[1], NULL, 16);
	bd_name[sizeof(bd_name) - 3] = (uint8_t)'0' + ((id & 0xf0) >> 4);
	bd_name[sizeof(bd_name) - 2] = (uint8_t)'0' + (id & 0x0f);

	shell_print(shell, "Broadcaster id set successfully");

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

	return 0;
}

static void peripheral_notify_cb(struct bt_conn *conn, void *user_data)
{
	const uint16_t *mtu = user_data;

	peripheral_status.tx_octers += *mtu;
	peripheral_status.tx_checksum += crc32_ieee_update(peripheral_status.tx_checksum,
							   vnd_wwr_value, *mtu);
	k_work_reschedule(&peripheral_work, peripheral_throughput_interval);
}

static void peripheral_handler(struct k_work *work)
{
	int err;
	static uint16_t mtu;

	struct bt_gatt_notify_params params = {
		.data = vnd_wwr_value,
		.func = peripheral_notify_cb,
		.user_data = &mtu,
	};

	params.attr = notify_attr;

	if (!peripheral_conn) {
		return;
	}

	mtu = bt_gatt_get_mtu(peripheral_conn) - 4;
	params.len = mtu;

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

		(void)k_work_cancel_delayable(&cmd_per_disc);

		shell_print(shell, "Stoped");

		return 0;
	}

	if ((!IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	     atomic_test_bit(&states, CMD_BROADCAST)) ||
	    atomic_test_and_set_bit(&states, CMD_PERIPHERAL)) {
		shell_error(shell, "Busy");
		return 0;
	}

	bd_name[sizeof(bd_name) - 3] = (uint8_t)'0';
	bd_name[sizeof(bd_name) - 2] = (uint8_t)'0';

	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_ADV_SCAN_UNIT(ADV_INT_SLOW_MS);
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
	char buffer[BT_ADDR_LE_STR_LEN];
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

	bt_addr_le_to_str(addr, buffer, sizeof(buffer));
	shell_print(ctx_shell, "Try to create connection %s", buffer);

	central_status.connecting_count++;
}

static void central_write_cmd_cb(struct bt_conn *conn, void *user_data)
{
	const uint16_t *mtu = user_data;
	central_status.tx_octers += *mtu;
	central_status.tx_checksum += crc32_ieee_update(central_status.tx_checksum,
							vnd_wwr_value, *mtu);
	k_work_reschedule(&central_work, central_throughput_interval);
}

static void central_handler(struct k_work *work)
{
	int err;
	static uint16_t mtu;

	if (!central_conn) {
		return;
	}

	mtu = bt_gatt_get_mtu(central_conn) - 4;

	err = bt_gatt_write_without_response_cb(central_conn, write_cmd_handle,
					        vnd_wwr_value, mtu,
						false, central_write_cmd_cb, &mtu);
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

		(void)k_work_cancel_delayable(&cmd_c_disc);

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
	        shell_print(ctx_shell, "[Central]  CONNING %d CONNED %d RATE %d%% TX %d [Checksum 0x%08x] RX %d [Checksum 0x%08x]",
		            central_status.connecting_count,central_status.connected_count,
			    central_status.disconncted_count > 1 ?
			    (((central_status.reason[BT_HCI_ERR_REMOTE_USER_TERM_CONN] +
			       central_status.reason[BT_HCI_ERR_LOCALHOST_TERM_CONN]) * 100) /
			      (central_status.disconncted_count)) :
			    0,
			    central_status.tx_octers, central_status.tx_checksum,
			    central_status.rx_octers, central_status.rx_checksum);


			for (int i = 0; i < ARRAY_SIZE(central_status.reason); i++) {
				if (!central_status.reason[i]) {
					continue;
				}

				shell_print(ctx_shell, "[Central] Reason 0x%02x Count %d",
					    i, central_status.reason[i]);
			}
        }

        if (atomic_test_bit(&states, CMD_PERIPHERAL)) {
	        shell_print(ctx_shell, "[PERIPHERAL]  CONN %d TX %d [Checksum 0x%08x] RX %d [Checksum 0x%08x]",
		            peripheral_status.connected_count,
			    peripheral_status.tx_octers, peripheral_status.tx_checksum,
			    peripheral_status.rx_octers, peripheral_status.rx_checksum);
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

static void cmd_peri_disc_handler(struct k_work *work)
{
	int err;

	if (atomic_test_bit(&states, CMD_PERIPHERAL_CONN)) {
		shell_print(ctx_shell, "[Periodic] Peripheral disconnecting");

		err = bt_conn_disconnect(peripheral_conn, 0x13);
		if (err) {
			shell_error(ctx_shell, "Unable to disconnect (err %d)", err);
			return;
		}

                atomic_set_bit(&states, CMD_PERIPHERAL_CANCEL_PENDING);
	} else {
		shell_print(ctx_shell, "[Periodic] Peripheral not connected, skiped");
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

static void cmd_cen_disc_handler(struct k_work *work)
{
	int err;

	if (atomic_test_bit(&states, CMD_CENTRAL_CONN)) {
		shell_print(ctx_shell, "[Periodic] Central disconnecting");

		if (IS_ENABLED(CONFIG_BT_GATT_CLIENT)) {
			(void)k_work_cancel_delayable(&central_work);
		}

		err = bt_conn_disconnect(central_conn, 0x13);
		if (err) {
			shell_error(ctx_shell, "Unable to disconnect (err %d)", err);
			return;
		}

                atomic_set_bit(&states, CMD_CENTRAL_CANCEL_PENDING);
	} else {
		shell_print(ctx_shell, "[Periodic] Central not connected, skiped");
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
#if defined(CONFIG_BT_BROADCASTER)
	SHELL_CMD_ARG(broadcast, NULL, "<value on, off> [cycles force]", cmd_broadcast, 2, 2),
	SHELL_CMD_ARG(broadcast_id, NULL, "<id>", cmd_broadcast_id, 2, 0),
#endif
#if defined(CONFIG_BT_OBSERVER)
	SHELL_CMD_ARG(observer, NULL, "<value on, off> [window interval(ms)]", cmd_observer, 2, 2),
#endif
#if defined(CONFIG_BT_PERIPHERAL)
	SHELL_CMD_ARG(peripheral, NULL, "<value on, off>", cmd_peripheral, 2, 0),
	SHELL_CMD_ARG(peripheral_periodic_disconnect, NULL, "periodic(s)", cmd_peripheral_periodic_disconnect, 2, 0),
	SHELL_CMD_ARG(peripheral_id, NULL, "<id>", cmd_peripheral_id, 2, 0),
	SHELL_CMD_ARG(peripheral_throughput, NULL, "<interval(ms)>", cmd_peripheral_throughput, 2, 0),
#endif
#if defined(CONFIG_BT_CENTRAL)
	SHELL_CMD_ARG(central, NULL, "<value on, off>", cmd_central, 2, 0),
	SHELL_CMD_ARG(central_periodic_disconnect, NULL, "periodic(s)", cmd_central_periodic_disconnect, 2, 0),
	SHELL_CMD_ARG(central_target, NULL, "<peer address> [handle]", cmd_central_target, 2, 1),
#if defined(CONFIG_BT_GATT_CLIENT)
	SHELL_CMD_ARG(central_throughput, NULL, "<interval(ms)>", cmd_central_throughput, 2, 0),
#endif
#endif
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
