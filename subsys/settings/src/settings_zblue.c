/*
 * Copyright (c) 2025 Liuxiang
 * Copyright (c) 2019 Xiaomi Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/settings/settings.h>

#include "settings/settings_zblue.h"
#include <conn_internal.h>
#include <iso_internal.h>

#include <hci_core.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>

#include "settings.h"

LOG_MODULE_DECLARE(settings, CONFIG_SETTINGS_LOG_LEVEL);

static int settings_zblue_load(struct settings_store* cs,
    const struct settings_load_arg* arg);
static int settings_zblue_save(struct settings_store* cs, const char* name,
    const char* value, size_t val_len);

sys_slist_t bt_settings_cbs;

static struct settings_store_itf settings_zblue_itf = {
    .csi_load = settings_zblue_load,
    .csi_save = settings_zblue_save,
};

static struct settings_zblue default_settings_zblue;

int settings_zblue_src(struct settings_zblue* cf)
{
    cf->cf_store.cs_itf = &settings_zblue_itf;
    settings_src_register(&cf->cf_store);

    return 0;
}

int settings_zblue_dst(struct settings_zblue* cf)
{
    cf->cf_store.cs_itf = &settings_zblue_itf;
    settings_dst_register(&cf->cf_store);

    return 0;
}

static ssize_t settings_zblue_read(void* back_end, void* data, size_t len)
{
    struct bt_settings_zblue_cb *listener, *next;
    char* name = (char*)back_end;
    bt_addr_le_t addr;
    int res = 0;

    if (!strncmp(name, "bt/keys/", strlen("bt/keys/"))) {
        bt_settings_decode_key(name + strlen("bt/keys/"), &addr);
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_settings_cbs, listener,
            next, _node)
        {
            if (listener->ltk_load) {
                res = listener->ltk_load(&addr, data, len);
            }
        }
    } else if (!strncmp(name, "bt/link_key/", strlen("bt/link_key/"))) {
        bt_settings_decode_key(name + strlen("bt/link_key/"), &addr);
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_settings_cbs, listener,
            next, _node)
        {
            if (listener->linkkey_load) {
                res = listener->linkkey_load(&addr, data, len);
            }
        }
    } else if (!strncmp(name, "bt/irk/", strlen("bt/irk/"))) {
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_settings_cbs, listener,
            next, _node)
        {
            if (listener->irk_load) {
                res = listener->irk_load(data, len);
            }
        }
    }

    return res;
}

static int settings_zblue_load(struct settings_store* cs,
    const struct settings_load_arg* arg)
{
    int len = 0;

    LOG_DBG("%s", __func__);

    /* load irk */
    settings_call_set_handler(
        "bt/irk/0", len /* unused */,
        settings_zblue_read /* read_cb */, "bt/irk/0" /* read_cb_arg */,
        NULL);
    return 0;
}

int bt_settings_load(uint8_t dev_id, uint8_t id, const char* key, bt_addr_le_t* addr)
{
    int err, len = 0;
    char id_str[4];
    char dev_id_str[4];
    char key_str[BT_SETTINGS_KEY_MAX];

    if (addr) {
        if (id) {
            u8_to_dec(id_str, sizeof(id_str), id);
        }

        u8_to_dec(dev_id_str, sizeof(dev_id_str), dev_id);
        bt_settings_encode_key(key_str, sizeof(key_str), key, addr, (id ? id_str : NULL), dev_id_str);
    } else {
        err = snprintk(key_str, sizeof(key_str), "bt/%s/%d", key, dev_id);
        if (err < 0) {
            return -EINVAL;
        }
    }

    settings_call_set_handler(
        key_str, len /* unused */,
        settings_zblue_read /* read_cb */, key_str /* read_cb_arg */,
        NULL);

    return 0;
}

int bt_settings_commit(uint8_t dev_id, uint8_t id, const char* key, bt_addr_le_t* addr)
{
    int err;
    char id_str[4];
    char dev_id_str[4];
    char key_str[BT_SETTINGS_KEY_MAX];

    if (addr) {
        if (id) {
            u8_to_dec(id_str, sizeof(id_str), id);
        }

        u8_to_dec(dev_id_str, sizeof(dev_id_str), dev_id);
        bt_settings_encode_key(key_str, sizeof(key_str), key, addr, (id ? id_str : NULL), dev_id_str);
    } else {
        err = snprintk(key_str, sizeof(key_str), "bt/%s/%d", key, dev_id);
        if (err < 0) {
            return -EINVAL;
        }
    }

    settings_call_commit_handler(
        key_str,
        NULL);

    return 0;
}

static void parse_settings_key(const char* name, uint8_t* dev_id, uint8_t* id, bt_addr_le_t* addr)
{
    const char *id_next, *dev_next;

    /* parse addr */
    bt_settings_decode_key(name, addr);

    /* parse dev_id(multi-controller) */
    settings_name_next(name, &dev_next);
    *dev_id = strtoul(dev_next, NULL, 10);

    /* parse id(optional) */
    settings_name_next(dev_next, &id_next);
    if (id_next)
        *id = strtoul(id_next, NULL, 10);
    else
        *id = BT_ID_DEFAULT;
}

static int settings_zblue_save(struct settings_store* cs, const char* name,
    const char* value, size_t val_len)
{
    struct bt_settings_zblue_cb *listener, *next;
    bt_addr_le_t addr;
    uint8_t dev_id, id;
    int err = 0;

    LOG_DBG("%s", __func__);

    if (!strncmp(name, "bt/keys/", strlen("bt/keys/"))) {
        parse_settings_key(name + strlen("bt/keys/"), &dev_id, &id, &addr);
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_settings_cbs, listener,
            next, _node)
        {
            if (listener->ltk_notify) {
                err = listener->ltk_notify(dev_id, id, &addr, value, val_len);
            }

            if (err) {
                return err;
            }
        }
    } else if (!strncmp(name, "bt/link_key/", strlen("bt/link_key/"))) {
        parse_settings_key(name + strlen("bt/link_key/"), &dev_id, &id, &addr);
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_settings_cbs, listener,
            next, _node)
        {
            if (listener->linkkey_notify) {
                err = listener->linkkey_notify(dev_id, &addr, value, val_len);
            }

            if (err) {
                return err;
            }
        }
    } else if (!strncmp(name, "bt/irk/", strlen("bt/irk/"))) {
        dev_id = strtoul(name + strlen("bt/irk/"), NULL, 10);
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_settings_cbs, listener,
            next, _node)
        {
            if (listener->irk_notify) {
                err = listener->irk_notify(dev_id, value, val_len);
            }

            if (err) {
                return err;
            }
        }
    }

    return err;
}

int bt_setting_cb_register(struct bt_settings_zblue_cb* cb)
{

    if (sys_slist_find(&bt_settings_cbs, &cb->_node, NULL)) {
        return -EEXIST;
    }

    sys_slist_append(&bt_settings_cbs, &cb->_node);

    return 0;
}

int settings_backend_init(void)
{
    settings_zblue_src(&default_settings_zblue);
    settings_zblue_dst(&default_settings_zblue);
    return 0;
}
