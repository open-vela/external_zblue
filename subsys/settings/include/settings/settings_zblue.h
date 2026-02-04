/*
 * Copyright (c) 2025 liuxiang
 * Copyright (c) 2025 Xiaomi Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SETTINGS_ZBLUE_H_
#define __SETTINGS_ZBLUE_H_

#include <zephyr/bluetooth/addr.h>
#include <zephyr/settings/settings.h>

#ifdef __cplusplus
extern "C" {
#endif

/* In the zblue backend
 *
 * Application integration requirements:
 * The application must register the callback function bt_settings_zblue_cb to receive:
 *   - keys notification
 *   - keys load
 *
 * About the XX_load callback of bt_settings_zblue_cb:
 *
 * The application must explicitly call bt_settings_load() API to trigger the loading of the
 * setting. This will invoke the settings subsystem (via handlers registered with
 * BT_SETTINGS_DEFINE) to process the value through the corresponding XX_load callback.
 */

struct settings_zblue {
    struct settings_store cf_store;
};

struct settings_zblue_save_fn_arg {
    bt_addr_le_t addr;
    const char* value;
};

struct bt_settings_zblue_cb {
    /** @brief notify local irk.
     *
     *  The callback notifies local irk for RPA parsing and generating.
     *
     *  @param dev_id Device identifier number.
     *  @param key_value Target memory address to which the private key needs to be copied.
     *  @param value_len key_value length.
     */
    int (*irk_notify)(uint8_t dev_id, const char* key_value, uint8_t value_len);

    /** @brief load local irk.
     *
     *  The callback is responsible for retrieving the irk from the application.
     *
     *  @param dev_id Device identifier number.
     *  @param key_value Target memory address to which the private key needs to be copied.
     *  @param value_len key_value length.
     */
    int (*irk_load)(uint8_t* key_value, uint8_t value_len);

#if defined(CONFIG_BT_CLASSIC)
    /** @brief notify that bredr pairing link key generated.
     *
     *  The callback notifies the application that the link key has been
     *  generated during the pairing procedure.
     *
     *  @param dev_id Device identifier number.
     *  @param addr Remote address.
     *  @param key_value Data streams requiring storage.
     *  @param value value length.
     */
    int (*linkkey_notify)(uint8_t dev_id, bt_addr_le_t* addr, const char* key_value, uint8_t value_len);

    /** @brief load BREDR link key.
     *
     *  The callback is responsible for retrieving the link_key data
     *  from the application.
     *
     *  @param addr Remote address.
     *  @param key_value Target memory address to which the private key needs to be copied.
     *  @param value_len key_value length.
     */
    int (*linkkey_load)(bt_addr_le_t* addr, uint8_t* key_value, uint8_t value_len);
#endif

    /** @brief notify that BLE pairing LTK generated.
     *
     *  The callback notifies the application that the LTK has been
     *  generated during the pairing procedure.
     *
     *  @param dev_id Device identifier number.
     *  @param id identifier number.
     *  @param addr Remote LE identity address.
     *  @param key_value Data streams requiring storage.
     *  @param value_len key_value length.
     */
    int (*ltk_notify)(uint8_t dev_id, uint8_t id, bt_addr_le_t* addr, const char* key_value, uint8_t value_len);

    /** @brief load BLE LTK.
     *
     *  The callback is responsible for retrieving the LTK data
     *  from the application.
     *
     *  @param addr Remote identity address.
     *  @param key_value Target memory address to which the private key needs to be copied.
     *  @param value_len key_value length.
     */
    int (*ltk_load)(bt_addr_le_t* addr, uint8_t* key_value, uint8_t value_len);

    /** @internal Internally used field for list handling */
    sys_snode_t _node;
};

/** @brief Register Bluetooth settings callbacks.
 *
 *  Register callbacks to handle Bluetooth settings operations,
 *  such as key notifications and key loading.
 *
 *  @param cb Callback struct. Must point to memory that remains valid.
 *
 * @retval 0 Success.
 * @retval -EEXIST if @p cb was already registered.
 */
int bt_setting_cb_register(struct bt_settings_zblue_cb* cb);

/* register zblue to be a source of settings */
int settings_zblue_src(struct settings_zblue* cf);

/* register zblue to be the destination of settings */
int settings_zblue_dst(struct settings_zblue* cf);

/** @brief Load Bluetooth settings for a device.
 *
 * Loads Bluetooth settings for the specified device and key. This function is typically
 * called to retrieve and apply stored Bluetooth configuration data (such as keys or addresses)
 * for a given device.
 *
 * @param dev_id   Device identifier number.
 * @param id       identifier number.
 * @param key      Key string identifying the setting to load.
 * @param addr     Remote identity address.
 *
 * @return Zero on success, or a negative error code on failure.
 */
int bt_settings_load(uint8_t dev_id, uint8_t id, const char* key, bt_addr_le_t* addr);

/** @brief Commit Bluetooth settings for a device.
 *
 * Commits Bluetooth settings for the specified device and key. This function
 * calls the settings commit handler for the matched settings subtree.
 *
 * @param dev_id   Device identifier number.
 * @param id       identifier number.
 * @param key      Key string identifying the setting to commit.
 * @param addr     Remote identity address.
 *
 * @return Zero on success, or a negative error code on failure.
 */
int bt_settings_commit(uint8_t dev_id, uint8_t id, const char* key, bt_addr_le_t* addr);

/* Initialize a zblue backend. */
int settings_zblue_backend_init(struct settings_zblue* cf);

#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_ZBLUE_H_ */
