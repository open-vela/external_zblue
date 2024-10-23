/*
 * Audio Video Distribution Protocol
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <zephyr.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/atomic.h>
#include <sys/byteorder.h>
#include <sys/util.h>

#include <bluetooth/a2dp-codec.h>
#include <bluetooth/avdtp.h>
#include <bluetooth/hci.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_AVDTP)
#define LOG_MODULE_NAME bt_avdtp
#include "common/log.h"

#include "hci_core.h"
#include "avdtp_internal.h"
#include "conn_internal.h"
#include "l2cap_internal.h"

#define AVDTP_DEBUG_LOG		1
#if AVDTP_DEBUG_LOG
#define avdtp_log(fmt, ...) \
		do {	\
			printk(fmt, ##__VA_ARGS__);	\
		} while (0)
#else
#define avdtp_log(fmt, ...)
#endif

#define AVDTP_TIMEOUT K_SECONDS(5)

#define AVDTP_MSG_POISTION 0x00
#define AVDTP_PKT_POSITION 0x02
#define AVDTP_TID_POSITION 0x04
#define AVDTP_SIGID_MASK 0x3f

#define AVDTP_GET_TR_ID(hdr) ((hdr & 0xf0) >> AVDTP_TID_POSITION)
#define AVDTP_GET_MSG_TYPE(hdr) (hdr & 0x03)
#define AVDTP_GET_PKT_TYPE(hdr) ((hdr & 0x0c) >> AVDTP_PKT_POSITION)
#define AVDTP_GET_SIG_ID(s) (s & AVDTP_SIGID_MASK)

#define AVDTP_CHAN(_ch) CONTAINER_OF(_ch, struct bt_avdtp, br_chan.chan)

static struct bt_avdtp_event_cb *event_cb;
static struct net_buf *avdtp_create_pdu(uint8_t msg_type,
					uint8_t pkt_type, uint8_t sig_id, uint8_t rxtid, uint8_t *cmdtid);
static int avdtp_send(struct bt_avdtp *session, struct net_buf *buf);
static int bt_avdtp_send_timeout_handler(struct bt_avdtp *session,
					struct bt_avdtp_req *req);
static int bt_avdtp_state_sm(struct bt_avdtp *session, struct bt_avdtp_req *req);

/* Capabilities service length */
static const uint8_t avdtp_cap_svr_len[] = {
	0xFF,	/* Not used */
	0x00,	/* BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT */
	0x00,	/* BT_AVDTP_SERVICE_CAT_REPORTING */
	0x03,	/* BT_AVDTP_SERVICE_CAT_RECOVERY */
	0xFF,	/* BT_AVDTP_SERVICE_CAT_CONTENT_PROTECTION, length depend on service */
	0x01,	/* BT_AVDTP_SERVICE_CAT_HDR_COMPRESSION */
	0xFF,	/* BT_AVDTP_SERVICE_CAT_MULTIPLEXING, length depend on service */
	0xFF,	/* BT_AVDTP_SERVICE_CAT_MEDIA_CODEC,  length depend on service */
	0x00	/* BT_AVDTP_SERVICE_CAT_DELAYREPORTING */
};

static int avdtp_check_capabilities(struct net_buf *buf,
							uint8_t sig_id, struct bt_avdtp_conf_rej *rej)
{
	uint8_t len;
	struct bt_avdtp_cap *cap = (void *)buf->data;

	for (len = 0; len < buf->len; ) {
		if (cap->cat == 0 || cap->cat > BT_AVDTP_SERVICE_CAT_MAX) {
			rej->category = cap->cat;
			rej->error = BT_AVDTP_ERR_BAD_SERV_CATEGORY;
			return -EINVAL;
		}

		if (sig_id == BT_AVDTP_RECONFIGURE && cap->cat == BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT) {
			rej->category = cap->cat;
			rej->error = BT_AVDTP_ERR_INVALID_CAPABILITIES;
			return -EINVAL;
		}

		if ((avdtp_cap_svr_len[cap->cat] != 0xFF) &&
			(avdtp_cap_svr_len[cap->cat] != cap->len)) {
			rej->category = cap->cat;
			switch (cap->cat) {
			case BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT:
				rej->error = BT_AVDTP_ERR_BAD_MEDIA_TRANSPORT_FORMAT;
				break;
			case BT_AVDTP_SERVICE_CAT_RECOVERY:
				rej->error = BT_AVDTP_ERR_BAD_RECOVERY_FORMAT;
				break;
			case BT_AVDTP_SERVICE_CAT_MULTIPLEXING:
				rej->error = BT_AVDTP_ERR_BAD_MULTIPLEXING_FORMAT;
				break;
			default:
				rej->error = BT_AVDTP_ERR_BAD_ROHC_FORMAT;
				break;
			}
			return -EINVAL;
		}

		len += cap->len + 2;
		cap = (void *)&buf->data[len];
	}

	if (len != buf->len) {
		rej->category = 0;
		rej->error = BT_AVDTP_ERR_BAD_LENGTH;
		return -EINVAL;
	}

	return 0;
}

static int avdtp_send_accept_resp(struct bt_avdtp *session,
							uint8_t sig_id, uint8_t rxtid)
{
	struct net_buf *buf;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	buf = avdtp_create_pdu(BT_AVDTP_ACCEPT,
					BT_AVDTP_PACKET_TYPE_SINGLE,
					sig_id, rxtid, &pAvdtp_conn->req.cmdtid);
	if (!buf) {
		return -ENOMEM;
	}

	return avdtp_send(session, buf);
}

static int avdtp_check_cmd_format(struct bt_avdtp *session,
							struct net_buf *buf, uint8_t sig_id, uint8_t rxtid, uint8_t *reqSeid)
{
	struct net_buf *resp_buf;
	uint8_t seid, tmp;
	uint8_t error_code = BT_AVDTP_SUCCESS;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	/* Check length */
	switch (sig_id) {
	case BT_AVDTP_DISCOVER:
		if (buf->len == 0) {
			return 0;
		}
		error_code = BT_AVDTP_ERR_BAD_LENGTH;
		break;
	case BT_AVDTP_GET_CAPABILITIES:
	case BT_AVDTP_GET_ALL_CAPABILITIES:
	case BT_AVDTP_GET_CONFIGURATION:
	case BT_AVDTP_OPEN:
	case BT_AVDTP_START:
	case BT_AVDTP_CLOSE:
	case BT_AVDTP_SUSPEND:
		if (buf->len != 1) {
			error_code = BT_AVDTP_ERR_BAD_LENGTH;
		}
		break;
	case BT_AVDTP_SET_CONFIGURATION:
	case BT_AVDTP_RECONFIGURE:
		if (buf->len < 2) {
			error_code = BT_AVDTP_ERR_BAD_LENGTH;
		}
		break;
	case BT_AVDTP_ABORT:
		/* ABORT: no response shall be sent. */
		return 0;
	}

	seid = buf->data[0] >> 2;
	if (error_code != BT_AVDTP_SUCCESS) {
		goto send_reject;
	}

	/* Check acp seid */
	switch (sig_id) {
	case BT_AVDTP_GET_CAPABILITIES:
	case BT_AVDTP_GET_ALL_CAPABILITIES:
	case BT_AVDTP_SET_CONFIGURATION:
	case BT_AVDTP_RECONFIGURE:
	case BT_AVDTP_OPEN:
	case BT_AVDTP_START:
	case BT_AVDTP_CLOSE:
	case BT_AVDTP_SUSPEND:
		if (!find_lsep_by_seid(seid)) {
			error_code = BT_AVDTP_ERR_BAD_ACP_SEID;
		}
		break;
	default:
		break;
	}

	if (error_code != BT_AVDTP_SUCCESS) {
		goto send_reject;
	}

	/* Check if command can be doing at current state */
	switch (sig_id) {
	case BT_AVDTP_SET_CONFIGURATION:
		if (lsep_seid_inused(seid)) {
			error_code = BT_AVDTP_ERR_SEP_IN_USE;
		}
		break;
	case BT_AVDTP_OPEN:
		if (pAvdtp_conn->stream.stream_state != BT_AVDTP_STREAM_STATE_CONFIGURED) {
			error_code = BT_AVDTP_ERR_BAD_STATE;
		}
		break;
	case BT_AVDTP_START:
		if (!pAvdtp_conn->media_session.connected) {
			pAvdtp_conn->pending_ahead_start = 1;
		} else if (!(pAvdtp_conn->stream.stream_state == BT_AVDTP_STREAM_STATE_OPEN ||
			pAvdtp_conn->stream.stream_state == BT_AVDTP_STREAM_STATE_SUSPEND)) {
			error_code = BT_AVDTP_ERR_BAD_STATE;
		}
		break;
	case BT_AVDTP_SUSPEND:
		if (pAvdtp_conn->stream.stream_state != BT_AVDTP_STREAM_STATE_STREAMING) {
			error_code = BT_AVDTP_ERR_BAD_STATE;
		}
		break;
	default:
		break;
	}

	if (error_code == BT_AVDTP_SUCCESS) {
		if (reqSeid) {
			*reqSeid = seid;
		}
		return 0;
	}

send_reject:
	resp_buf = avdtp_create_pdu(BT_AVDTP_REJECT,
						    BT_AVDTP_PACKET_TYPE_SINGLE,
						    sig_id, rxtid, &pAvdtp_conn->req.cmdtid);
	if (!resp_buf) {
		return -ENOMEM;
	}

	seid = seid << 2;
	switch (sig_id) {
	case BT_AVDTP_SET_CONFIGURATION:
	case BT_AVDTP_RECONFIGURE:
		tmp = 0;		/* Service Category */
		net_buf_add_mem(resp_buf, &tmp, sizeof(tmp));
		break;
	case BT_AVDTP_START:
	case BT_AVDTP_SUSPEND:
		net_buf_add_mem(resp_buf, &seid, sizeof(seid));
		break;
	case BT_AVDTP_OPEN:
	case BT_AVDTP_CLOSE:
	default:
		break;
	}

	net_buf_add_mem(resp_buf, &error_code, sizeof(error_code));
	avdtp_send(session, resp_buf);
	return -error_code;
}

static void avdtp_discover_cmd_handle(struct bt_avdtp *session,
				struct net_buf *buf, uint8_t rxtid)
{
	struct net_buf *resp_buf;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	resp_buf = avdtp_create_pdu((bt_avdtp_ep_empty() ? BT_AVDTP_REJECT : BT_AVDTP_ACCEPT),
					BT_AVDTP_PACKET_TYPE_SINGLE, BT_AVDTP_DISCOVER,
					rxtid, &pAvdtp_conn->req.cmdtid);
	if (!resp_buf) {
		return;
	}

	bt_avdtp_ep_append_seid(resp_buf);
	avdtp_send(session, resp_buf);
	pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_DISCOVERED;
}

static void avdtp_discover_resp_handle(struct bt_avdtp *session,
				struct net_buf *buf, uint8_t msg_type)
{
	struct bt_avdtp_seid_info *seid;
	uint8_t i, add_flag;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (msg_type != BT_AVDTP_ACCEPT) {
		return;
	}

	pAvdtp_conn->get_seid_num = 0;
	pAvdtp_conn->get_rsid_cap_index = 0;
	memset(pAvdtp_conn->get_seid, 0, sizeof(pAvdtp_conn->get_seid));

	for (i = 0; i < buf->len; i += 2) {
		/* Is better GET_ALL_CAPABILITIES and select that right seid  */
		if ((session->role == BT_A2DP_CH_SINK) && find_free_lsep_by_role_codectype(BT_A2DP_EP_SINK, BT_A2DP_MPEG2)) {
			/* Get from last seid, most phone aac in last */
			seid = (struct bt_avdtp_seid_info *)&buf->data[buf->len - 2 - i];
		} else {
			/* Get from first seid, most phone sbc in first */
			seid = (struct bt_avdtp_seid_info *)&buf->data[i];
		}

		add_flag = 0;
		if ((seid->media_type == BT_AVDTP_MEDIA_TYPE_AUDIO) && (!seid->inuse)) {
			if ((session->role == BT_A2DP_CH_SOURCE) && (seid->tsep == BT_A2DP_EP_SINK) &&
				find_free_lsep_by_role(BT_A2DP_EP_SOURCE)) {
				add_flag = 1;
			} else if ((session->role == BT_A2DP_CH_SINK) && (seid->tsep == BT_A2DP_EP_SOURCE) &&
				find_free_lsep_by_role(BT_A2DP_EP_SINK)) {
				add_flag = 1;
			} else if ((session->role == BT_A2DP_CH_UNKOWN) && (seid->tsep == BT_A2DP_EP_SINK) &&
						find_free_lsep_by_role(BT_A2DP_EP_SOURCE)) {
				add_flag = 1;
			} else if ((session->role == BT_A2DP_CH_UNKOWN) && (seid->tsep == BT_A2DP_EP_SOURCE) &&
						find_free_lsep_by_role(BT_A2DP_EP_SINK)) {
				add_flag = 1;
			}
		}

		if (add_flag) {
			if (pAvdtp_conn->get_seid_num < BT_AVDTP_GET_SEID_MAX) {
				memcpy(&pAvdtp_conn->get_seid[pAvdtp_conn->get_seid_num], seid, sizeof(struct bt_avdtp_seid_info));
				pAvdtp_conn->get_seid_num++;
			} else {
				avdtp_log("avdtp cache get seid full!\n");
			}
		}
	}

	if (pAvdtp_conn->get_seid_num == 0) {
		return;
	}

	pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_DISCOVERED;
}

static void avdtp_discover_handle(struct bt_avdtp *session, struct net_buf *buf,
				  uint8_t msg_type, uint8_t rxtid)
{
	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		avdtp_discover_cmd_handle(session, buf, rxtid);
	} else {
		avdtp_discover_resp_handle(session, buf, msg_type);
	}
}

static int avdtp_get_capabilities_cmd_handle(struct bt_avdtp *session,
					      struct net_buf *buf, uint8_t sig_id, uint8_t rxtid)
{
	struct net_buf *resp_buf = NULL;
	uint8_t reqSeid;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (avdtp_check_cmd_format(session, buf, sig_id, rxtid, &reqSeid)) {
		return -EINVAL;
	}

	resp_buf = avdtp_create_pdu(BT_AVDTP_ACCEPT,
					BT_AVDTP_PACKET_TYPE_SINGLE,
					sig_id, rxtid, &pAvdtp_conn->req.cmdtid);
	if (!resp_buf) {
		return -ENOMEM;
	}

	bt_avdtp_ep_append_capabilities(resp_buf, reqSeid);
	avdtp_send(session, resp_buf);
	pAvdtp_conn->stream.acp_state = BT_AVDTP_SIG_ID_TO_STATE_ED(sig_id);
	return 0;
}

static void avdtp_get_capabilities_resp_handle(struct bt_avdtp *session,
					      struct net_buf *buf, uint8_t msg_type, uint8_t sig_id)
{
	int ret;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (msg_type != BT_AVDTP_ACCEPT) {
		/* Reject: what todo? */
		return;
	}

	ret = bt_avdtp_ep_check_set_codec_cp(session, buf, 0, sig_id);
	if (!ret) {
		/* Get capabilities success */
		pAvdtp_conn->stream.int_state = BT_AVDTP_SIG_ID_TO_STATE_ED(sig_id);
		pAvdtp_conn->get_rsid_cap_index = 0;
	} else {
		/* Rsid cap is not the right one, try next index rsid.
		 * For same device, first source rsid id not sbc(maybe not sbc/aac),
		 * it will failed, so need try next rsid.
		 */
		pAvdtp_conn->stream.int_state = BT_AVDTP_SIG_ID_TO_STATE_EXT(sig_id);
	}
}

static void avdtp_get_capabilities_handle(struct bt_avdtp *session,
					      struct net_buf *buf, uint8_t msg_type, uint8_t rxtid)
{
	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		avdtp_get_capabilities_cmd_handle(session, buf,
						BT_AVDTP_GET_CAPABILITIES, rxtid);
	} else {
		avdtp_get_capabilities_resp_handle(session, buf, msg_type, BT_AVDTP_GET_CAPABILITIES);
	}
}

static void avdtp_get_all_capabilities_handle(struct bt_avdtp *session,
					      struct net_buf *buf, uint8_t msg_type, uint8_t rxtid)
{
	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		avdtp_get_capabilities_cmd_handle(session, buf,
					BT_AVDTP_GET_ALL_CAPABILITIES, rxtid);
	} else {
		avdtp_get_capabilities_resp_handle(session, buf, msg_type, BT_AVDTP_GET_ALL_CAPABILITIES);
	}
}

static uint8_t a2dp_test_pts_err_code = 0xFF;

void bt_pts_a2dp_set_err_code(uint8_t err_code)
{
	a2dp_test_pts_err_code = err_code;
}

static int avdtp_setreset_configuration_cmd_handle(struct bt_avdtp *session,
					struct net_buf *buf, uint8_t rxtid, uint8_t *int_seid, uint8_t *acp_seid, uint8_t sig_id)
{
	int ret = 0;
	struct bt_avdtp_setconf_req *req = (void *)buf->data;
	struct bt_avdtp_reconf_req *req_reconf = (void *)buf->data;
	struct net_buf *resp_buf;
	struct bt_avdtp_conf_rej rej = {0, BT_AVDTP_SUCCESS};
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("");

	if (avdtp_check_cmd_format(session, buf, sig_id, rxtid, NULL)) {
		return -EINVAL;
	}

	if (sig_id == BT_AVDTP_SET_CONFIGURATION) {
		*int_seid = req->int_seid;
		*acp_seid = req->acp_seid;
		net_buf_pull(buf, sizeof(*req));
	} else {
		*acp_seid = req_reconf->acp_seid;
		net_buf_pull(buf, sizeof(*req_reconf));
	}

	/* check services */
	if (avdtp_check_capabilities(buf, sig_id, &rej)) {
		goto out;
	}

	ret = bt_avdtp_ep_check_set_codec_cp(session, buf, *acp_seid, sig_id);
	if (ret) {
		rej.category = BT_AVDTP_SERVICE_CAT_MEDIA_CODEC;
		rej.error = (uint8_t)(-ret);
		goto out;
	}

out:
	if (rej.error == BT_AVDTP_SUCCESS) {
		resp_buf = avdtp_create_pdu(BT_AVDTP_ACCEPT,
					    BT_AVDTP_PACKET_TYPE_SINGLE,
					    sig_id, rxtid, &pAvdtp_conn->req.cmdtid);
		if (!resp_buf) {
			return -ENOMEM;
		} else {
			ret = 0;
		}
	} else {
		resp_buf = avdtp_create_pdu(BT_AVDTP_REJECT,
					    BT_AVDTP_PACKET_TYPE_SINGLE,
					    sig_id, rxtid, &pAvdtp_conn->req.cmdtid);
		if (!resp_buf) {
			return -ENOMEM;
		} else {
			net_buf_add_mem(resp_buf, &rej, sizeof(struct bt_avdtp_conf_rej));
			ret = -rej.error;
		}
	}

	avdtp_send(session, resp_buf);
	return ret;
}

static void avdtp_set_configuration_handle(struct bt_avdtp *session,
				    struct net_buf *buf, uint8_t msg_type, uint8_t rxtid)
{
	uint8_t acp_seid, int_seid;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_setreset_configuration_cmd_handle(session, buf, rxtid, &int_seid,
							&acp_seid, BT_AVDTP_SET_CONFIGURATION) == 0) {
			lsep_set_seid_used_by_seid(acp_seid, &pAvdtp_conn->stream);
			if (pAvdtp_conn->stream.lsid.tsep == BT_A2DP_EP_SOURCE) {
				session->role = BT_A2DP_CH_SOURCE;
				pAvdtp_conn->stream.rsid.tsep = BT_A2DP_EP_SINK;
				pAvdtp_conn->stream.rsid.id = int_seid;
			} else if (pAvdtp_conn->stream.lsid.tsep == BT_A2DP_EP_SINK) {
				session->role = BT_A2DP_CH_SINK;
				pAvdtp_conn->stream.rsid.tsep = BT_A2DP_CH_SOURCE;
				pAvdtp_conn->stream.rsid.id = int_seid;
			}

			pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_CONFIGURED;
			pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_SET_CFGED;

			event_cb->seted_codec(session, &pAvdtp_conn->stream.codec, pAvdtp_conn->stream.cp_type);
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		/* As INT, session->role is set in start connect */
		lsep_set_seid_used_by_stream(&pAvdtp_conn->stream);
		pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_CONFIGURED;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_SET_CFGED;

		event_cb->seted_codec(session, &pAvdtp_conn->stream.codec, pAvdtp_conn->stream.cp_type);
	} else {
		/* Reject: what todo? */
	}
}

static void avdtp_reconfigure_handle(struct bt_avdtp *session,
				    struct net_buf *buf, uint8_t msg_type, uint8_t rxtid)
{
	uint8_t acp_seid;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_setreset_configuration_cmd_handle(session, buf, rxtid, 0,
							&acp_seid, BT_AVDTP_RECONFIGURE) == 0) {
			event_cb->seted_codec(session, &pAvdtp_conn->stream.codec, pAvdtp_conn->stream.cp_type);
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_RECFGED;
		event_cb->seted_codec(session, &pAvdtp_conn->stream.codec, pAvdtp_conn->stream.cp_type);
	} else {
		/* Reject: what todo? */
	}
}

static void avdtp_open_handle(struct bt_avdtp *session, struct net_buf *buf,
			      uint8_t msg_type, uint8_t rxtid)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_check_cmd_format(session, buf, BT_AVDTP_OPEN, rxtid, NULL)) {
			avdtp_log("avdtp_check_cmd_format failed\n");
			return;
		}

		if (avdtp_send_accept_resp(session, BT_AVDTP_OPEN, rxtid) > 0) {
			pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_OPEN;
			pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_OPENED;
			event_cb->media_state_req(session, BT_AVDTP_OPEN);
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_OPEN;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_OPENED;
		event_cb->media_state_req(session, BT_AVDTP_OPEN);
	} else {
		/* Reject: what todo? */
	}
}

static void avdtp_start_handle(struct bt_avdtp *session, struct net_buf *buf,
			       uint8_t msg_type, uint8_t rxtid)
{
	struct net_buf *resp_buf;
	uint8_t resp_msg, error_code;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_check_cmd_format(session, buf, BT_AVDTP_START, rxtid, NULL)) {
			return;
		}

		if (event_cb->media_state_req(session, BT_AVDTP_START) == 0) {
			resp_msg = BT_AVDTP_ACCEPT;
		} else {
			resp_msg = BT_AVDTP_REJECT;
			error_code = BT_AVDTP_ERR_BAD_STATE;
		}

		resp_buf = avdtp_create_pdu(resp_msg,
			    BT_AVDTP_PACKET_TYPE_SINGLE,
			    BT_AVDTP_START, rxtid, &pAvdtp_conn->req.cmdtid);
		if (!resp_buf) {
			return;
		}

		if (resp_msg != BT_AVDTP_ACCEPT) {
			net_buf_add_mem(resp_buf, &error_code, sizeof(error_code));
		} else {
			pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_STREAMING;
			pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_STARTED;
		}

		if (pAvdtp_conn->pending_ahead_start) {
			if (pAvdtp_conn->pending_resp_buf) {
				net_buf_unref(pAvdtp_conn->pending_resp_buf);
			}
			pAvdtp_conn->pending_resp_buf = resp_buf;
		} else {
			avdtp_send(session, resp_buf);
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_STREAMING;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_STARTED;
		event_cb->media_state_req(session, BT_AVDTP_START);
	} else {
		/* Reject: what todo? */
	}
}

static void avdtp_close_handle(struct bt_avdtp *session, struct net_buf *buf,
			       uint8_t msg_type, uint8_t rxtid)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_check_cmd_format(session, buf, BT_AVDTP_CLOSE, rxtid, NULL)) {
			return;
		}

		/* avdtp_check_cmd_format have check seid exist */
		lsep_set_seid_free(buf->data[0] >> 2);

		if (avdtp_send_accept_resp(session, BT_AVDTP_CLOSE, rxtid) > 0) {
			event_cb->media_state_req(session, BT_AVDTP_CLOSE);
			pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_CLOSED;
			pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_CLOSEED;
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		lsep_set_seid_free(pAvdtp_conn->stream.lsid.id);
		pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_CLOSED;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_CLOSEED;
		event_cb->media_state_req(session, BT_AVDTP_CLOSE);
	} else {
		/* Reject: what todo? */
	}
}

static void avdtp_suspend_handle(struct bt_avdtp *session, struct net_buf *buf,
			       uint8_t msg_type, uint8_t rxtid)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_check_cmd_format(session, buf, BT_AVDTP_SUSPEND, rxtid, NULL)) {
			return;
		}

		if (avdtp_send_accept_resp(session, BT_AVDTP_SUSPEND, rxtid) > 0) {
			event_cb->media_state_req(session, BT_AVDTP_SUSPEND);
			pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_SUSPEND;
			pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_SUSPENDED;
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_SUSPEND;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_SUSPENDED;
		event_cb->media_state_req(session, BT_AVDTP_SUSPEND);
	} else {
		/* Reject: what todo? */
	}
}

static void avdtp_abort_handle(struct bt_avdtp *session, struct net_buf *buf,
			       uint8_t msg_type, uint8_t rxtid)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("msg_type %d", msg_type);

	if (msg_type == BT_AVDTP_CMD) {
		if (avdtp_check_cmd_format(session, buf, BT_AVDTP_ABORT, rxtid, NULL)) {
			return;
		}

		if (avdtp_send_accept_resp(session, BT_AVDTP_ABORT, rxtid) > 0) {
			event_cb->media_state_req(session, BT_AVDTP_ABORT);
			pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_ABORTING;
			pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_ABORTED;
		}
	} else if (msg_type == BT_AVDTP_ACCEPT) {
		pAvdtp_conn->stream.stream_state = BT_AVDTP_STREAM_STATE_ABORTING;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_ABORTED;
		event_cb->media_state_req(session, BT_AVDTP_ABORT);
	} else {
		/* Reject: what todo? */
	}
}

struct avdtp_signaling_handler {
	uint8_t sig_id;
	void (*func)(struct bt_avdtp *session, struct net_buf *buf,
		     uint8_t msg_type, uint8_t rxtid);
};

static const struct avdtp_signaling_handler handler[] = {
	{ BT_AVDTP_DISCOVER, avdtp_discover_handle },
	{ BT_AVDTP_GET_CAPABILITIES, avdtp_get_capabilities_handle },
	{ BT_AVDTP_GET_ALL_CAPABILITIES, avdtp_get_all_capabilities_handle },
	{ BT_AVDTP_SET_CONFIGURATION, avdtp_set_configuration_handle },
	{ BT_AVDTP_RECONFIGURE, avdtp_reconfigure_handle },
	{ BT_AVDTP_OPEN, avdtp_open_handle },
	{ BT_AVDTP_START, avdtp_start_handle },
	{ BT_AVDTP_CLOSE, avdtp_close_handle },
	{ BT_AVDTP_SUSPEND, avdtp_suspend_handle },
	{ BT_AVDTP_ABORT, avdtp_abort_handle },
};

/* Send failed, response to unref buf */
static int avdtp_send(struct bt_avdtp *session, struct net_buf *buf)
{
	int result;
	struct bt_avdtp_single_sig_hdr hdr;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	memcpy(&hdr, buf->data, sizeof(struct bt_avdtp_single_sig_hdr));

	avdtp_log("avdtp send sig:0x%x, msg:%d\n", AVDTP_GET_SIG_ID(hdr.signal_id), AVDTP_GET_MSG_TYPE(hdr.hdr));
	result = bt_l2cap_chan_send(&session->br_chan.chan, buf);
	if (result < 0) {
		net_buf_unref(buf);
		BT_ERR("Error:L2CAP send fail - result = %d", result);
		return result;
	}

	if (AVDTP_GET_MSG_TYPE(hdr.hdr) == BT_AVDTP_CMD) {
		/* Not replase session->req */
		pAvdtp_conn->req.sig = AVDTP_GET_SIG_ID(hdr.signal_id);
		pAvdtp_conn->req.tid = AVDTP_GET_TR_ID(hdr.hdr);
		pAvdtp_conn->req.func = bt_avdtp_send_timeout_handler;

		/* Send command, Start timeout work */
		k_work_schedule(&pAvdtp_conn->req.timeout_work.work, AVDTP_TIMEOUT);
		pAvdtp_conn->stream.int_state = BT_AVDTP_SIG_ID_TO_STATE_ING(pAvdtp_conn->req.sig);
	}

	return result;
}

static struct net_buf *avdtp_create_pdu(uint8_t msg_type,
					uint8_t pkt_type, uint8_t sig_id, uint8_t rxtid, uint8_t *cmdtid)
{
	struct net_buf *buf;
	struct bt_avdtp_single_sig_hdr *hdr;

	BT_DBG("");

	buf = bt_l2cap_create_pdu(NULL, 0);
	if (!buf) {
		BT_ERR("Can't get buf for msg_type:%d, sig_id:%d", msg_type, sig_id);
		return buf;
	}

	hdr = net_buf_add(buf, sizeof(*hdr));

	hdr->hdr = (msg_type | pkt_type << AVDTP_PKT_POSITION |
		    ((msg_type == BT_AVDTP_CMD) ? *cmdtid : rxtid) << AVDTP_TID_POSITION);
	hdr->signal_id = sig_id & AVDTP_SIGID_MASK;

	if (msg_type == BT_AVDTP_CMD) {
		(*cmdtid) += 1;
		(*cmdtid) %= 16; /* Loop for 16*/
	}

	BT_DBG("hdr = 0x%02X, Signal_ID = 0x%02X", hdr->hdr, hdr->signal_id);
	return buf;
}

/* Timeout handler */
static void avdtp_timeout(struct k_work *work)
{
	struct bt_avdtp_req *req = CONTAINER_OF(work, struct bt_avdtp_req,
								timeout_work);
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_REQ(req);

	BT_DBG("Failed Signal_id = %d", req->sig);
	if (req->func) {
		req->func(&pAvdtp_conn->signal_session, req);
	}
}

static void avdtp_state_sm_work(struct bt_avdtp_req *req)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_REQ(req);

	if (req->state_sm_func) {
		req->state_sm_func(&pAvdtp_conn->signal_session, req);
	}
}

/* L2CAP Interface callbacks */
void bt_avdtp_l2cap_connected(struct bt_l2cap_chan *chan)
{
	struct bt_avdtp *session;
	struct bt_avdtp_conn *pAvdtp_conn;

	if (!chan) {
		BT_ERR("Invalid AVDTP chan");
		return;
	}

	session = AVDTP_CHAN(chan);
	BT_DBG("chan %p session %p", chan, session);

	if (session->session_priority == BT_AVDTP_SIGNALING_SESSION) {
		pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);
		pAvdtp_conn->stream.acp_state = BT_AVDTP_ACPINT_STATE_IDLE;
		pAvdtp_conn->stream.int_state = BT_AVDTP_ACPINT_STATE_IDLE;
		if (session->role == BT_A2DP_CH_SOURCE) {
			pAvdtp_conn->stream.lsid.tsep = BT_A2DP_EP_SOURCE;
		} else if (session->role == BT_A2DP_CH_SINK) {
			pAvdtp_conn->stream.lsid.tsep = BT_A2DP_EP_SINK;
		}

		/* Init the timer */
		k_work_init_delayable(&pAvdtp_conn->req.timeout_work.work, avdtp_timeout);

		pAvdtp_conn->req.state_sm_func = bt_avdtp_state_sm;
	}

	session->connected = 1;

	avdtp_log("avdtp connected:%d\n", session->session_priority);
	event_cb->connected(session);

	if (session->session_priority == BT_AVDTP_MEDIA_SESSION) {
		pAvdtp_conn = AVDTP_CONN_BY_MEDIA(session);
		if (pAvdtp_conn->pending_ahead_start) {
			pAvdtp_conn->pending_ahead_start = 0;
			if (pAvdtp_conn->pending_resp_buf) {
				avdtp_send(&pAvdtp_conn->signal_session, pAvdtp_conn->pending_resp_buf);
				pAvdtp_conn->pending_resp_buf = NULL;
				event_cb->media_state_req(&pAvdtp_conn->signal_session, BT_AVDTP_PENDING_AHEAD_START);
			}
		}
	}
}

void bt_avdtp_l2cap_disconnected(struct bt_l2cap_chan *chan)
{
	struct bt_avdtp *session = AVDTP_CHAN(chan);
	struct bt_avdtp_conn *pAvdtp_conn;

	avdtp_log("avdtp disconnected:%d\n", session->session_priority);
	event_cb->disconnected(session);

	BT_DBG("chan %p session %p", chan, session);
	session->br_chan.chan.conn = NULL;

	if ((session->session_priority == BT_AVDTP_SIGNALING_SESSION) &&
		 session->connected) {
		pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

		/* Only need free stream after connected */
		lsep_set_seid_free(pAvdtp_conn->stream.lsid.id);

		/* Clear the Pending req if set*/
		k_work_cancel_delayable(&pAvdtp_conn->req.timeout_work.work);
		pAvdtp_conn->req.state_sm_func = NULL;

		pAvdtp_conn->pending_ahead_start = 0;
		if (pAvdtp_conn->pending_resp_buf) {
			net_buf_unref(pAvdtp_conn->pending_resp_buf);
			pAvdtp_conn->pending_resp_buf = NULL;
		}
	}

	session->connected = 0;
}

void bt_avdtp_l2cap_encrypt_changed(struct bt_l2cap_chan *chan, uint8_t status)
{
	BT_DBG("");
}

static int bt_avdtp_l2cap_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	struct bt_avdtp_single_sig_hdr *hdr = (void *)buf->data;
	struct bt_avdtp *session = AVDTP_CHAN(chan);
	uint8_t i, msgtype, sigid, tid;
	struct bt_avdtp_conn *pAvdtp_conn;

	if (session->session_priority == BT_AVDTP_MEDIA_SESSION) {
		event_cb->media_handler(session, buf);
		return 0;
	}

	pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (buf->len < sizeof(*hdr)) {
		BT_ERR("Recvd Wrong AVDTP Header");
		return -EINVAL;
	}

	msgtype = AVDTP_GET_MSG_TYPE(hdr->hdr);
	sigid = AVDTP_GET_SIG_ID(hdr->signal_id);
	tid = AVDTP_GET_TR_ID(hdr->hdr);

	BT_DBG("msg_type[0x%02x] sig_id[0x%02x] tid[0x%02x]",
		msgtype, sigid, tid);
	net_buf_pull(buf, sizeof(*hdr));

	/* validate if there is an outstanding resp expected*/
	if (msgtype != BT_AVDTP_CMD) {
		if (pAvdtp_conn->req.sig != sigid ||
		    pAvdtp_conn->req.tid != tid) {
			avdtp_log("Peer mismatch resp, expected sig[0x%02x]"
				"tid[0x%02x] sigid[0x%02x] tid[0x%02x]", pAvdtp_conn->req.sig,
				pAvdtp_conn->req.tid, sigid, tid);
			return -EINVAL;
		}

		/* Get responed, cancel delay work */
		k_work_cancel_delayable(&pAvdtp_conn->req.timeout_work.work);
		pAvdtp_conn->req.msg_type = msgtype;
	}

	avdtp_log("avdtp rcv sig:0x%x, msg:%d\n", sigid, msgtype);
	for (i = 0; i < ARRAY_SIZE(handler); i++) {
		if (sigid == handler[i].sig_id) {
			handler[i].func(session, buf, msgtype, tid);
			if (msgtype != BT_AVDTP_CMD) {
				avdtp_state_sm_work(&pAvdtp_conn->req);
			}
			return 0;
		}
	}

	if (msgtype == BT_AVDTP_CMD &&
		(sigid == 0 || sigid > BT_AVDTP_DELAYREPORT)) {/*invalid sigid */
		struct net_buf *resp_buf;
		struct bt_avdtp_single_sig_hdr *hdr;

		resp_buf = bt_l2cap_create_pdu(NULL, 0);
		if (!resp_buf) {
			return 0;
		}

		hdr = net_buf_add(resp_buf, sizeof(*hdr));

		hdr->hdr = BT_AVDTP_GEN_REJECT | 0 << AVDTP_PKT_POSITION |
			    (tid << AVDTP_TID_POSITION);
		hdr->signal_id = sigid & AVDTP_SIGID_MASK;

		avdtp_send(session, resp_buf);
	}

	return 0;
}

static bool bt_avdtp_is_media_aac_codec(struct bt_avdtp *session)
{
	struct bt_avdtp_conn *pAvdtp_conn;
	struct bt_a2dp_media_codec *codec;

	if (session->session_priority != BT_AVDTP_MEDIA_SESSION) {
		return false;
	}

	pAvdtp_conn = AVDTP_CONN_BY_MEDIA(session);
	codec = &pAvdtp_conn->stream.codec;
	if (codec == NULL) {
		return false;
	}

	if (codec->head.codec_type == BT_A2DP_MPEG2) {
		return true;
	} else {
		return false;
	}
}

/*A2DP Layer interface */
int bt_avdtp_connect(struct bt_conn *conn, struct bt_avdtp *session, uint8_t role)
{
	static const struct bt_l2cap_chan_ops ops = {
		.connected = bt_avdtp_l2cap_connected,
		.disconnected = bt_avdtp_l2cap_disconnected,
		.encrypt_change = bt_avdtp_l2cap_encrypt_changed,
		.recv = bt_avdtp_l2cap_recv
	};

	if (!session) {
		return -EINVAL;
	}

	session->role = role;
	session->intacp_role = BT_AVDTP_INT;
	session->br_chan.chan.ops = (struct bt_l2cap_chan_ops *)&ops;
	session->br_chan.rx.mtu = BT_L2CAP_RX_MTU;
	return bt_l2cap_chan_connect(conn, &session->br_chan.chan,
				     BT_L2CAP_PSM_AVDTP);
}

int bt_avdtp_disconnect(struct bt_avdtp *session)
{
	if (!session) {
		return -EINVAL;
	}

	BT_DBG("session %p", session);
	return bt_l2cap_chan_disconnect(&session->br_chan.chan);
}

int bt_avdtp_l2cap_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	struct bt_avdtp *session = NULL;
	int result;
	static const struct bt_l2cap_chan_ops ops = {
		.connected = bt_avdtp_l2cap_connected,
		.disconnected = bt_avdtp_l2cap_disconnected,
		.recv = bt_avdtp_l2cap_recv,
	};

	BT_DBG("conn %p", conn);
	/* Get the AVDTP session from upper layer */
	result = event_cb->accept(conn, &session);
	if (result < 0) {
		return result;
	}
	session->br_chan.chan.ops = (struct bt_l2cap_chan_ops *)&ops;
	session->br_chan.rx.mtu = BT_L2CAP_RX_MTU;
	*chan = &session->br_chan.chan;
	return 0;
}

/* Application will register its callback */
int bt_avdtp_register(struct bt_avdtp_event_cb *cb)
{
	BT_DBG("");

	if (event_cb) {
		return -EALREADY;
	}

	event_cb = cb;
	return 0;
}

static void bt_avdtp_env_init(void)
{
	event_cb = NULL;
	bt_avdtp_ep_env_init();
}

/* init function */
int bt_avdtp_init(void)
{
	int err;
	static struct bt_l2cap_server avdtp_l2cap = {
		.psm = BT_L2CAP_PSM_AVDTP,
		.sec_level = BT_SECURITY_L2,
		.accept = bt_avdtp_l2cap_accept,
	};

	BT_DBG("");

	bt_avdtp_env_init();

	/* Register AVDTP PSM with L2CAP */
	err = bt_l2cap_br_server_register(&avdtp_l2cap);
	if (err < 0) {
		BT_ERR("AVDTP L2CAP Registration failed %d", err);
	}

	return err;
}

/* AVDTP Discover Request */
int bt_avdtp_discover(struct bt_avdtp *session)
{
	struct net_buf *buf;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("");
	if (!session) {
		BT_DBG("Error: Callback/Session not valid");
		return -EINVAL;
	}

	buf = avdtp_create_pdu(BT_AVDTP_CMD,
			       BT_AVDTP_PACKET_TYPE_SINGLE,
			       BT_AVDTP_DISCOVER, 0, &pAvdtp_conn->req.cmdtid);
	if (!buf) {
		return -ENOMEM;
	}

	return avdtp_send(session, buf);
}

static int bt_avdtp_req_cmd_seid(struct bt_avdtp *session, uint8_t sig_id)
{
	struct net_buf *buf;
	struct bt_avdtp_get_capabilities_req req;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("");
	if (!session || !session->connected) {
		BT_ERR("Error: Session not valid or stream is NULL");
		return -EINVAL;
	}

	buf = avdtp_create_pdu(BT_AVDTP_CMD,
			       BT_AVDTP_PACKET_TYPE_SINGLE,
			       sig_id, 0, &pAvdtp_conn->req.cmdtid);
	if (!buf) {
		return -ENOMEM;
	}

	req.rfa0 = 0;
	req.seid = pAvdtp_conn->stream.rsid.id;
	net_buf_add_mem(buf, &req, sizeof(struct bt_avdtp_get_capabilities_req));

	return avdtp_send(session, buf);
}

static int bt_avdtp_get_cap_cmd(struct bt_avdtp *session, uint8_t sig_id)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (pAvdtp_conn->get_rsid_cap_index >= pAvdtp_conn->get_seid_num) {
		/* Do a2dp disconnect */
		avdtp_log("avdtp get cap mismatch disconnect\n");
		event_cb->intiator_connect_result(session, false);
		return -EIO;
	}

	memcpy(&pAvdtp_conn->stream.rsid, &pAvdtp_conn->get_seid[pAvdtp_conn->get_rsid_cap_index], sizeof(struct bt_avdtp_seid_info));
	pAvdtp_conn->get_rsid_cap_index++;
	return bt_avdtp_req_cmd_seid(session, sig_id);
}

int bt_avdtp_get_capabilities(struct bt_avdtp *session)
{
	return bt_avdtp_get_cap_cmd(session, BT_AVDTP_GET_CAPABILITIES);
}

int bt_avdtp_get_all_capabilities(struct bt_avdtp *session)
{
	return bt_avdtp_get_cap_cmd(session, BT_AVDTP_GET_ALL_CAPABILITIES);
}

static int bt_avdtp_setreset_configuration(struct bt_avdtp *session, uint8_t sig_id,
					struct bt_a2dp_media_codec *codec)
{
	struct net_buf *buf;
	struct bt_avdtp_setconf_req req;
	struct bt_avdtp_cap cap;
	struct bt_avdtp_seid_lsep *lsep;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("");
	if (!session || !session->connected) {
		BT_DBG("Error: Session not valid or stream is NULL");
		return -EINVAL;
	}

	if ((sig_id == BT_AVDTP_RECONFIGURE) &&
		(pAvdtp_conn->stream.stream_state != BT_AVDTP_STREAM_STATE_OPEN) &&
		(pAvdtp_conn->stream.stream_state != BT_AVDTP_STREAM_STATE_SUSPEND)) {
		return -EACCES;
	}

	buf = avdtp_create_pdu(BT_AVDTP_CMD,
			       BT_AVDTP_PACKET_TYPE_SINGLE,
			       sig_id, 0, &pAvdtp_conn->req.cmdtid);
	if (!buf) {
		return -ENOMEM;
	}

	if (!pAvdtp_conn->stream.lsid.id) {
		uint8_t local_role;

		if (pAvdtp_conn->stream.rsid.tsep == BT_A2DP_EP_SINK)
			local_role = BT_A2DP_EP_SOURCE;
		else
			local_role = BT_A2DP_EP_SINK;
		lsep = find_free_lsep_by_role_codectype(local_role, pAvdtp_conn->stream.codec.head.codec_type);
		if (lsep)
			pAvdtp_conn->stream.lsid.id = lsep->sid.id;
	}

	/* Add acp int id */
	memset(&req, 0, sizeof(struct bt_avdtp_setconf_req));
	req.acp_seid = pAvdtp_conn->stream.rsid.id;
	req.int_seid = pAvdtp_conn->stream.lsid.id;
	if (sig_id == BT_AVDTP_RECONFIGURE) {
		/* Reconfigure only need acp seid */
		net_buf_add_mem(buf, &req, sizeof(struct bt_avdtp_reconf_req));
	} else {
		net_buf_add_mem(buf, &req, sizeof(struct bt_avdtp_setconf_req));
	}

	if (sig_id == BT_AVDTP_SET_CONFIGURATION) {
		/* Add BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT */
		cap.cat = BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT;
		cap.len = 0;
		net_buf_add_mem(buf, &cap, sizeof(struct bt_avdtp_cap));
	}

	/* Add BT_AVDTP_SERVICE_CAT_MEDIA_CODEC */
	cap.cat = BT_AVDTP_SERVICE_CAT_MEDIA_CODEC;

	if (sig_id == BT_AVDTP_SET_CONFIGURATION) {
		cap.len = bt_avdtp_ep_get_codec_len(&pAvdtp_conn->stream.codec);
	} else {
		cap.len = bt_avdtp_ep_get_codec_len(codec);
		memcpy(&pAvdtp_conn->stream.codec, codec, cap.len);
	}
	net_buf_add_mem(buf, &cap, sizeof(struct bt_avdtp_cap));

	/* Add codec */
	if (cap.len) {
		net_buf_add_mem(buf, &pAvdtp_conn->stream.codec, cap.len);
	}

	/* Add content protection type */
	if (pAvdtp_conn->stream.cp_type == BT_AVDTP_AV_CP_TYPE_SCMS_T) {
		cap.cat = BT_AVDTP_SERVICE_CAT_CONTENT_PROTECTION;
		cap.len = 2;
		net_buf_add_mem(buf, &cap, sizeof(struct bt_avdtp_cap));
		net_buf_add_le16(buf, BT_AVDTP_AV_CP_TYPE_SCMS_T);
	}

	/* Add delay report */
	if (pAvdtp_conn->stream.delay_report) {
		cap.cat = BT_AVDTP_SERVICE_CAT_DELAYREPORTING;
		cap.len = 2;
		net_buf_add_mem(buf, &cap, sizeof(struct bt_avdtp_cap));
	}

	return avdtp_send(session, buf);
}

int bt_avdtp_set_configuration(struct bt_avdtp *session)
{
	return bt_avdtp_setreset_configuration(session, BT_AVDTP_SET_CONFIGURATION, NULL);
}


int bt_avdtp_reconfig(struct bt_avdtp *session, struct bt_a2dp_media_codec *codec)
{
	return bt_avdtp_setreset_configuration(session, BT_AVDTP_RECONFIGURE, codec);
}

int bt_avdtp_open(struct bt_avdtp *session)
{
	return bt_avdtp_req_cmd_seid(session, BT_AVDTP_OPEN);
}

int bt_avdtp_start(struct bt_avdtp *session)
{
	return bt_avdtp_req_cmd_seid(session, BT_AVDTP_START);
}

int bt_avdtp_suspend(struct bt_avdtp *session)
{
	return bt_avdtp_req_cmd_seid(session, BT_AVDTP_SUSPEND);
}

int bt_avdtp_close(struct bt_avdtp *session)
{
	return bt_avdtp_req_cmd_seid(session, BT_AVDTP_CLOSE);
}

int bt_avdtp_abort(struct bt_avdtp *session)
{
	return bt_avdtp_req_cmd_seid(session, BT_AVDTP_ABORT);
}

/* delay_time: 1/10 milliseconds */
int bt_avdtp_delayreport(struct bt_avdtp *session, uint16_t delay_time)
{
	struct net_buf *buf;
	struct bt_avdtp_get_capabilities_req req;
	uint8_t time[2];
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	BT_DBG("");
	if (!session || !session->connected) {
		BT_DBG("Error: Session not valid or stream is NULL");
		return -EINVAL;
	}

	if (!pAvdtp_conn->stream.delay_report) {
		return -EIO;
	}

	buf = avdtp_create_pdu(BT_AVDTP_CMD,
			       BT_AVDTP_PACKET_TYPE_SINGLE,
			       BT_AVDTP_DELAYREPORT, 0, &pAvdtp_conn->req.cmdtid);
	if (!buf) {
		return -ENOMEM;
	}

	req.rfa0 = 0;
	req.seid = pAvdtp_conn->stream.rsid.id;
	net_buf_add_mem(buf, &req, sizeof(struct bt_avdtp_get_capabilities_req));

	time[0] = (delay_time >> 8)&0xFF;
	time[1] = delay_time&0xFF;
	net_buf_add_mem(buf, time, sizeof(time));

	return avdtp_send(session, buf);
}

static int bt_avdtp_send_timeout_handler(struct bt_avdtp *session,
					struct bt_avdtp_req *req)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	avdtp_log("avdtp send timeout state:0x%x, sig:0x%x\n", pAvdtp_conn->stream.int_state, req->sig);
	if (BT_AVDTP_IS_ACPINT_STATE_ING(pAvdtp_conn->stream.int_state) &&
		(pAvdtp_conn->stream.stream_state < BT_AVDTP_STREAM_STATE_OPEN)) {
		/* Do a2dp disconnect */
		avdtp_log("avdtp send timeout disconnect\n");
		event_cb->intiator_connect_result(session, false);
	}

	return 0;
}

static int bt_avdtp_state_sm(struct bt_avdtp *session, struct bt_avdtp_req *req)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if ((pAvdtp_conn->stream.int_state != BT_AVDTP_ACPINT_STATE_OPENED) &&
		(pAvdtp_conn->stream.int_state != BT_AVDTP_ACPINT_STATE_RECFGED) &&
		(pAvdtp_conn->stream.acp_state >= BT_AVDTP_ACPINT_STATE_SET_CFGED)) {
		/* If pair device start avdtp as initiator, and we not recieve open accept, change us to acceptor */
		avdtp_log("avdtp sm change to acp\n");
		session->intacp_role = BT_AVDTP_ACP;
	}

	if (session->intacp_role != BT_AVDTP_INT) {
		/* Not as initiator role, do nothing */
		avdtp_log("avdtp sm acp role\n");
		return 0;
	}

	if (BT_AVDTP_IS_ACPINT_STATE_ING(pAvdtp_conn->stream.int_state) &&
		(pAvdtp_conn->stream.stream_state < BT_AVDTP_STREAM_STATE_OPEN)) {
		/* Do a2dp disconnect */
		avdtp_log("avdtp sm cmd not accecpt int_state:0x%x, sig:0x%x\n", pAvdtp_conn->stream.int_state, req->sig);
		event_cb->intiator_connect_result(session, false);
		return 0;
	}

	avdtp_log("avdtp sm state:0x%x\n", pAvdtp_conn->stream.int_state);
	switch (pAvdtp_conn->stream.int_state) {
	case BT_AVDTP_ACPINT_STATE_DISCOVERED:
	case BT_AVDTP_ACPINT_STATE_GET_CAPEXT:
		bt_avdtp_get_capabilities(session);
		break;
	case BT_AVDTP_ACPINT_STATE_GET_ACFGEXT:
		bt_avdtp_get_all_capabilities(session);
		break;
	case BT_AVDTP_ACPINT_STATE_GET_CAPED:
		if ((session->role == BT_A2DP_CH_SOURCE)
			|| (session->role == BT_A2DP_CH_SINK)) {
			if (find_free_lsep_by_role_codectype(pAvdtp_conn->stream.lsid.tsep,
				pAvdtp_conn->stream.codec.head.codec_type)) {
				bt_avdtp_set_configuration(session);
			}
		}
		break;
	case BT_AVDTP_ACPINT_STATE_SET_CFGED:
		if (pAvdtp_conn->stream.stream_state < BT_AVDTP_STREAM_STATE_OPEN) {
			bt_avdtp_open(session);
		}
		break;
	case BT_AVDTP_ACPINT_STATE_OPENED:
		event_cb->do_media_connect(session, true);
		break;
	case BT_AVDTP_ACPINT_STATE_CLOSEED:
		event_cb->do_media_connect(session, false);
		break;
	}

	return 0;
}

struct bt_a2dp_media_codec *bt_avdtp_get_seted_codec(struct bt_avdtp *session)
{
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	switch (pAvdtp_conn->stream.stream_state) {
	case BT_AVDTP_STREAM_STATE_OPEN:
	case BT_AVDTP_STREAM_STATE_STREAMING:
	case BT_AVDTP_STREAM_STATE_SUSPEND:
		return &pAvdtp_conn->stream.codec;
	default:
		break;
	}

	return NULL;
}
