/* keys.c - Bluetooth key handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>

#include "common/bt_str.h"

#include "common/rpa.h"
#include "conn_internal.h"
#include "gatt_internal.h"
#include "hci_core.h"
#include "smp.h"
#include "settings.h"
#include "keys.h"

#define LOG_LEVEL CONFIG_BT_KEYS_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_keys);

#define BT_KEYS_STORAGE_LEN_COMPAT (BT_KEYS_STORAGE_LEN - sizeof(uint32_t))

static struct bt_keys_pool key_pool[CONFIG_BT_NUM_CTLRS];

#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)

struct key_data {
	bool in_use;
	uint8_t id;
};

static void find_key_in_use(struct bt_conn *conn, void *data)
{
	struct key_data *kdata = data;
	struct bt_keys *key;

	__ASSERT_NO_MSG(conn != NULL);
	__ASSERT_NO_MSG(data != NULL);

	if (conn->state == BT_CONN_CONNECTED) {
		key = bt_keys_find_addr(conn->hdev, conn->id, bt_conn_get_dst(conn));
		if (key == NULL) {
			return;
		}

		/* Ensure that the reference returned matches the current pool item */
		if (key == &conn->hdev->keys->key_pool[kdata->id]) {
			kdata->in_use = true;
			LOG_DBG("Connected device %s is using key_pool[%d]",
				bt_addr_le_str(bt_conn_get_dst(conn)), kdata->id);
		}
	}
}

static bool key_is_in_use(uint8_t id)
{
	struct key_data kdata = { false, id };

	bt_conn_foreach(BT_CONN_TYPE_ALL, find_key_in_use, &kdata);

	return kdata.in_use;
}
#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

void bt_keys_reset(struct bt_dev *hdev)
{
	hdev->keys = &key_pool[hdev->dev_id];

	memset(&hdev->keys->key_pool, 0, sizeof(hdev->keys->key_pool));
#if defined(CONFIG_BT_CLASSIC)
	memset(&hdev->keys->br_key_pool, 0, sizeof(hdev->keys->br_key_pool));
#endif
}

struct bt_keys *bt_keys_get_addr(struct bt_dev *hdev, uint8_t id, const bt_addr_le_t *addr)
{
	struct bt_keys *keys;
	int i;
	size_t first_free_slot = ARRAY_SIZE(hdev->keys->key_pool);

	__ASSERT_NO_MSG(addr != NULL);

	LOG_DBG("%s", bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		keys = &hdev->keys->key_pool[i];

		if (keys->id == id && bt_addr_le_eq(&keys->addr, addr)) {
			return keys;
		}
		if (first_free_slot == ARRAY_SIZE(hdev->keys->key_pool) &&
		    bt_addr_le_eq(&keys->addr, BT_ADDR_LE_ANY)) {
			first_free_slot = i;
		}
	}

#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (first_free_slot == ARRAY_SIZE(hdev->keys->key_pool)) {
		struct bt_keys *oldest = NULL;
		bt_addr_le_t oldest_addr;

		for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
			struct bt_keys *current = &hdev->keys->key_pool[i];
			bool key_in_use = key_is_in_use(i);

			if (key_in_use) {
				continue;
			}

			if ((oldest == NULL) || (current->aging_counter < oldest->aging_counter)) {
				oldest = current;
			}
		}

		if (oldest == NULL) {
			LOG_DBG("unable to create keys for %s", bt_addr_le_str(addr));
			return NULL;
		}

		/* Use a copy as bt_unpair will clear the oldest key. */
		bt_addr_le_copy(&oldest_addr, &oldest->addr);
		bt_unpair(oldest->id, &oldest_addr);
		if (bt_addr_le_eq(&oldest->addr, BT_ADDR_LE_ANY)) {
			first_free_slot = oldest - &hdev->keys->key_pool[0];
		}
	}

#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
	if (first_free_slot < ARRAY_SIZE(hdev->keys->key_pool)) {
		keys = &hdev->keys->key_pool[first_free_slot];
		keys->id = id;
		bt_addr_le_copy(&keys->addr, addr);
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
		keys->aging_counter = ++hdev->keys->aging_counter_val;
		hdev->keys->last_keys_updated = keys;
#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
		LOG_DBG("created %p for %s", keys, bt_addr_le_str(addr));
		return keys;
	}

	LOG_DBG("unable to create keys for %s", bt_addr_le_str(addr));

	return NULL;
}

void bt_foreach_bond_mc(uint8_t dev_id, uint8_t id, void (*func)(const struct bt_bond_info *info,
					   void *user_data),
		     void *user_data)
{
	int i;
	struct bt_dev *hdev = bt_dev_get(dev_id);

	__ASSERT_NO_MSG(func != NULL);

	if (!hdev) {
		return;
	}

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		struct bt_keys *keys = &hdev->keys->key_pool[i];

		if (keys->keys && keys->id == id) {
			struct bt_bond_info info;

			bt_addr_le_copy(&info.addr, &keys->addr);
			func(&info, user_data);
		}
	}

	if (IS_ENABLED(CONFIG_BT_CLASSIC)) {
		bt_foreach_bond_br(func, user_data);
	}
}

void bt_keys_foreach_type(struct bt_dev *hdev, enum bt_keys_type type,
			void (*func)(struct bt_keys *keys, void *data), void *data)
{
	int i;

	__ASSERT_NO_MSG(func != NULL);

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		if ((hdev->keys->key_pool[i].keys & type)) {
			func(&hdev->keys->key_pool[i], data);
		}
	}
}

struct bt_keys *bt_keys_find(struct bt_dev *hdev, enum bt_keys_type type,
				uint8_t id, const bt_addr_le_t *addr)
{
	int i;

	__ASSERT_NO_MSG(addr != NULL);

	LOG_DBG("type %d %s", type, bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		if ((hdev->keys->key_pool[i].keys & type) && hdev->keys->key_pool[i].id == id &&
		    bt_addr_le_eq(&hdev->keys->key_pool[i].addr, addr)) {
			return &hdev->keys->key_pool[i];
		}
	}

	return NULL;
}

struct bt_keys *bt_keys_get_type(struct bt_dev *hdev, enum bt_keys_type type,
				uint8_t id, const bt_addr_le_t *addr)
{
	struct bt_keys *keys;

	__ASSERT_NO_MSG(addr != NULL);

	LOG_DBG("type %d %s", type, bt_addr_le_str(addr));

	keys = bt_keys_find(hdev, type, id, addr);
	if (keys) {
		return keys;
	}

	keys = bt_keys_get_addr(hdev, id, addr);
	if (!keys) {
		return NULL;
	}

	bt_keys_add_type(keys, type);

	return keys;
}

struct bt_keys *bt_keys_find_irk_mc(uint8_t dev_id, uint8_t id, const bt_addr_le_t *addr)
{
	int i;
	struct bt_dev *hdev = bt_dev_get(dev_id);

	__ASSERT_NO_MSG(addr != NULL);

	LOG_DBG("%s", bt_addr_le_str(addr));

	if (!hdev) {
		return NULL;
	}

	if (!bt_addr_le_is_rpa(addr)) {
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		if (!(hdev->keys->key_pool[i].keys & BT_KEYS_IRK)) {
			continue;
		}

		if (hdev->keys->key_pool[i].id == id &&
		    bt_addr_eq(&addr->a, &hdev->keys->key_pool[i].irk.rpa)) {
			LOG_DBG("cached RPA %s for %s", bt_addr_str(&hdev->keys->key_pool[i].irk.rpa),
				bt_addr_le_str(&hdev->keys->key_pool[i].addr));
			return &hdev->keys->key_pool[i];
		}
	}

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		if (!(hdev->keys->key_pool[i].keys & BT_KEYS_IRK)) {
			continue;
		}

		if (hdev->keys->key_pool[i].id != id) {
			continue;
		}

		if (bt_rpa_irk_matches(hdev->keys->key_pool[i].irk.val, &addr->a)) {
			LOG_DBG("RPA %s matches %s", bt_addr_str(&hdev->keys->key_pool[i].irk.rpa),
				bt_addr_le_str(&hdev->keys->key_pool[i].addr));

			bt_addr_copy(&hdev->keys->key_pool[i].irk.rpa, &addr->a);

			return &hdev->keys->key_pool[i];
		}
	}

	LOG_DBG("No IRK for %s", bt_addr_le_str(addr));

	return NULL;
}

struct bt_keys *bt_keys_find_addr(struct bt_dev *hdev, uint8_t id, const bt_addr_le_t *addr)
{
	int i;

	__ASSERT_NO_MSG(addr != NULL);

	LOG_DBG("%s", bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(hdev->keys->key_pool); i++) {
		if (hdev->keys->key_pool[i].id == id &&
		    bt_addr_le_eq(&hdev->keys->key_pool[i].addr, addr)) {
			return &hdev->keys->key_pool[i];
		}
	}

	return NULL;
}

void bt_keys_add_type(struct bt_keys *keys, enum bt_keys_type type)
{
	__ASSERT_NO_MSG(keys != NULL);

	keys->keys |= type;
}

void bt_keys_clear(struct bt_dev *hdev, struct bt_keys *keys)
{
	__ASSERT_NO_MSG(keys != NULL);

	LOG_DBG("%s (keys 0x%04x)", bt_addr_le_str(&keys->addr), keys->keys);

	if (keys->state & BT_KEYS_ID_ADDED) {
		bt_id_del(hdev, keys);
	}

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		/* Delete stored keys from flash */
		bt_settings_delete_keys(hdev->dev_id, keys->id, &keys->addr);
	}

	(void)memset(keys, 0, sizeof(*keys));
}

#if defined(CONFIG_BT_SETTINGS)
int bt_keys_store(uint8_t dev_id, struct bt_keys *keys)
{
	int err;

	__ASSERT_NO_MSG(keys != NULL);

	err = bt_settings_store_keys(dev_id, keys->id, &keys->addr, keys->storage_start,
				     BT_KEYS_STORAGE_LEN);
	if (err) {
		LOG_ERR("Failed to save keys (err %d)", err);
		return err;
	}

	LOG_DBG("Stored keys for %s", bt_addr_le_str(&keys->addr));

	return 0;
}

static int keys_set(const char *name, size_t len_rd,
			settings_read_cb read_cb, void *cb_arg)
{
	struct bt_keys *keys;
	bt_addr_le_t addr;
	uint8_t id;
	ssize_t len;
	int err;
	char val[BT_KEYS_STORAGE_LEN];
	const char *next, *dev_next;
	struct bt_dev* hdev;

	if (!name) {
		LOG_ERR("Insufficient number of arguments");
		return -EINVAL;
	}

	len = read_cb(cb_arg, val, sizeof(val));
	if (len < 0) {
		LOG_ERR("Failed to read value (err %zd)", len);
		return -EINVAL;
	}

	LOG_DBG("name %s val %s", name, (len) ? bt_hex(val, sizeof(val)) : "(null)");

	err = bt_settings_decode_key(name, &addr);
	if (err) {
		LOG_ERR("Unable to decode address %s", name);
		return -EINVAL;
	}

	settings_name_next(name, &dev_next);
	unsigned long dev_id = strtoul(dev_next, NULL, 10);
	hdev = bt_dev_get(dev_id);
	if (!hdev) {
		LOG_ERR("Failed to find corresponding bt_dev:%u", dev_id);
		return -ENODEV;
	}

	settings_name_next(dev_next, &next);

	if (!next) {
		id = BT_ID_DEFAULT;
	} else {
		unsigned long next_id = strtoul(next, NULL, 10);

		if (next_id >= CONFIG_BT_ID_MAX) {
			LOG_ERR("Invalid local identity %lu", next_id);
			return -EINVAL;
		}

		id = (uint8_t)next_id;
	}

	if (!len) {
		keys = bt_keys_find(hdev, BT_KEYS_ALL, id, &addr);
		if (keys) {
			(void)memset(keys, 0, sizeof(*keys));
			LOG_DBG("Cleared keys for %s", bt_addr_le_str(&addr));
		} else {
			LOG_WRN("Unable to find deleted keys for %s", bt_addr_le_str(&addr));
		}

		return 0;
	}

	keys = bt_keys_get_addr(hdev, id, &addr);
	if (!keys) {
		LOG_ERR("Failed to allocate keys for %s", bt_addr_le_str(&addr));
		return -ENOMEM;
	}
	if (len != BT_KEYS_STORAGE_LEN) {
		if (IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST) &&
		    len == BT_KEYS_STORAGE_LEN_COMPAT) {
			/* Load shorter structure for compatibility with old
			 * records format with no counter.
			 */
			LOG_WRN("Keys for %s have no aging counter", bt_addr_le_str(&addr));
			memcpy(keys->storage_start, val, len);
		} else {
			LOG_ERR("Invalid key length %zd != %zu", len, BT_KEYS_STORAGE_LEN);
			bt_keys_clear(hdev, keys);

			return -EINVAL;
		}
	} else {
		memcpy(keys->storage_start, val, len);
	}

	LOG_DBG("Successfully restored keys for %s", bt_addr_le_str(&addr));
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (hdev->keys->aging_counter_val < keys->aging_counter) {
		hdev->keys->aging_counter_val = keys->aging_counter;
	}
#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
	return 0;
}

static void id_add(struct bt_keys *keys, void *user_data)
{
	struct bt_dev* hdev = user_data;

	__ASSERT_NO_MSG(keys != NULL);

	bt_id_add(hdev, keys);
}

static int keys_commit(void)
{
	struct bt_dev* hdev;
	uint8_t dev_id;

	for (dev_id = 0; dev_id < CONFIG_BT_NUM_CTLRS; dev_id++) {
		hdev = bt_dev_get(dev_id);

		if (!hdev) {
			continue;
		}
		/* We do this in commit() rather than add() since add() may get
		* called multiple times for the same address, especially if
		* the keys were already removed.
		*/
		if (IS_ENABLED(CONFIG_BT_CENTRAL) && IS_ENABLED(CONFIG_BT_PRIVACY)) {
			bt_keys_foreach_type(hdev, BT_KEYS_ALL, id_add, hdev);
		} else {
			bt_keys_foreach_type(hdev, BT_KEYS_IRK, id_add, hdev);
		}
	}

	return 0;
}

BT_SETTINGS_DEFINE(keys, "keys", keys_set, keys_commit);

#endif /* CONFIG_BT_SETTINGS */

#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
void bt_keys_update_usage(struct bt_dev *hdev, uint8_t id, const bt_addr_le_t *addr)
{
	__ASSERT_NO_MSG(addr != NULL);

	struct bt_keys *keys = bt_keys_find_addr(hdev, id, addr);

	if (!keys) {
		return;
	}

	if (hdev->keys->last_keys_updated == keys) {
		return;
	}

	keys->aging_counter = ++hdev->keys->aging_counter_val;
	hdev->keys->last_keys_updated = keys;

	LOG_DBG("Aging counter for %s is set to %u", bt_addr_le_str(addr), keys->aging_counter);

	if (IS_ENABLED(CONFIG_BT_KEYS_SAVE_AGING_COUNTER_ON_PAIRING)) {
		bt_keys_store(hdev->dev_id, keys);
	}
}

#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

#if defined(CONFIG_BT_LOG_SNIFFER_INFO)
void bt_keys_show_sniffer_info(struct bt_keys *keys, void *data)
{
	uint8_t ltk[16];

	__ASSERT_NO_MSG(keys != NULL);

	if (keys->keys & BT_KEYS_LTK_P256) {
		sys_memcpy_swap(ltk, keys->ltk.val, keys->enc_size);
		LOG_INF("SC LTK: 0x%s", bt_hex(ltk, keys->enc_size));
	}

#if !defined(CONFIG_BT_SMP_SC_PAIR_ONLY)
	if (keys->keys & BT_KEYS_PERIPH_LTK) {
		sys_memcpy_swap(ltk, keys->periph_ltk.val, keys->enc_size);
		LOG_INF("Legacy LTK: 0x%s (peripheral)", bt_hex(ltk, keys->enc_size));
	}
#endif /* !CONFIG_BT_SMP_SC_PAIR_ONLY */

	if (keys->keys & BT_KEYS_LTK) {
		sys_memcpy_swap(ltk, keys->ltk.val, keys->enc_size);
		LOG_INF("Legacy LTK: 0x%s (central)", bt_hex(ltk, keys->enc_size));
	}
}
#endif /* defined(CONFIG_BT_LOG_SNIFFER_INFO) */

#ifdef ZTEST_UNITTEST
struct bt_keys *bt_keys_get_hdev->keys->key_pool(struct bt_dev *hdev)
{
	return hdev->keys->key_pool;
}

#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
uint32_t bt_keys_get_aging_counter_val(struct bt_dev *hdev)
{
	return hdev->keys->aging_counter_val;
}

struct bt_keys *bt_keys_get_last_keys_updated(struct bt_dev *hdev)
{
	return hdev->keys->last_keys_updated;
}
#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
#endif /* ZTEST_UNITTEST */
