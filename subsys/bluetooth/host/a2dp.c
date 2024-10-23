/** @file
 * @brief Advance Audio Distribution Profile.
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

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/avdtp.h>
#include <bluetooth/a2dp.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_A2DP)
#define LOG_MODULE_NAME bt_a2dp
#include "common/log.h"

#include "hci_core.h"
#include "avdtp_internal.h"
#include "conn_internal.h"
#include "l2cap_internal.h"

struct bt_avdtp_conn avdtp_conn[CONFIG_BT_MAX_CONN];

/* A2dp app register call back handler */
static struct bt_a2dp_app_cb *reg_a2dp_app_cb;

static struct bt_avdtp *a2dp_get_new_connection(struct bt_conn *conn)
{
	uint8_t i, free, find, index;
	uint8_t session_priority = 0;
	struct bt_avdtp *session = NULL;

	if (!conn) {
		BT_ERR("Invalid Input (err: %d)", -EINVAL);
		return NULL;
	}

	find = CONFIG_BT_MAX_CONN;
	free = CONFIG_BT_MAX_CONN;

	for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (avdtp_conn[i].signal_session.br_chan.chan.conn == conn) {
			BT_DBG("Conn:%p already connected signal_session: %p", conn, &avdtp_conn[i]);
			session_priority++;
			find = i;
			if (avdtp_conn[i].media_session.br_chan.chan.conn == conn) {
				session_priority++;
			}
			break;
		} else if (!avdtp_conn[i].signal_session.br_chan.chan.conn) {
			if (avdtp_conn[i].media_session.br_chan.chan.conn == conn) {
				BT_ERR("media session exist (err: %d)", -EEXIST);
				return NULL;
			}

			free = i;
		}
	}

	if ((free == CONFIG_BT_MAX_CONN) && (find == CONFIG_BT_MAX_CONN)) {
		BT_DBG("More connection cannot be supported");
		return NULL;
	}

	if (find != CONFIG_BT_MAX_CONN) {
		index = find;
	} else {
		index = free;
	}

	/* Clean the memory area before returning */
	switch (session_priority) {
	case BT_AVDTP_SIGNALING_SESSION:
		memset(&avdtp_conn[index], 0, sizeof(struct bt_avdtp_conn));
		avdtp_conn[index].signal_session.session_priority = session_priority;
		session = &avdtp_conn[index].signal_session;
		break;
	case BT_AVDTP_MEDIA_SESSION:
		avdtp_conn[index].media_session.session_priority = session_priority;
		session = &avdtp_conn[index].media_session;
		break;
	default:
		BT_ERR("Wait TODO!");
		break;
	}

	return session;
}

static struct bt_avdtp_conn *a2dp_lookup_by_conn(struct bt_conn *conn)
{
	uint8_t i;

	if (!conn) {
		return NULL;
	}

	for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (avdtp_conn[i].signal_session.br_chan.chan.conn == conn) {
			return &avdtp_conn[i];
		}
	}

	return NULL;
}

int a2dp_accept(struct bt_conn *conn, struct bt_avdtp **session)
{
	struct bt_avdtp *avdtp_session;

	avdtp_session = a2dp_get_new_connection(conn);
	if (!avdtp_session) {
		return -ENOMEM;
	}

	avdtp_session->intacp_role = BT_AVDTP_ACP;
	*session = avdtp_session;
	BT_DBG("session: %p", avdtp_session);

	return 0;
}

static void a2dp_avdtp_connected_cb(struct bt_avdtp *session)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	if ((session->session_priority == BT_AVDTP_SIGNALING_SESSION) &&
		(session->intacp_role == BT_AVDTP_INT)) {
		/* Only as initiator to trigger discover */
		bt_avdtp_discover(session);
	}

	if (reg_a2dp_app_cb && reg_a2dp_app_cb->connected &&
		(session->session_priority == BT_AVDTP_MEDIA_SESSION)) {
		/* Tell app a2dp connected after media session connected and signal session connected */
		pAvdtp_conn = AVDTP_CONN_BY_MEDIA(session);
		if (pAvdtp_conn->signal_session.connected) {
			reg_a2dp_app_cb->connected(session->br_chan.chan.conn);
		}
	}
}

static void a2dp_avdtp_disconnected_cb(struct bt_avdtp *session)
{
	if (reg_a2dp_app_cb && reg_a2dp_app_cb->disconnected &&
		(session->session_priority == BT_AVDTP_SIGNALING_SESSION) &&
		session->connected) {
		reg_a2dp_app_cb->disconnected(session->br_chan.chan.conn);
	}
}

static void a2dp_avdtp_do_media_connect_cb(struct bt_avdtp *session, bool isconnect)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (isconnect) {
		bt_a2dp_connect(session->br_chan.chan.conn, BT_A2DP_CH_MEDIA);
	} else if (pAvdtp_conn->media_session.connected) {
		bt_avdtp_disconnect(&pAvdtp_conn->media_session);
	}
}

static void a2dp_avdtp_media_handler_cb(struct bt_avdtp *session, struct net_buf *buf)
{
	if (reg_a2dp_app_cb && reg_a2dp_app_cb->media_handler) {
		reg_a2dp_app_cb->media_handler(session->br_chan.chan.conn, buf->data, buf->len);
	}
}

static int a2dp_avdtp_media_state_req_cb(struct bt_avdtp *session, uint8_t sig_id)
{
	if (reg_a2dp_app_cb && reg_a2dp_app_cb->media_state_req) {
		return reg_a2dp_app_cb->media_state_req(session->br_chan.chan.conn, sig_id);
	}

	return 0;
}

static int a2dp_intiator_connect_result_cb(struct bt_avdtp *session, bool success)
{
	if (success) {
		/* As intiator, connect success, do nothing */
	} else {
		/* This session is BT_AVDTP_SIGNALING_SESSION session */
		bt_a2dp_disconnect(session->br_chan.chan.conn);
	}

	return 0;
}

static void a2dp_avdtp_seted_codec_cb(struct bt_avdtp *session,
				struct bt_a2dp_media_codec *codec, uint8_t cp_type)
{
	if (reg_a2dp_app_cb && reg_a2dp_app_cb->seted_codec) {
		reg_a2dp_app_cb->seted_codec(session->br_chan.chan.conn, codec, cp_type);
	}
}

/* The above callback structures need to be packed and passed to AVDTP */
static const struct bt_avdtp_event_cb avdtp_cb = {
	.accept = a2dp_accept,
	.connected = a2dp_avdtp_connected_cb,
	.disconnected = a2dp_avdtp_disconnected_cb,
	.do_media_connect = a2dp_avdtp_do_media_connect_cb,
	.media_handler = a2dp_avdtp_media_handler_cb,
	.media_state_req = a2dp_avdtp_media_state_req_cb,
	.intiator_connect_result = a2dp_intiator_connect_result_cb,
	.seted_codec = a2dp_avdtp_seted_codec_cb,
};

static void bt_a2dp_env_init(void)
{
	memset(avdtp_conn, 0, sizeof(struct bt_avdtp_conn)*CONFIG_BT_MAX_CONN);
	reg_a2dp_app_cb = NULL;
}

int bt_a2dp_init(void)
{
	int err;

	bt_a2dp_env_init();

	/* Register event handlers with AVDTP */
	err = bt_avdtp_register((struct bt_avdtp_event_cb *)&avdtp_cb);
	if (err < 0) {
		BT_ERR("A2DP registration failed");
		return err;
	}

	BT_DBG("A2DP Initialized successfully.");
	return 0;
}

static struct bt_avdtp_conn *a2dp_lookup_by_media_conn(struct bt_conn *conn)
{
	uint8_t i;

	if (!conn) {
		return NULL;
	}

	for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if ((avdtp_conn[i].signal_session.br_chan.chan.conn == NULL) &&
			(avdtp_conn[i].media_session.br_chan.chan.conn == conn)) {
			return &avdtp_conn[i];
		}
	}

	return NULL;
}

static int a2dp_connect_check_conflict(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn = a2dp_lookup_by_media_conn(conn);

	if (!pAvdtp_conn) {
		return 0;
	}

	if ((pAvdtp_conn->signal_session.connected == 0) && (pAvdtp_conn->media_session.connected == 1)) {
		printk("A2dp connect conflict\n");
		bt_avdtp_disconnect(&pAvdtp_conn->media_session);
		return 1;
	}

	if ((pAvdtp_conn->signal_session.connected == 0) && pAvdtp_conn->media_session.connected == 0) {
		printk("media connect pending, wait.\n");
		return 1;
	}

	return 0;
}

int bt_a2dp_connect(struct bt_conn *conn, uint8_t role)
{
	struct bt_avdtp *avdtp_session;
	int err;

	if ((role == BT_A2DP_CH_SOURCE) || (role == BT_A2DP_CH_SINK)) {
		if (a2dp_connect_check_conflict(conn)) {
			BT_INFO("Connect conflict");
			return 0;
		}

		if (a2dp_lookup_by_conn(conn)) {
			BT_INFO("Already connect");
			return 0;
		}
	}

	avdtp_session = a2dp_get_new_connection(conn);
	if (!avdtp_session) {
		BT_ERR("Cannot allocate memory");
		return -EIO;
	}

	err = bt_avdtp_connect(conn, avdtp_session, role);
	if (err < 0) {
		/* If error occurs, undo the saving and return the error */
		memset(avdtp_session, 0, sizeof(struct bt_avdtp));
		BT_DBG("AVDTP Connect failed");
		return err;
	}

	BT_DBG("Connect request sent");
	return 0;
}

int bt_a2dp_disconnect(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn) {
		return -EEXIST;
	}

	if (pAvdtp_conn->media_session.connected) {
		bt_avdtp_disconnect(&pAvdtp_conn->media_session);
	}

	if (pAvdtp_conn->signal_session.connected) {
		bt_avdtp_disconnect(&pAvdtp_conn->signal_session);
	}

	return 0;
}

int bt_a2dp_register_endpoint(struct bt_a2dp_endpoint *endpoint,
			      uint8_t media_type, uint8_t role)
{
	BT_ASSERT(endpoint);

	return bt_avdtp_ep_register_sep(media_type, role, &(endpoint->info));
}

int bt_a2dp_halt_endpoint(struct bt_a2dp_endpoint *endpoint, bool halt)
{
	BT_ASSERT(endpoint);

	return bt_avdtp_ep_halt_sep(&(endpoint->info), halt);
}

int bt_a2dp_register_cb(struct bt_a2dp_app_cb *cb)
{
	if (reg_a2dp_app_cb) {
		BT_WARN("Already register app_cb");
	}

	reg_a2dp_app_cb = cb;
	return 0;
}

int bt_a2dp_start(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn ||
		(pAvdtp_conn->signal_session.connected == 0) ||
		(pAvdtp_conn->media_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_start(&pAvdtp_conn->signal_session);
}

int bt_a2dp_suspend(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn ||
		(pAvdtp_conn->signal_session.connected == 0) ||
		(pAvdtp_conn->media_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_suspend(&pAvdtp_conn->signal_session);
}

int bt_a2dp_reconfig(struct bt_conn *conn, struct bt_a2dp_media_codec *codec)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn ||
		(pAvdtp_conn->signal_session.connected == 0) ||
		(pAvdtp_conn->media_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_reconfig(&pAvdtp_conn->signal_session, codec);
}

int bt_a2dp_send_delay_report(struct bt_conn *conn, uint16_t delay_time)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_delayreport(&pAvdtp_conn->signal_session, delay_time);
}

int bt_a2dp_send_audio_data(struct bt_conn *conn, uint8_t *data, uint16_t len)
{
	struct net_buf *buf;
	struct bt_avdtp_conn *pAvdtp_conn;
	int ret;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->media_session.connected == 0)) {
		return -EIO;
	}

	/* if (len > bt_inner_value.l2cap_tx_mtu) { */
	if (len > BT_L2CAP_TX_MTU) {
		return -EFBIG;
	}

	buf = bt_l2cap_create_pdu(NULL, 0);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_mem(buf, data, len);
	ret = bt_l2cap_chan_send(&pAvdtp_conn->media_session.br_chan.chan, buf);
	if (ret < 0) {
		net_buf_unref(buf);
		return ret;
	} else {
		return (int)len;
	}
}

struct bt_a2dp_media_codec *bt_a2dp_get_seted_codec(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return NULL;
	}

	return bt_avdtp_get_seted_codec(&pAvdtp_conn->signal_session);
}

uint8_t bt_a2dp_get_a2dp_role(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return 0;
	}

	return pAvdtp_conn->signal_session.role;
}

bool bt_a2dp_is_media_rx_channel(uint16_t handle, uint16_t cid)
{
	int i;
	struct bt_conn *conn;

	for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (avdtp_conn[i].signal_session.br_chan.chan.conn &&
			avdtp_conn[i].media_session.br_chan.chan.conn) {
			conn = avdtp_conn[i].media_session.br_chan.chan.conn;
			if ((conn->handle == handle) &&
				(avdtp_conn[i].media_session.br_chan.rx.cid == cid)) {
				return true;
			}
		}
	}

	return false;
}

bool bt_a2dp_is_media_tx_channel(uint16_t handle, uint16_t cid)
{
	int i;
	struct bt_conn *conn;

	for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (avdtp_conn[i].signal_session.br_chan.chan.conn &&
			avdtp_conn[i].media_session.br_chan.chan.conn) {
			conn = avdtp_conn[i].media_session.br_chan.chan.conn;
			if ((conn->handle == handle) &&
				(avdtp_conn[i].media_session.br_chan.tx.cid == cid)) {
				return true;
			}
		}
	}

	return false;
}

uint16_t bt_a2dp_get_a2dp_media_tx_mtu(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->media_session.connected == 0)) {
		return 0;
	}

	//printk("bt_a2dp_get_a2dp_media_tx_mtu pAvdtp_conn->media_session.br_chan.tx.mtu=%d\n", pAvdtp_conn->media_session.br_chan.tx.mtu);
	return pAvdtp_conn->media_session.br_chan.tx.mtu;
}

int bt_a2dp_send_audio_data_with_cb(struct bt_conn *conn, uint8_t *data, uint16_t len,
				    void (*cb)(struct bt_conn *, void *))
{
	struct net_buf *buf;
	struct bt_avdtp_conn *pAvdtp_conn;
	int ret;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->media_session.connected == 0)) {
		return -EIO;
	}

	/* if (len > bt_inner_value.l2cap_tx_mtu) { */
	if (len > BT_L2CAP_TX_MTU) {
		return -EFBIG;
	}

	buf = bt_l2cap_create_pdu(NULL, 0);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_mem(buf, data, len);
	ret = bt_l2cap_chan_send_with_cb(&pAvdtp_conn->media_session.br_chan.chan, buf, cb);
	if (ret < 0) {
		net_buf_unref(buf);
		return ret;
	} else {
		return (int)len;
	}
}

int bt_a2dp_discover(struct bt_conn *conn, uint8_t role)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	BT_INFO("int_state %d acp_state %d\n",
			pAvdtp_conn->stream.int_state,pAvdtp_conn->stream.acp_state);
	if ((pAvdtp_conn->stream.int_state != BT_AVDTP_ACPINT_STATE_IDLE) ||
		(pAvdtp_conn->stream.acp_state != BT_AVDTP_ACPINT_STATE_IDLE)) {
		return -EEXIST;
	}

	pAvdtp_conn->signal_session.intacp_role = BT_AVDTP_INT;
	pAvdtp_conn->signal_session.role = role;

	return bt_avdtp_discover(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_discover(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_discover(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_get_capabilities(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_get_capabilities(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_get_all_capabilities(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_get_all_capabilities(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_set_configuration(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_set_configuration(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_open(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_open(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_close(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_close(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_abort(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_abort(&pAvdtp_conn->signal_session);
}

int bt_pts_a2dp_disconnect_media_session(struct bt_conn *conn)
{
	struct bt_avdtp_conn *pAvdtp_conn;

	pAvdtp_conn = a2dp_lookup_by_conn(conn);
	if (!pAvdtp_conn || (pAvdtp_conn->signal_session.connected == 0)) {
		return -EIO;
	}

	return bt_avdtp_disconnect(&pAvdtp_conn->signal_session);
}
