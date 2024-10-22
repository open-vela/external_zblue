/** @file
 * @brief Advance Audio Remote controller Profile.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <errno.h>
#include <sys/atomic.h>
#include <sys/byteorder.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <assert.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/avrcp_cttg.h>

#include <acts_bluetooth/sdp.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_AVRCP)
#define LOG_MODULE_NAME bt_avrcp_cttg
#include "common/log.h"

#include "hci_core.h"
#include "conn_internal.h"
#include "avrcp_internal.h"
#include "common_internal.h"

extern struct bt_avrcp avrcp_connection[];

static struct bt_avrcp_app_cb *reg_avrcp_app_cb;

static struct bt_avrcp *avrcp_get_new_connection(struct bt_conn *conn)
{
	uint8_t i;

	if (!conn) {
		BT_ERR("Invalid Input (err: %d)", -EINVAL);
		return NULL;
	}

	for (i = 0; i < bt_inner_value.br_max_conn; i++) {
		if (!avrcp_connection[i].br_chan.chan.conn) {
			memset(&avrcp_connection[i], 0, sizeof(struct bt_avrcp));
			return &avrcp_connection[i];
		}
	}

	BT_DBG("More connection cannot be supported");
	return NULL;
}

static struct bt_avrcp *avrcp_lookup_by_conn(struct bt_conn *conn)
{
	uint8_t i;

	if (!conn) {
		return NULL;
	}

	for (i = 0; i < bt_inner_value.br_max_conn; i++) {
		if (avrcp_connection[i].br_chan.chan.conn == conn) {
			return &avrcp_connection[i];
		}
	}

	return NULL;
}

static int avrcp_accept(struct bt_conn *conn, struct bt_avrcp **session)
{
	struct bt_avrcp *avrc_session;

	if (avrcp_lookup_by_conn(conn)) {
		return -EALREADY;
	}

	avrc_session = avrcp_get_new_connection(conn);
	if (!avrc_session) {
		return -ENOMEM;
	}

	*session = avrc_session;
	BT_DBG("session: %p", avrc_session);

	return 0;
}

static void avrcp_connected_cb(struct bt_avrcp *session)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->connected) {
		reg_avrcp_app_cb->connected(session->br_chan.chan.conn);
	}
}

static void avrcp_disconnected_cb(struct bt_avrcp *session)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->disconnected) {
		reg_avrcp_app_cb->disconnected(session->br_chan.chan.conn);
	}
}

static void avrcp_event_notify_cb(struct bt_avrcp *session, uint8_t event_id, uint8_t status)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->notify) {
		reg_avrcp_app_cb->notify(session->br_chan.chan.conn, event_id, status);
	}
}

static void avrcp_pass_through_ctrl_cb(struct bt_avrcp *session, uint8_t op_id, uint8_t state)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->pass_ctrl) {
		reg_avrcp_app_cb->pass_ctrl(session->br_chan.chan.conn, op_id, state);
	}
}

static void avrcp_get_play_status_cb(struct bt_avrcp *session, uint8_t cmd, uint32_t *song_len,
									uint32_t *song_pos, uint8_t *play_state)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->get_play_status) {
		reg_avrcp_app_cb->get_play_status(session->br_chan.chan.conn, cmd, song_len, song_pos, play_state);
	}
}

static void avrcp_get_volume_cb(struct bt_avrcp *session, uint8_t *volume)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->get_volume) {
		reg_avrcp_app_cb->get_volume(session->br_chan.chan.conn, volume);
	}
}

static void avrcp_update_id3_info(struct bt_avrcp *session, struct id3_info * info)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->update_id3_info) {
		reg_avrcp_app_cb->update_id3_info(session->br_chan.chan.conn, info);
	}
}

static void avrcp_playback_pos(struct bt_avrcp *session, uint32_t pos)
{
	if (reg_avrcp_app_cb && reg_avrcp_app_cb->playback_pos) {
		reg_avrcp_app_cb->playback_pos(session->br_chan.chan.conn, pos);
	}
}

static const struct bt_avrcp_event_cb avrcp_cb = {
	.accept = avrcp_accept,
	.connected = avrcp_connected_cb,
	.disconnected = avrcp_disconnected_cb,
	.notify = avrcp_event_notify_cb,
	.pass_ctrl = avrcp_pass_through_ctrl_cb,
	.get_play_status = avrcp_get_play_status_cb,
	.get_volume = avrcp_get_volume_cb,
	.update_id3_info = avrcp_update_id3_info,
	.playback_pos = avrcp_playback_pos,
};

static void bt_avrcp_cttg_env_init(void)
{
	reg_avrcp_app_cb = NULL;
	memset(avrcp_connection, 0, sizeof(struct bt_avrcp)*bt_inner_value.br_max_conn);
}

int bt_avrcp_cttg_init(void)
{
	int err;

	bt_avrcp_cttg_env_init();

	/* Register event handlers with AVRCP */
	err = bt_avrcp_ctrl_register((struct bt_avrcp_event_cb *)&avrcp_cb);
	if (err < 0) {
		BT_ERR("AVRC controller registration failed");
		return err;
	}

	BT_DBG("AVRC controller Initialized successfully.");
	return 0;
}

int bt_avrcp_ct_pass_through_cmd(struct bt_conn *conn, avrcp_op_id opid, bool push)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		BT_ERR("AVRC not connect!");
		return -EIO;
	}

	return bt_avrcp_pass_through_cmd(session, opid,
				push ? BT_AVRCP_PASS_THROUGH_PUSHED : BT_AVRCP_PASS_THROUGH_RELEASED);
}

/* To do: only support notify volume change */
int bt_avrcp_tg_notify_change(struct bt_conn *conn, uint8_t volume)
{
	uint8_t param = volume;
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		BT_ERR("AVRC not connect!");
		return -EIO;
	}

	return bt_avrcp_notify_change(session, BT_AVRCP_EVENT_VOLUME_CHANGED, &param, 1);
}

int bt_avrcp_ct_get_id3_info(struct bt_conn *conn)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return -EIO;
	}

	return bt_avrcp_get_id3_info(session);
}

int bt_avrcp_ct_get_playback_pos(struct bt_conn *conn)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return -EIO;
	}

	return bt_avrcp_get_playback_pos(session);
}

int bt_avrcp_cttg_connect(struct bt_conn *conn)
{
	struct bt_avrcp *avrcp_conn;
	int err = 0;

	if (avrcp_lookup_by_conn(conn)) {
		BT_INFO("Already connect");
		goto exit_connect;
	}

	avrcp_conn = avrcp_get_new_connection(conn);
	if (!avrcp_conn) {
		BT_ERR("Cannot allocate memory");
		err = -EIO;
		goto exit_connect;
	}

	err = bt_avrcp_connect(conn, avrcp_conn);
	if (err < 0) {
		/* If error occurs, undo the saving and return the error */
		memset(avrcp_conn, 0, sizeof(struct bt_avrcp));
		BT_INFO("AVCRP Connect failed");
	}

exit_connect:
	return err;
}

int bt_avrcp_cttg_disconnect(struct bt_conn *conn)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return -EIO;
	}

	return bt_avrcp_disconnect(session);
}

int bt_avrcp_cttg_register_cb(struct bt_avrcp_app_cb *cb)
{
	if (reg_avrcp_app_cb) {
		BT_WARN("Already register reg_avrcp_app_cb");
	}

	reg_avrcp_app_cb = cb;
	return 0;
}

int bt_pts_avrcp_ct_get_capabilities(struct bt_conn *conn)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return -EIO;
	}

	return bt_avrcp_get_capabilities(session);
}

int bt_avrcp_ct_get_play_status(struct bt_conn *conn)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return -EIO;
	}

	return bt_avrcp_get_play_status(session);
}

int bt_pts_avrcp_ct_register_notification(struct bt_conn *conn)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return -EIO;
	}

	return bt_avrcp_register_notification(session, BT_AVRCP_EVENT_VOLUME_CHANGED);
}

int bt_avrcp_ct_set_absolute_volume(struct bt_conn *conn, uint32_t param)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		BT_ERR("AVRC not connect!");
		return -EIO;
	}

	return bt_avrcp_set_absolute_volume(session, param);
}

bool bt_avrcp_ct_check_event_support(struct bt_conn *conn, uint8_t event_id)
{
	struct bt_avrcp *session = avrcp_lookup_by_conn(conn);

	if (!session) {
		return false;
	}

	return bt_avrcp_check_event_support(session, event_id);
}
