/* keys_br.c - Bluetooth BR/EDR key handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/settings/settings.h>

#include "common/bt_str.h"

#include "host/hci_core.h"
#include "host/settings.h"
#include "host/keys.h"

#define LOG_LEVEL CONFIG_BT_KEYS_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_keys_br);

/* Port note (2026-10-12, R2): br_key.bin 现在是"写临时文件 + fsync +
 * rename"的原子替换, 记录里带 CRC16, 防止掉电写坏后把 1 个 bond 槽位的
 * 记录读成垃圾(旧实现 O_TRUNC+单次 write, 写一半掉电=全丢)。
 * 记录仍为 32 字节, 向后兼容:
 *   [0]=0xA5 [1]=key_type [2]=flags [3]=rsvd [4..9]=addr [10..25]=key
 *   [26..27]=CRC16(小端, 覆盖 [0..25]) [28]=layout version(1) [29..31]=rsvd
 * 旧文件这两个字节为 0 => 视为 legacy 记录直接接受(不做 CRC 校验), 只在
 * 日志里说明; 新写的记录 CRC 非 0, 校验失败则跳过该记录。 */
static uint16_t bt_keys_link_key_crc16(const uint8_t *p, size_t n)
{
	uint16_t crc = 0xFFFF;

	for (size_t i = 0; i < n; i++) {
		crc ^= (uint16_t)p[i] << 8;
		for (int b = 0; b < 8; b++) {
			crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
					      : (uint16_t)(crc << 1);
		}
	}

	return crc;
}

/* Port note (2026-10-12): /data 是 littlefs, 挂载很晚(~10.6s); 而
 * bluetoothd 可能在其之前就 mkdir /data/misc/bt —— 那次 mkdir 落在
 * rootfs 上, 挂载后被 littlefs 遮住, 结果 open("/data/misc/bt/br_key.bin")
 * 报 ENOENT, 保存/读取全部失败(旧实现无声无息)。这里在每次读写前
 * 自行补建目录(EEXIST 容忍), 让 link key 持久化不依赖启动时序。 */
static void bt_keys_link_key_ensure_dir(void)
{
	(void)mkdir("/data/misc", 0777);
	(void)mkdir("/data/misc/bt", 0777);
}

/* Port note (2026-08-17): load BR link keys from the file written by
 * bt_keys_link_key_store() into the RAM pool, so a reboot no longer
 * forces re-pairing. Returns the key for addr, or NULL. See the store
 * function for the record layout. */
struct bt_keys_link_key *bt_keys_link_key_load_file(struct bt_dev *hdev,
						    const bt_addr_t *addr)
{
	char path[] = "/data/misc/bt/br_key.bin";
	uint8_t rec[32];
	ssize_t got;
	int fd;

	bt_keys_link_key_ensure_dir();
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		/* Port note (2026-10-12): 这里是区分根因的关键日志 ——
		 * 若冷启早期(system up < /data 挂载)就走到这里, fd<0 说明文件
		 * 还不可读(挂载竞态); 若后期仍 fd<0 则是文件缺失/被写坏。 */
		LOG_WRN("br_key load: open %s failed (fd %d)", path, fd);
		return NULL;
	}

	while ((got = read(fd, rec, sizeof(rec))) == sizeof(rec)) {
		struct bt_keys_link_key *slot;

		if (rec[0] != 0xA5) {
			continue;
		}

		/* Port note (2026-10-12, R2): 只有带 layout marker 的记录才做 CRC 校验。
		 * 为什么不能"CRC 非 0 就算新格式": 旧实现只填了前 26 字节就把整个 32B
		 * 结构写进文件, [26..31] 是**未初始化的栈垃圾**, 会被误判成损坏的 CRC
		 * 而把唯一一条有效 bond 丢掉(真机踩到: rec 0x6000 != calc 0xb1ab)。
		 * 因此用 3 字节 marker 区分: 没有 marker 的记录按 legacy 接受并提示,
		 * 它在下次 store 时会被自动升级成带 CRC 的新格式。 */
		if (rec[28] == 1 && rec[29] == 0x5A && rec[30] == 0xA5) {
			uint16_t want = (uint16_t)(rec[26] | ((uint16_t)rec[27] << 8));

			if (want != bt_keys_link_key_crc16(rec, 26)) {
				LOG_WRN("br_key load: bad crc (rec 0x%04x != calc 0x%04x), skip",
					want, bt_keys_link_key_crc16(rec, 26));
				continue;
			}
		} else {
			LOG_WRN("br_key load: legacy record without crc, accepted (will be rewritten)");
		}

		if (!bt_addr_eq((const bt_addr_t *)&rec[4], addr)) {
			continue;
		}

		slot = bt_keys_get_link_key(hdev, addr);
		if (!slot) {
			break;
		}

		bt_addr_copy(&slot->addr, addr);
		slot->key_type = rec[1];
		slot->flags = rec[2];
		memcpy(slot->val, &rec[10], 16);
		close(fd);
		LOG_DBG("br_key load: restored key for %s\n",
		       bt_addr_str(addr));
		return slot;
	}

	close(fd);
	return NULL;
}

struct bt_keys_link_key *bt_keys_find_link_key(struct bt_dev *hdev, const bt_addr_t *addr)
{
	struct bt_keys_link_key *key;
	int i;

	/* Port note (2026-10-12): bt_dev_get() already returns a valid bt_dev
	 * before bt_keys_init() has allocated hdev->keys, so the pool must be
	 * checked as well (the SAL link key restore path can run that early). */
	if (!hdev || !hdev->keys) {
		return NULL;
	}

	LOG_DBG("%s", bt_addr_str(addr));

	for (i = 0; i < ARRAY_SIZE(hdev->keys->br_key_pool); i++) {
		key = &hdev->keys->br_key_pool[i];

		if (bt_addr_eq(&key->addr, addr)) {
			return key;
		}
	}

	return NULL;
}

struct bt_keys_link_key *bt_keys_get_link_key(struct bt_dev *hdev, const bt_addr_t *addr)
{
	struct bt_keys_link_key *key;

	/* Same early-call guard as bt_keys_find_link_key(): the caller must get
	 * NULL instead of dereferencing a pool that does not exist yet. */
	if (!hdev || !hdev->keys) {
		return NULL;
	}

	key = bt_keys_find_link_key(hdev, addr);
	if (key) {
		return key;
	}

	key = bt_keys_find_link_key(hdev, BT_ADDR_ANY);
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (!key) {
		int i;

		key = &hdev->keys->br_key_pool[0];
		for (i = 1; i < ARRAY_SIZE(hdev->keys->br_key_pool); i++) {
			struct bt_keys_link_key *current = &hdev->keys->br_key_pool[i];

			if (current->aging_counter < key->aging_counter) {
				key = current;
			}
		}

		if (key) {
			bt_keys_link_key_clear(hdev, key);
		}
	}
#endif

	if (key) {
		bt_addr_copy(&key->addr, addr);
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
		key->aging_counter = ++hdev->keys->br_aging_counter_val;
		hdev->keys->br_last_keys_updated = key;
#endif
		LOG_DBG("created %p for %s", key, bt_addr_str(addr));
		return key;
	}

	LOG_DBG("unable to create keys for %s", bt_addr_str(addr));

	return NULL;
}

void bt_keys_link_key_clear(struct bt_dev *hdev, struct bt_keys_link_key *link_key)
{
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_addr_le_t le_addr;

		le_addr.type = BT_ADDR_LE_PUBLIC;
		bt_addr_copy(&le_addr.a, &link_key->addr);

		bt_settings_delete_link_key(hdev->dev_id, &le_addr);
	}

	LOG_DBG("%s", bt_addr_str(&link_key->addr));
	(void)memset(link_key, 0, sizeof(*link_key));
}

void bt_keys_link_key_clear_addr(struct bt_dev *hdev, const bt_addr_t *addr)
{
	int i;
	struct bt_keys_link_key *key;

	if (!addr) {
		for (i = 0; i < ARRAY_SIZE(hdev->keys->br_key_pool); i++) {
			key = &hdev->keys->br_key_pool[i];
			bt_keys_link_key_clear(hdev, key);
		}
		/* Port note (2026-10-12): 内存清空后必须让 br_key.bin 同步失效,
		 * 否则手机"忘记设备"后板子重启还会拿旧 key 去应答 -> 反复 auth 失败。
		 * 说明: bt_keys_link_key_store() 的文件分支是整池重写, 与传入的
		 * link_key 无关; 仅当将来打开 CONFIG_BT_SETTINGS 时才需要专门的
		 * 文件专用 helper(那时 settings 分支会写入全 0, 需一并处理)。 */
		bt_keys_link_key_store(hdev, &hdev->keys->br_key_pool[0]);
		return;
	}

	key = bt_keys_find_link_key(hdev, addr);
	if (key) {
		bt_keys_link_key_clear(hdev, key);
		/* 同上: unpair 后同步失效文件记录, 避免陈旧 key 复活 */
		bt_keys_link_key_store(hdev, key);
	}
}

void bt_keys_link_key_store(struct bt_dev *hdev, struct bt_keys_link_key *link_key)
{
	/* Port note (2026-08-17): file-backed BR link key persistence for the
	 * openvela port, which has no settings subsystem. Record layout in
	 * /data/misc/bt/br_key.bin (little endian, fixed 32 bytes/record):
	 *   u8 valid (0xA5) | u8 key_type | u8 flags | u8 rsvd |
	 *   u8 addr[6] | u8 key[16]
	 * The whole file is rewritten on each store (few records, rare event).
	 * Since 2026-10-12 it goes through br_key.bin.tmp + fsync + rename and
	 * carries a CRC16 (see bt_keys_link_key_crc16), so a power cut mid-write
	 * can no longer destroy the stored bond. */
	{
		char path[] = "/data/misc/bt/br_key.bin";
		struct bt_keys_link_key *k;
		uint8_t rec[32];
		uint8_t buf[CONFIG_BT_MAX_PAIRED * 32];
		int fd, n = 0;

		for (int i = 0; i < CONFIG_BT_MAX_PAIRED; i++) {
			k = &hdev->keys->br_key_pool[i];
			if (!bt_addr_eq(&k->addr, BT_ADDR_ANY) || k->val[0] || k->val[15]) {
				uint16_t crc;

				rec[0] = 0xA5;
				rec[1] = k->key_type;
				rec[2] = (uint8_t)k->flags;
				rec[3] = 0;
				memcpy(&rec[4], k->addr.val, 6);
				memcpy(&rec[10], k->val, 16);
				crc = bt_keys_link_key_crc16(rec, 26);
				rec[26] = (uint8_t)(crc & 0xFF);
				rec[27] = (uint8_t)(crc >> 8);
				rec[28] = 1;      /* layout version */
				rec[29] = 0x5A;   /* layout marker: 只有带 marker 的记录才做 CRC 校验 */
				rec[30] = 0xA5;
				rec[31] = 0;
				memcpy(&buf[n * 32], rec, 32);
				n++;
			}
		}

		char tmp[] = "/data/misc/bt/br_key.bin.tmp";

		bt_keys_link_key_ensure_dir();
		/* Port note (2026-10-12, R2): 先写临时文件 + fsync, 再 rename 原子
		 * 覆盖 —— 掉电时要么看到旧文件、要么看到新文件, 不会留下半截记录
		 * (旧实现 O_TRUNC + 单次 write, 写一半掉电就把 bond 全丢了)。 */
		fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC);
		if (fd >= 0) {
			ssize_t wrote = write(fd, buf, n * 32);

			(void)fsync(fd);
			(void)close(fd);
			if (wrote == (ssize_t)(n * 32) && rename(tmp, path) == 0) {
				LOG_DBG("br_key store: %d key(s)\n", n);
			} else {
				LOG_ERR("br_key store: write/rename failed (%d/%d)",
					(int)wrote, errno);
				(void)unlink(tmp);
			}
		} else {
			LOG_ERR("br_key store: open %s failed (%d)\n",
			       tmp, fd);
		}
	}

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		int err;
		bt_addr_le_t le_addr;

		le_addr.type = BT_ADDR_LE_PUBLIC;
		bt_addr_copy(&le_addr.a, &link_key->addr);

		err = bt_settings_store_link_key(hdev->dev_id, &le_addr, link_key->storage_start,
						 BT_KEYS_LINK_KEY_STORAGE_LEN);
		if (err) {
			LOG_ERR("Failed to save link key (err %d)", err);
		}
	}
}

void bt_foreach_bond_br_mc(uint8_t dev_id, void (*func)(const struct bt_bond_info *info, void *user_data),
			void *user_data)
{
	__ASSERT_NO_MSG(func != NULL);
	struct bt_dev *hdev = bt_dev_get(dev_id);

	if (!hdev) {
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(hdev->keys->br_key_pool); i++) {
		struct bt_keys_link_key *key = &hdev->keys->br_key_pool[i];

		if (!bt_addr_eq(&key->addr, BT_ADDR_ANY)) {
			struct bt_bond_info info;

			info.addr.type = BT_ADDR_LE_PUBLIC;
			bt_addr_copy(&info.addr.a, &key->addr);
			func(&info, user_data);
		}
	}
}

#if defined(CONFIG_BT_SETTINGS)

static int link_key_set(const char *name, size_t len_rd,
			settings_read_cb read_cb, void *cb_arg)
{
	int err;
	ssize_t len;
	bt_addr_le_t le_addr;
	struct bt_keys_link_key *link_key;
	char val[BT_KEYS_LINK_KEY_STORAGE_LEN];
	const char *dev_next;
	struct bt_dev *hdev;

	if (!name) {
		LOG_ERR("Insufficient number of arguments");
		return -EINVAL;
	}

	len = read_cb(cb_arg, val, sizeof(val));
	if (len < 0) {
		LOG_ERR("Failed to read value (err %zu)", len);
		return -EINVAL;
	}

	LOG_DBG("name %s val %s", name, len ? bt_hex(val, sizeof(val)) : "(null)");

	err = bt_settings_decode_key(name, &le_addr);
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

	link_key = bt_keys_get_link_key(hdev, &le_addr.a);
	if (len != BT_KEYS_LINK_KEY_STORAGE_LEN) {
		if (link_key) {
			bt_keys_link_key_clear(hdev, link_key);
			LOG_DBG("Clear keys for %s", bt_addr_le_str(&le_addr));
		} else {
			LOG_WRN("Unable to find deleted keys for %s", bt_addr_le_str(&le_addr));
		}

		return 0;
	}

	memcpy(link_key->storage_start, val, len);
	LOG_DBG("Successfully restored link key for %s", bt_addr_le_str(&le_addr));
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (hdev->keys->br_aging_counter_val < link_key->aging_counter) {
		hdev->keys->br_aging_counter_val = link_key->aging_counter;
	}
#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(bt_link_key, "bt/link_key", NULL, link_key_set,
			       NULL, NULL);

#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
void bt_keys_link_key_update_usage(struct bt_dev *hdev, const bt_addr_t *addr)
{
	struct bt_keys_link_key *link_key = bt_keys_find_link_key(hdev, addr);

	if (!link_key) {
		return;
	}

	if (hdev->keys->br_last_keys_updated == link_key) {
		return;
	}

	link_key->aging_counter = ++hdev->keys->br_aging_counter_val;
	hdev->keys->br_last_keys_updated = link_key;

	LOG_DBG("Aging counter for %s is set to %u", bt_addr_str(addr), link_key->aging_counter);

	if (IS_ENABLED(CONFIG_BT_KEYS_SAVE_AGING_COUNTER_ON_PAIRING)) {
		bt_keys_link_key_store(hdev, link_key);
	}
}
#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

#endif /* defined(CONFIG_BT_SETTINGS) */
