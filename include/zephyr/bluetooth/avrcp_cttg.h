/** @file
 * @brief Advance Audio Remote controller Profile header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_AVRCP_CTTG_H
#define __BT_AVRCP_CTTG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <acts_bluetooth/avrcp.h>


struct bt_avrcp_app_cb {
	void (*connected)(struct bt_conn *conn);
	void (*disconnected)(struct bt_conn *conn);
	void (*notify)(struct bt_conn *conn, uint8_t event_id, uint8_t status);
	void (*pass_ctrl)(struct bt_conn *conn, uint8_t op_id, uint8_t state);
	void (*get_play_status)(struct bt_conn *conn, uint8_t cmd, uint32_t *song_len, uint32_t *song_pos, uint8_t *play_state);
	void (*get_volume)(struct bt_conn *conn, uint8_t *volume);
	void (*update_id3_info)(struct bt_conn *conn, struct id3_info * info);
	void (*playback_pos)(struct bt_conn *conn, uint32_t pos);
};

/* Register avrcp app callback function */
int bt_avrcp_cttg_register_cb(struct bt_avrcp_app_cb *cb);

/* Connect to avrcp TG device */
int bt_avrcp_cttg_connect(struct bt_conn *conn);

/* Disonnect avrcp */
int bt_avrcp_cttg_disconnect(struct bt_conn *conn);

/* Send avrcp control key */
int bt_avrcp_ct_pass_through_cmd(struct bt_conn *conn, avrcp_op_id opid, bool push);

/* Target notify change to controller */
int bt_avrcp_tg_notify_change(struct bt_conn *conn, uint8_t play_state);

/* get current playback track id3 info */
int bt_avrcp_ct_get_id3_info(struct bt_conn *conn);
int bt_avrcp_ct_get_playback_pos(struct bt_conn *conn);
int bt_avrcp_ct_set_absolute_volume(struct bt_conn *conn, uint32_t param);
bool bt_avrcp_ct_check_event_support(struct bt_conn *conn, uint8_t event_id);
int bt_avrcp_ct_get_play_status(struct bt_conn *conn);

int bt_pts_avrcp_ct_get_capabilities(struct bt_conn *conn);
int bt_pts_avrcp_ct_register_notification(struct bt_conn *conn);

#ifdef __cplusplus
}
#endif

#endif /* __BT_AVRCP_CTTG_H */
