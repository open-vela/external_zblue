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

#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/sdp.h>
#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/avrcp.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_AVRCP)
#define LOG_MODULE_NAME bt_avrcp
#include "common/log.h"

#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "avrcp_internal.h"
#include "common_internal.h"

#define AVRCP_DEBUG_LOG		0
#if AVRCP_DEBUG_LOG
#define avrcp_log(fmt, ...) \
		do {	\
			if (bt_internal_debug_log())	\
				printk(fmt, ##__VA_ARGS__);	\
		} while (0)
#else
#define avrcp_log(fmt, ...)
#endif

#define AVRCP_TIMEOUT K_SECONDS(3)
#define AVRCP_CHAN(_ch) CONTAINER_OF(_ch, struct bt_avrcp, br_chan.chan)
#define AVRCP_2U8T_TO_U16T(x)	((x[0] << 8) | (x[1]))
#define AVRCP_4U8T_TO_U32T(x)	((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | (x[3]))

static struct bt_avrcp_event_cb *avrcp_ctrl_event_cb;

static int bt_avrcp_send_timeout_handler(struct bt_avrcp *session,
					struct bt_avrcp_req *req);
static int bt_avrcp_state_sm(struct bt_avrcp *session, struct bt_avrcp_req *req);

static struct net_buf *avctp_create_pdu(struct bt_avrcp *session, uint8_t cmd)
{
	struct net_buf *buf;
	struct bt_avctp_header *hdr;

	BT_DBG("");

	buf = bt_l2cap_create_pdu(NULL, 0);
	if (!buf) {
		BT_ERR("Error: No Buff available");
		return buf;
	}

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->cr = (cmd == BT_AVRCP_CMD) ? 0 : 1;
	hdr->ipid = 0;
	hdr->packet_type = 0;
	hdr->tid = (cmd == BT_AVRCP_CMD) ? session->ct_tid : session->tg_tid;
	hdr->pid = sys_cpu_to_be16(BT_SDP_AV_REMOTE_SVCLASS);

	if (cmd == BT_AVRCP_CMD) {
		session->ct_tid++;
		session->ct_tid %= 16; /* Loop for 16*/
	}

	return buf;
}

static struct net_buf *avrcp_create_unit_pdu(struct bt_avrcp *session,
						uint8_t cmd, uint8_t ctype, uint8_t op_id)
{
	struct net_buf *buf;
	struct bt_avrcp_unit_info  *info;

	buf = avctp_create_pdu(session, cmd);
	if (!buf) {
		return buf;
	}

	info = net_buf_add(buf, sizeof(*info));
	memset(info, 0, sizeof(*info));
	info->hdr.ctype = ctype;
	info->hdr.subunit_id = BT_AVRCP_SUBUNIT_ID_IGNORE;
	info->hdr.subunit_type = BT_AVRCP_SUBUNIT_TYPE_UNIT;
	info->hdr.opcode = op_id;

	return buf;
}

static struct net_buf *avrcp_create_pass_pdu(struct bt_avrcp *session,
						uint8_t cmd, uint8_t ctype, uint8_t op_id, uint8_t state)
{
	struct net_buf *buf;
	struct bt_avrcp_pass_through_info *pass;

	buf = avctp_create_pdu(session, cmd);
	if (!buf) {
		return buf;
	}

	pass = net_buf_add(buf, sizeof(*pass));
	memset(pass, 0, sizeof(*pass));
	pass->hdr.ctype = ctype;
	pass->hdr.subunit_type = BT_AVRCP_SUBUNIT_TYPE_PANEL;
	pass->hdr.opcode = BT_AVRCP_PASS_THROUGH_OPCODE;
	pass->state = state;
	pass->op_id = op_id;

	return buf;
}

static struct net_buf *avrcp_create_vendor_pdu(struct bt_avrcp *session,
						uint8_t cmd, uint8_t ctype, uint8_t pdu_id)
{
	struct net_buf *buf;
	struct bt_avrcp_vendor_info *verdor;

	buf = avctp_create_pdu(session, cmd);
	if (!buf) {
		return buf;
	}

	verdor = net_buf_add(buf, sizeof(*verdor));
	memset(verdor, 0, sizeof(*verdor));
	verdor->hdr.ctype = ctype;
	verdor->hdr.subunit_type = BT_AVRCP_SUBUNIT_TYPE_PANEL;
	verdor->hdr.subunit_id = BT_AVRCP_SUBUNIT_ID;
	verdor->hdr.opcode = BT_AVRCP_VENDOR_DEPENDENT_OPCODE;
	verdor->company_id[0] = (uint8_t)(BT_SIG_COMPANY_ID >> 16);
	verdor->company_id[1] = (uint8_t)(BT_SIG_COMPANY_ID >> 8);
	verdor->company_id[2] = (uint8_t)(BT_SIG_COMPANY_ID);
	verdor->pdu_id = pdu_id;

	return buf;
}

/* Send failed, response to unref buf */
static int avrcp_send(struct bt_avrcp *session, struct net_buf *buf)
{
	int result;
	struct bt_avctp_header hdr;
	struct bt_avrcp_header avrcphdr;

	memcpy(&hdr, buf->data, sizeof(struct bt_avctp_header));
	memcpy(&avrcphdr, &buf->data[sizeof(struct bt_avctp_header)], sizeof(struct bt_avrcp_header));

	avrcp_log("avrcp send opcode:0x%x, msg:%d ,tid : %d\n", avrcphdr.opcode, hdr.cr,hdr.tid);
	result = bt_l2cap_chan_send(&session->br_chan.chan, buf);
	if (result < 0) {
		net_buf_unref(buf);
		BT_ERR("Error:L2CAP send fail - result = %d", result);
		return result;
	}

	if ((hdr.cr == BT_AVRCP_CMD) &&
		(avrcphdr.opcode != BT_AVRCP_PASS_THROUGH_OPCODE)) {
		session->req.subunit_type = avrcphdr.subunit_type;
		session->req.opcode = avrcphdr.opcode;
		session->req.tid = hdr.tid;
		session->req.timeout_func = bt_avrcp_send_timeout_handler;

		/* Send command, Start timeout work */
		k_delayed_work_submit(&session->req.timeout_work, AVRCP_TIMEOUT);
	}

	return 0;
}


/* Timeout handler */
static void avrcp_timeout(struct k_work *work)
{
	struct bt_avrcp_req *req = CONTAINER_OF(work, struct bt_avrcp_req, timeout_work);
	struct bt_avrcp *session = CONTAINER_OF(req, struct bt_avrcp, req);

	BT_DBG("Failed subunit_type:%d, opcode:%d tid:%d", req->subunit_type, req->opcode,req->tid);
	if (req->timeout_func) {
		req->timeout_func(session, req);
	}
}

static void avrcp_state_sm_work(struct bt_avrcp_req *req)
{
	struct bt_avrcp *session = CONTAINER_OF(req, struct bt_avrcp, req);

	if (req->state_sm_func) {
		req->state_sm_func(session, req);
	}
}

static void bt_avrcp_l2cap_connected(struct bt_l2cap_chan *chan)
{
	struct bt_avrcp *session;

	if (!chan) {
		BT_ERR("Invalid AVRCP chan");
		return;
	}

	session = AVRCP_CHAN(chan);
	BT_DBG("chan %p session %p", chan, session);

	if (bti_avrcp_vol_sync()) {
		session->l_tg_ebitmap = AVRCP_LOCAL_TG_SUPPORT_EVENT | BT_AVRCP_EVENT_BIT_MAP(BT_AVRCP_EVENT_VOLUME_CHANGED);
	} else {
		session->l_tg_ebitmap = AVRCP_LOCAL_TG_SUPPORT_EVENT;
	}
	session->r_tg_ebitmap = 0;
	session->l_reg_notify_event = BT_AVRCP_EVENT_BIT_MAP(BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED) |
		BT_AVRCP_EVENT_BIT_MAP(BT_AVRCP_EVENT_TRACK_CHANGED);
	session->r_reg_notify_event = 0;
	session->req.state_sm_func = bt_avrcp_state_sm;
	k_delayed_work_init(&session->req.timeout_work, avrcp_timeout);

	avrcp_ctrl_event_cb->connected(session);

	avrcp_log("avrcp connected\n");
	session->CT_state = BT_AVRCP_STATE_CONNECTED;
	avrcp_state_sm_work(&session->req);
}

void bt_avrcp_l2cap_disconnected(struct bt_l2cap_chan *chan)
{
	struct bt_avrcp *session = AVRCP_CHAN(chan);

	avrcp_log("avrcp connected\n");
	avrcp_ctrl_event_cb->disconnected(session);

	BT_DBG("chan %p session %p", chan, session);
	session->br_chan.chan.conn = NULL;
	session->r_tg_ebitmap = 0;

	/* Cancel timer */
	k_delayed_work_cancel(&session->req.timeout_work);
	session->req.state_sm_func = NULL;
}

static void avrcp_unit_info_cmd_handle(struct bt_avrcp *session)
{
	struct net_buf *buf;
	uint8_t param[5];

	buf = avrcp_create_unit_pdu(session, BT_AVRCP_RESOPEN,
				BT_AVRCP_CTYPE_IMPLEMENTED_STABLE, BT_AVRCP_UNIT_INFO_OPCODE);
	if (!buf) {
		return;
	}

	param[0] = 0x07;
	param[1] = (BT_AVRCP_SUBUNIT_TYPE_PANEL << 3) | BT_AVRCP_SUBUNIT_ID;
	param[2] = (uint8_t)(BT_ACTION_COMPANY_ID >> 16);
	param[3] = (uint8_t)(BT_ACTION_COMPANY_ID >> 8);
	param[4] = (uint8_t)BT_ACTION_COMPANY_ID;
	net_buf_add_mem(buf, param, sizeof(param));

	avrcp_send(session, buf);
}

static void avrcp_subunit_info_cmd_handle(struct bt_avrcp *session)
{
	struct net_buf *buf;
	uint8_t param[5];

	buf = avrcp_create_unit_pdu(session, BT_AVRCP_RESOPEN,
				BT_AVRCP_CTYPE_IMPLEMENTED_STABLE, BT_AVRCP_SUBUNIT_INFO_OPCODE);
	if (!buf) {
		return;
	}

	param[0] = 0x07;	/* page: 0, extension code:0x7 */
	param[1] = (BT_AVRCP_SUBUNIT_TYPE_PANEL << 3) | BT_AVRCP_SUBUNIT_ID;
	param[2] = 0xFF;
	param[3] = 0xFF;
	param[4] = 0xFF;
	net_buf_add_mem(buf, param, sizeof(param));

	avrcp_send(session, buf);
}

static void avrcp_pass_through_cmd_handle(struct bt_avrcp *session, struct net_buf *buf)
{
	struct net_buf *rsp_buf;
	struct bt_avrcp_pass_through_info *info = (void *)buf->data;

	avrcp_log("avrcp rx pass cmd opid:0x%x, state:%d\n", info->op_id, info->state);
	rsp_buf = avrcp_create_pass_pdu(session, BT_AVRCP_RESOPEN, BT_AVRCP_CTYPE_ACCEPTED, info->op_id, info->state);
	if (!rsp_buf) {
		return;
	}

	avrcp_send(session, rsp_buf);

	avrcp_ctrl_event_cb->pass_ctrl(session, info->op_id, info->state);
}

static void avrcp_pass_through_rsp_handle(struct bt_avrcp *session,
								struct net_buf *buf)
{
	struct bt_avrcp_pass_through_info *info = (void *)buf->data;

	if (info->hdr.ctype != BT_AVRCP_CTYPE_ACCEPTED) {
		BT_ERR("Responed ctyep:%d not accepted", info->hdr.ctype);
		return;
	}

	/* Can't send released immediately, some slower device can't identify push and released
	 * bt_avrcp_ct_pass_through_cmd send released 5ms later after receive accept pushed.
	 */
	if ((info->op_id != AVRCP_OPERATION_ID_REWIND) &&
		(info->op_id != AVRCP_OPERATION_ID_FAST_FORWARD)) {
		if (info->state == BT_AVRCP_PASS_THROUGH_PUSHED) {
			avrcp_ctrl_event_cb->pass_ctrl(session, info->op_id, BT_AVRCP_RSP_STATE_PASS_THROUGH_PUSHED);
		} else {
			avrcp_ctrl_event_cb->pass_ctrl(session, info->op_id, BT_AVRCP_RSP_STATE_PASS_THROUGH_RELEASED);
		}
	}
}

static void avrcp_cmd_reject_rsp(struct bt_avrcp *session, uint8_t pdu_id, uint8_t err_code)
{
	struct net_buf *rsp_buf;
	uint8_t param[2];
	uint16_t rsp_len = 0;

	rsp_buf = avrcp_create_vendor_pdu(session, BT_AVRCP_RESOPEN,
					BT_AVRCP_CTYPE_REJECTED, pdu_id);
	if (!rsp_buf) {
		return;
	}

	rsp_len = 1;
	param[0] = BT_AVRCP_ERROR_INVALID_CMD;
	net_buf_add_be16(rsp_buf, rsp_len);
	net_buf_add_mem(rsp_buf, param, rsp_len);

	avrcp_send(session, rsp_buf);
}

static void avrcp_verdor_capabilities_cmd_handle(struct bt_avrcp *session,
									struct net_buf *buf)
{
	struct bt_avrcp_vendor_capabilities *cap = (void *)buf->data;
	struct net_buf *rsp_buf;
	uint8_t param[16], i;
	uint16_t rsp_len = 0;

	if (cap->capabilityid == BT_AVRCP_CAPABILITY_ID_COMPANY) {
		rsp_len = 8;
		param[0] = cap->capabilityid;
		param[1] = 0x02;		/* Capablity count */
		param[2] = (uint8_t)(BT_SIG_COMPANY_ID >> 16);
		param[3] = (uint8_t)(BT_SIG_COMPANY_ID >> 8);
		param[4] = (uint8_t)BT_SIG_COMPANY_ID;
		param[5] = (uint8_t)(BT_ACTION_COMPANY_ID >> 16);
		param[6] = (uint8_t)(BT_ACTION_COMPANY_ID >> 8);
		param[7] = (uint8_t)BT_ACTION_COMPANY_ID;
	} else if (cap->capabilityid == BT_AVRCP_CAPABILITY_ID_EVENT) {
		for (i = BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED;
			i <= BT_AVRCP_EVENT_VOLUME_CHANGED; i++) {
			if (BT_AVRCP_EVENT_SUPPORT(session->l_tg_ebitmap, i)) {
				param[2 + rsp_len] = i;
				rsp_len++;
			}
		}

		if (rsp_len) {
			param[0] = cap->capabilityid;
			param[1] = rsp_len;
			rsp_len += 2;
		}
	} else {
		avrcp_cmd_reject_rsp(session, BT_AVRCP_PDU_ID_GET_CAPABILITIES, BT_AVRCP_ERROR_INVALID_PARAM);
		return;
	}

	if (rsp_len) {
		rsp_buf = avrcp_create_vendor_pdu(session, BT_AVRCP_RESOPEN,
						BT_AVRCP_CTYPE_IMPLEMENTED_STABLE, BT_AVRCP_PDU_ID_GET_CAPABILITIES);
		if (!rsp_buf) {
			return;
		}

		net_buf_add_be16(rsp_buf, rsp_len);
		net_buf_add_mem(rsp_buf, param, rsp_len);

		avrcp_send(session, rsp_buf);
	}
}

static void avrcp_verdor_play_status_cmd_handle(struct bt_avrcp *session,
									struct net_buf *buf)
{
	struct net_buf *rsp_buf;
	uint32_t song_len = 0xFFFFFFFF;		/* Not support value */
	uint32_t song_pos = 0xFFFFFFFF;
	uint8_t play_state = 0xFF;

	rsp_buf = avrcp_create_vendor_pdu(session, BT_AVRCP_RESOPEN,
					BT_AVRCP_CTYPE_IMPLEMENTED_STABLE, BT_AVRCP_PDU_ID_GET_PLAY_STATUS);
	if (!rsp_buf) {
		return;
	}

	avrcp_ctrl_event_cb->get_play_status(session, 1, &song_len, &song_pos, &play_state);

	net_buf_add_be16(rsp_buf, 9);
	net_buf_add_be32(rsp_buf, song_len);
	net_buf_add_be32(rsp_buf, song_pos);
	net_buf_add_u8(rsp_buf, play_state);

	avrcp_send(session, rsp_buf);
}

static void avrcp_verdor_notify_cmd_handle(struct bt_avrcp *session,
									struct net_buf *buf)
{
	struct net_buf *rsp_buf;
	struct bt_avrcp_vendor_notify_cmd *cmd = (void *)buf->data;
	uint8_t param[2];
	uint8_t volume = 0x3F;

	if ((cmd->event_id > BT_AVRCP_EVENT_VOLUME_CHANGED) || (cmd->event_id < BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED)
		|| (!BT_AVRCP_EVENT_SUPPORT(session->l_tg_ebitmap, cmd->event_id))) {
		avrcp_cmd_reject_rsp(session, BT_AVRCP_PDU_ID_REGISTER_NOTIFICATION, BT_AVRCP_ERROR_INVALID_PARAM);
		return;
	}

	avrcp_log("avrcp cmd tid:0x%x 0x%x\n", session->tg_tid,cmd->event_id);
	session->tg_notify_tid = session->tg_tid;
	session->r_reg_notify_event = cmd->event_id;
	session->r_reg_notify_interval = AVRCP_4U8T_TO_U32T(cmd->interval);

	rsp_buf = avrcp_create_vendor_pdu(session, BT_AVRCP_RESOPEN,
					BT_AVRCP_CTYPE_INTERIM, BT_AVRCP_PDU_ID_REGISTER_NOTIFICATION);
	if (!rsp_buf) {
		return;
	}

	avrcp_ctrl_event_cb->get_volume(session, &volume);

	net_buf_add_be16(rsp_buf, 2);

	param[0] = session->r_reg_notify_event;
	param[1] = volume;
	net_buf_add_mem(rsp_buf, param, 2);

	avrcp_send(session, rsp_buf);
}

static void avrcp_verdor_set_volume_cmd_handle(struct bt_avrcp *session,
									struct net_buf *buf)
{
	struct net_buf *rsp_buf;
	struct bt_avrcp_vendor_setvolume_cmd *cmd = (void *)buf->data;

	if (cmd->len == 0) {
		avrcp_cmd_reject_rsp(session, BT_AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME, BT_AVRCP_ERROR_INVALID_PARAM);
		return;
	}

	avrcp_ctrl_event_cb->notify(session, BT_AVRCP_EVENT_VOLUME_CHANGED, cmd->volume);
	rsp_buf = avrcp_create_vendor_pdu(session, BT_AVRCP_RESOPEN,
					BT_AVRCP_CTYPE_ACCEPTED, BT_AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME);
	if (!rsp_buf) {
		return;
	}

	net_buf_add_be16(rsp_buf, 1);
	net_buf_add_u8(rsp_buf, cmd->volume);

	avrcp_send(session, rsp_buf);
}

static void avrcp_verdor_cmd_handle(struct bt_avrcp *session, struct net_buf *buf)
{
	struct bt_avrcp_vendor_info *info = (void *)buf->data;

	avrcp_log("avrcp cmd pdu_id:0x%x\n", info->pdu_id);
	switch (info->pdu_id) {
	case BT_AVRCP_PDU_ID_GET_CAPABILITIES:
		avrcp_verdor_capabilities_cmd_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_GET_PLAY_STATUS:
		avrcp_verdor_play_status_cmd_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_REGISTER_NOTIFICATION:
		avrcp_verdor_notify_cmd_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME:
		avrcp_verdor_set_volume_cmd_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_UNDEFINED:
		avrcp_cmd_reject_rsp(session, info->pdu_id, BT_AVRCP_ERROR_INVALID_CMD);
		break;
	}
}

static void avrcp_verdor_capabilities_rsp_handle(struct bt_avrcp *session,
									struct net_buf *buf)
{
	uint8_t i;
	struct bt_avrcp_vendor_capabilities *cap = (void *)buf->data;

	if (cap->capabilityid == BT_AVRCP_CAPABILITY_ID_COMPANY) {
		/* TODO */
	} else if (cap->capabilityid == BT_AVRCP_CAPABILITY_ID_EVENT) {
		session->r_tg_ebitmap = 0;
		for (i = 0; i < cap->capabilitycnt; i++) {
			session->r_tg_ebitmap |= BT_AVRCP_EVENT_BIT_MAP(cap->capability[i]);
		}

		avrcp_log("avrcp r_tg_ebitmap:0x%x\n", session->r_tg_ebitmap);
		session->CT_state = BT_AVRCP_STATE_GET_CAPABILITIES_ED;
		avrcp_state_sm_work(&session->req);
	}
}

static void avrcp_verdor_play_status_rsp_handle(struct bt_avrcp *session,
								struct net_buf *buf)
{
	struct bt_avrcp_vendor_getplaystatus_rsp *rsp = (void *)buf->data;
	uint32_t len, pos;

	len = AVRCP_4U8T_TO_U32T(rsp->SongLen);
	pos = AVRCP_4U8T_TO_U32T(rsp->SongPos);
	BT_DBG("len:0x%x, pos:0x%x, status:%d", len, pos, rsp->status);

	avrcp_ctrl_event_cb->get_play_status(session, 0, &len, &pos, &rsp->status);
}

static void avrcp_verdor_notify_rsp_handle(struct bt_avrcp *session,
								struct net_buf *buf)
{
	uint32_t pos;
	struct bt_avrcp_vendor_notify_rsp *rsp = (void *)buf->data;

	avrcp_log("notify ctype:0x%x, event_id:0x%x\n", rsp->info.hdr.ctype, rsp->event_id);
	if (rsp->info.hdr.ctype == BT_AVRCP_CTYPE_CHANGED_STABLE) {
		if (rsp->event_id == BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED) {
			avrcp_ctrl_event_cb->notify(session, rsp->event_id, rsp->status);
			session->CT_state = BT_AVRCP_STATE_STATUS_CHANGED_ED;
		} else if (rsp->event_id == BT_AVRCP_EVENT_TRACK_CHANGED) {
			avrcp_ctrl_event_cb->notify(session, rsp->event_id, 0);
			session->CT_state = BT_AVRCP_STATE_TRACK_CHANGED_ED;
		} else if (rsp->event_id == BT_AVRCP_EVENT_PLAYBACK_POS_CHANGED) {
			pos = AVRCP_4U8T_TO_U32T(rsp->pos);
			/* Spec: If no track currently selected, then return 0xFFFFFFFF in the INTERIM response */
			avrcp_ctrl_event_cb->playback_pos(session, pos);
			return;		/* Get playback pos not need auto register again */
		} else if (bt_internal_is_pts_test() && rsp->event_id == BT_AVRCP_EVENT_VOLUME_CHANGED) {
			BT_INFO("Notify volume change, value %d\n", rsp->status);
		}

		avrcp_state_sm_work(&session->req);
	} else if (rsp->info.hdr.ctype == BT_AVRCP_CTYPE_INTERIM) {
		/* Application don't care notify responed state
		 *if (rsp->event_id == BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED) {
		 *	avrcp_ctrl_event_cb->notify(session, rsp->event_id, rsp->status);
		 *}
		 */

		if (rsp->event_id == BT_AVRCP_EVENT_PLAYBACK_POS_CHANGED) {
			pos = AVRCP_4U8T_TO_U32T(rsp->pos);
			/* Spec: If no track currently selected, then return 0xFFFFFFFF in the INTERIM response */
			avrcp_ctrl_event_cb->playback_pos(session, pos);
		}

		session->CT_state = BT_AVRCP_STATE_REGISTER_NOTIFICATION_ED;
	}
}

static void avrcp_verdor_get_attributes_rsp_handle(struct bt_avrcp *session,
								struct net_buf *buf)
{
	struct bt_avrcp_vendor_getelementatt_rsp *rsp = (void *)buf->data;
	uint8_t *plen = (uint8_t*)&(rsp->len);
	uint8_t *pid;
	rsp->len = AVRCP_2U8T_TO_U16T(plen);
	int total_len = rsp->len -1;//1byte attribute num
	int num = rsp->attribute_num;

	//BT_INFO("buf->len %d, rsp->len %d\n", buf->len, rsp->len);
	if (buf->len < rsp->len + sizeof(struct bt_avrcp_vendor_info)) {
		BT_ERR("buf->len %d, rsp->len %d\n", buf->len, rsp->len);
		return;
	}

	if(num > TOTAL_ATTRIBUTE_ITEM_NUM) {
		BT_ERR("attr num %d > 5.\n", num);
		return;
	}

	if(num != TOTAL_ATTRIBUTE_ITEM_NUM)
		avrcp_log("attribute num is  %d\n",num);
	uint8_t *data = (uint8_t *)rsp->attribute;
	
	struct id3_info info;
	memset(&info,0,sizeof(info));
	while(total_len > 0 && num > 0){
		struct Attribute *attribute = (struct Attribute *)data;

		if (total_len < sizeof(struct Attribute)) {
			BT_ERR("total_len %d, header %d.", total_len, sizeof(struct Attribute));
			return;
		}

		plen = (uint8_t*)&(attribute->len);
		attribute->len = AVRCP_2U8T_TO_U16T(plen);
		pid = (uint8_t*)&(attribute->id);
		attribute->id = AVRCP_4U8T_TO_U32T(pid);
		pid = (uint8_t*)&(attribute->character_id);
		attribute->character_id = AVRCP_2U8T_TO_U16T(pid);

		if (total_len < sizeof(struct Attribute) + attribute->len) {
			BT_ERR("total_len %d, attrlen %d.", total_len, attribute->len);
			return;
		}

		info.item[TOTAL_ATTRIBUTE_ITEM_NUM - num].id = attribute->id;
		info.item[TOTAL_ATTRIBUTE_ITEM_NUM - num].character_id = attribute->character_id;
		info.item[TOTAL_ATTRIBUTE_ITEM_NUM - num].len = attribute->len;
		if(attribute->len)
			info.item[TOTAL_ATTRIBUTE_ITEM_NUM - num].data = attribute->data;
		num--;
		
		data += (sizeof(struct Attribute) + attribute->len);
		total_len -= (sizeof(struct Attribute) + attribute->len);
	}
	if(num != 0 || total_len != 0)
		avrcp_log("parser error :num %d total_len %d\n",num, total_len);
	avrcp_ctrl_event_cb->update_id3_info(session, &info);
}

static void avrcp_verdor_rsp_handle(struct bt_avrcp *session,
								struct net_buf *buf)
{
	struct bt_avrcp_vendor_info *info = (void *)buf->data;

	avrcp_log("avrcp rsp pdu_id:0x%x\n", info->pdu_id);
	switch (info->pdu_id) {
	case BT_AVRCP_PDU_ID_GET_CAPABILITIES:
		avrcp_verdor_capabilities_rsp_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_GET_PLAY_STATUS:
		avrcp_verdor_play_status_rsp_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_REGISTER_NOTIFICATION:
		avrcp_verdor_notify_rsp_handle(session, buf);
		break;
	case BT_AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:
		avrcp_verdor_get_attributes_rsp_handle(session, buf);
		break;
	}
}

static void avrcp_vendor_dependent_handle(struct bt_avrcp *session,
				struct net_buf *buf, uint8_t msg_type)
{
	if (msg_type == BT_AVRCP_CMD) {
		avrcp_verdor_cmd_handle(session, buf);
	} else {
		avrcp_verdor_rsp_handle(session, buf);
	}
}

static void avrcp_unit_info_handle(struct bt_avrcp *session,
				struct net_buf *buf, uint8_t msg_type)
{
	if (msg_type == BT_AVRCP_CMD) {
		avrcp_unit_info_cmd_handle(session);
	} else {
		session->CT_state = BT_AVRCP_STATE_UNIT_INFO_ED;
		avrcp_state_sm_work(&session->req);
	}
}

static void avrcp_subunit_info_handle(struct bt_avrcp *session,
				struct net_buf *buf, uint8_t msg_type)
{
	if (msg_type == BT_AVRCP_CMD) {
		avrcp_subunit_info_cmd_handle(session);
	} else {
		session->CT_state = BT_AVRCP_STATE_SUBUNIT_INFO_ED;
		avrcp_state_sm_work(&session->req);
	}
}

static void avrcp_pass_through_handle(struct bt_avrcp *session,
				struct net_buf *buf, uint8_t msg_type)
{
	if (msg_type == BT_AVRCP_CMD) {
		avrcp_pass_through_cmd_handle(session, buf);
	} else {
		avrcp_pass_through_rsp_handle(session, buf);
	}
}

struct avrcp_opcode_handler {
	uint8_t opcode;
	void (*func)(struct bt_avrcp *session, struct net_buf *buf, uint8_t msg_type);
};

static const struct avrcp_opcode_handler handler[] = {
	{ BT_AVRCP_VENDOR_DEPENDENT_OPCODE, avrcp_vendor_dependent_handle },
	{ BT_AVRCP_UNIT_INFO_OPCODE, avrcp_unit_info_handle },
	{ BT_AVRCP_SUBUNIT_INFO_OPCODE, avrcp_subunit_info_handle },
	{ BT_AVRCP_PASS_THROUGH_OPCODE, avrcp_pass_through_handle },
};

static int bt_avrcp_l2cap_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	struct bt_avrcp *session = AVRCP_CHAN(chan);
	struct bt_avctp_header *avctphdr = (void *)buf->data;
	struct bt_avrcp_header *avrcphdr;
	uint8_t msg_type, i;

	if (buf->len < sizeof(struct bt_avctp_header)) {
		BT_ERR("Recvd Wrong AVCTP Header");
		return -EINVAL;
	}

	if ((avctphdr->ipid != 0) ||
		(sys_be16_to_cpu(avctphdr->pid) != BT_SDP_AV_REMOTE_SVCLASS)) {
		BT_ERR("AVCTP recv error, ipid:%d, pid:0x%x\n", avctphdr->ipid, sys_be16_to_cpu(avctphdr->pid));
		struct net_buf *rsp_buf;

		rsp_buf = avctp_create_pdu(session, BT_AVRCP_RESOPEN);
		if (!rsp_buf) {
			return -ENOMEM;
		}

		struct bt_avctp_header *rsphdr = (void *)rsp_buf->data;

		rsphdr->ipid = 1;
		rsphdr->pid = avctphdr->pid;
		avrcp_send(session, rsp_buf);
		return 0;
	}

	msg_type = avctphdr->cr;

	net_buf_pull(buf, sizeof(*avctphdr));
	avrcphdr = (void *)buf->data;

	avrcp_log("avrcp rev opcode:0x%x, msg:%d tid %d\n", avrcphdr->opcode, msg_type,avctphdr->tid);
	if (msg_type == BT_AVRCP_CMD) {
		session->tg_tid = avctphdr->tid;
	} else {
		if ((avrcphdr->opcode == BT_AVRCP_VENDOR_DEPENDENT_OPCODE) &&
			(avrcphdr->ctype == BT_AVRCP_CTYPE_CHANGED_STABLE)) {
			/* Target active notify change */
		} else if (avrcphdr->opcode != BT_AVRCP_PASS_THROUGH_OPCODE) {
			if (session->req.subunit_type != avrcphdr->subunit_type ||
				session->req.opcode != avrcphdr->opcode ||
				session->req.tid != avctphdr->tid) {
				avrcp_log("Peer mismatch rsp, expected subunit_type:%d(%d), opcode:%d(%d), tid:%d(%d)\n",
						session->req.subunit_type,avrcphdr->subunit_type, session->req.opcode,avrcphdr->opcode,
						session->req.tid,avctphdr->tid);
				//return;
			}else{
				k_delayed_work_cancel(&session->req.timeout_work);
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(handler); i++) {
		if (avrcphdr->opcode == handler[i].opcode) {
			handler[i].func(session, buf, msg_type);
			return 0;
		}
	}

	return 0;
}

int bt_avrcp_ctrl_l2cap_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	struct bt_avrcp *session = NULL;
	int result;
	static const struct bt_l2cap_chan_ops ops = {
		.connected = bt_avrcp_l2cap_connected,
		.disconnected = bt_avrcp_l2cap_disconnected,
		.recv = bt_avrcp_l2cap_recv,
	};

	BT_DBG("conn %p", conn);
	/* Get the AVRCP session from upper layer */
	result = avrcp_ctrl_event_cb->accept(conn, &session);
	if (result < 0) {
		return result;
	}
	session->br_chan.chan.ops = (struct bt_l2cap_chan_ops *)&ops;
	session->br_chan.rx.mtu = BT_AVRCP_MAX_MTU;
	*chan = &session->br_chan.chan;
	return 0;
}


static void bt_avrcp_l2cap_encrypt_changed(struct bt_l2cap_chan *chan, uint8_t status)
{
	BT_DBG("");
}

int bt_avrcp_connect(struct bt_conn *conn, struct bt_avrcp *session)
{
	static const struct bt_l2cap_chan_ops ops = {
		.connected = bt_avrcp_l2cap_connected,
		.disconnected = bt_avrcp_l2cap_disconnected,
		.encrypt_change = bt_avrcp_l2cap_encrypt_changed,
		.recv = bt_avrcp_l2cap_recv,
	};

	if (!session) {
		return -EINVAL;
	}

	session->br_chan.chan.ops = (struct bt_l2cap_chan_ops *)&ops;
	session->br_chan.rx.mtu = BT_AVRCP_MAX_MTU;
	session->br_chan.chan.required_sec_level = BT_SECURITY_L2;

	return bt_l2cap_chan_connect(conn, &session->br_chan.chan,
				     BT_L2CAP_PSM_AVCTP_CONTROL);
}

int bt_avrcp_disconnect(struct bt_avrcp *session)
{
	if (!session) {
		return -EINVAL;
	}

	BT_DBG("session %p", session);

	return bt_l2cap_chan_disconnect(&session->br_chan.chan);
}

int bt_avrcp_pass_through_cmd(struct bt_avrcp *session, avrcp_op_id opid, uint8_t pushedstate)
{
	struct net_buf *buf;

	avrcp_log("avrcp pass opid:0x%x, state:%d\n", opid, pushedstate);
	buf = avrcp_create_pass_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_CONTROL, opid, pushedstate);
	if (!buf) {
		return -ENOMEM;
	}

	return avrcp_send(session, buf);
}

/* Application will register its callback */
int bt_avrcp_ctrl_register(struct bt_avrcp_event_cb *cb)
{
	BT_DBG("");

	if (avrcp_ctrl_event_cb) {
		return -EALREADY;
	}

	avrcp_ctrl_event_cb = cb;

	return 0;
}

static void bt_avrcp_env_init(void)
{
	avrcp_ctrl_event_cb = NULL;
}

int bt_avrcp_init(void)
{
	int err;
	static struct bt_l2cap_server avrcp_l2cap = {
		.psm = BT_L2CAP_PSM_AVCTP_CONTROL,
		.sec_level = BT_SECURITY_L2,
		.accept = bt_avrcp_ctrl_l2cap_accept,
	};

	BT_DBG("");

	bt_avrcp_env_init();

	/* Register AVRCP PSM with L2CAP */
	err = bt_l2cap_br_server_register(&avrcp_l2cap);
	if (err < 0) {
		BT_ERR("AVRCP L2CAP Registration failed %d", err);
	}

	return err;
}

int bt_avrcp_get_unit_info(struct bt_avrcp *session)
{
	struct net_buf *buf;
	uint8_t param[5];

	buf = avrcp_create_unit_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_STATUS, BT_AVRCP_UNIT_INFO_OPCODE);
	if (!buf) {
		return -ENOMEM;
	}

	memset(param, 0xFF, 5);
	net_buf_add_mem(buf, param, sizeof(param));

	return avrcp_send(session, buf);
}

int bt_avrcp_get_subunit_info(struct bt_avrcp *session)
{
	struct net_buf *buf;
	uint8_t param[5];

	buf = avrcp_create_unit_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_STATUS, BT_AVRCP_SUBUNIT_INFO_OPCODE);
	if (!buf) {
		return -ENOMEM;
	}

	memset(param, 0xFF, 5);
	param[0] = 0x07;	/* page: 0, extension code:0x7 */
	net_buf_add_mem(buf, param, sizeof(param));

	return avrcp_send(session, buf);
}

int bt_avrcp_get_capabilities(struct bt_avrcp *session)
{
	struct net_buf *buf;

	buf = avrcp_create_vendor_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_STATUS, BT_AVRCP_PDU_ID_GET_CAPABILITIES);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_be16(buf, 1);
	net_buf_add_u8(buf, BT_AVRCP_CAPABILITY_ID_EVENT);

	return avrcp_send(session, buf);
}

int bt_avrcp_get_play_status(struct bt_avrcp *session)
{
	struct net_buf *buf;

	buf = avrcp_create_vendor_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_STATUS, BT_AVRCP_PDU_ID_GET_PLAY_STATUS);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_be16(buf, 0);
	return avrcp_send(session, buf);
}

int bt_avrcp_get_id3_info(struct bt_avrcp *session)
{
	struct net_buf *buf;
	uint8_t param[TOTAL_ATTRIBUTE_ITEM_NUM * 4] = {0};

	buf = avrcp_create_vendor_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_STATUS, BT_AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_be16(buf, sizeof(param) + 8 +1);
	net_buf_add_mem(buf, param, 8);/*Identifier 0x0 */
	net_buf_add_u8(buf, TOTAL_ATTRIBUTE_ITEM_NUM);/* NumAttributes */
	param[3] = BT_AVRCP_ATTRIBUTE_ID_TITLE;
	param[7] = BT_AVRCP_ATTRIBUTE_ID_ARTIST;
	param[11] = BT_AVRCP_ATTRIBUTE_ID_ALBUM;
	param[15] = BT_AVRCP_ATTRIBUTE_ID_GENRE;
	param[19] = BT_AVRCP_ATTRIBUTE_ID_TIME;
	net_buf_add_mem(buf, param, sizeof(param));
	return avrcp_send(session, buf);
}

int bt_avrcp_get_playback_pos(struct bt_avrcp *session)
{
	return bt_avrcp_register_notification(session, BT_AVRCP_EVENT_PLAYBACK_POS_CHANGED);
}

int bt_avrcp_register_notification(struct bt_avrcp *session, uint8_t event_id)
{
	struct net_buf *buf;
	uint32_t interval = (event_id == BT_AVRCP_EVENT_PLAYBACK_POS_CHANGED) ? 1 : 0;

	avrcp_log("avrcp register notify r_tg_ebitmap:0x%x, event_id:0x%x\n",
				session->r_tg_ebitmap, event_id);
	if (BT_AVRCP_EVENT_VOLUME_CHANGED != event_id) {
		if (!BT_AVRCP_EVENT_SUPPORT(session->r_tg_ebitmap, event_id)) {
			session->l_reg_notify_event &= ~(BT_AVRCP_EVENT_BIT_MAP(event_id));
			return -EINVAL;
		}
	}

	buf = avrcp_create_vendor_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_NOTIFY,
					BT_AVRCP_PDU_ID_REGISTER_NOTIFICATION);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_be16(buf, 5);
	net_buf_add_u8(buf, event_id);
	net_buf_add_be32(buf, interval);

	session->l_reg_notify_event |= BT_AVRCP_EVENT_BIT_MAP(event_id);
	return avrcp_send(session, buf);
}

int bt_avrcp_notify_change(struct bt_avrcp *session, uint8_t event_id, uint8_t *param, uint8_t len)
{
	struct net_buf *buf;

	/* TODO: only support notify volume change*/
	if (event_id != session->r_reg_notify_event)
		return -EINVAL;

	session->tg_tid = session->tg_notify_tid;
	buf = avrcp_create_vendor_pdu(session, BT_AVRCP_RESOPEN, BT_AVRCP_CTYPE_CHANGED_STABLE,
						BT_AVRCP_PDU_ID_REGISTER_NOTIFICATION);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_add_be16(buf, (len+1));
	net_buf_add_u8(buf, event_id);
	net_buf_add_mem(buf, param, len);

	return avrcp_send(session, buf);
}

int bt_avrcp_set_absolute_volume(struct bt_avrcp *session, uint32_t param)
{
	struct net_buf *buf;
	uint8_t len;
	union {
		uint8_t c_param[4];		/* 0:dev_type, 1:len, 2~3:data */
		int32_t i_param;
	} value;

	session->tg_tid = session->tg_notify_tid;
	buf = avrcp_create_vendor_pdu(session, BT_AVRCP_CMD, BT_AVRCP_CTYPE_CONTROL,
						BT_AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME);
	if (!buf) {
		return -ENOMEM;
	}

	value.i_param = (int32_t)param;
	len = value.c_param[1];
	net_buf_add_be16(buf, len);
	net_buf_add_u8(buf, (value.c_param[2] & 0x7F));
	if (len == 2) {
		net_buf_add_u8(buf, value.c_param[3]);
	}

	return avrcp_send(session, buf);
}

bool bt_avrcp_check_event_support(struct bt_avrcp *session, uint8_t event_id)
{
	return BT_AVRCP_EVENT_SUPPORT(session->r_tg_ebitmap, event_id) ? true : false;
}

static int bt_avrcp_send_timeout_handler(struct bt_avrcp *session,
					struct bt_avrcp_req *req)
{
	BT_DBG("");
	return 0;
}

static int bt_avrcp_state_sm(struct bt_avrcp *session, struct bt_avrcp_req *req)
{
	if (bt_internal_is_pts_test()) {
		return 0;
	}

	avrcp_log("avrcp sm state:%d\n", session->CT_state);
	switch (session->CT_state) {
	case BT_AVRCP_STATE_CONNECTED:
#if 0	/* No need get AVRCP status */
		if (bt_avrcp_get_unit_info(session) == 0) {
			session->CT_state = BT_AVRCP_STATE_UNIT_INFO_ING;
		}
		break;

	case BT_AVRCP_STATE_UNIT_INFO_ED:
		if (bt_avrcp_get_subunit_info(session) == 0) {
			session->CT_state = BT_AVRCP_STATE_SUBUNIT_INFO_ING;
		}
		break;

	case BT_AVRCP_STATE_SUBUNIT_INFO_ED:
#endif
		if (bt_avrcp_get_capabilities(session) == 0) {
			session->CT_state = BT_AVRCP_STATE_GET_CAPABILITIES_ING;
		}
		break;

	case BT_AVRCP_STATE_GET_CAPABILITIES_ED:
		if (bt_avrcp_register_notification(session, BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED) < 0) {
			break;
		}
		if (bt_avrcp_register_notification(session, BT_AVRCP_EVENT_TRACK_CHANGED) < 0) {
			break;
		}
		session->CT_state = BT_AVRCP_STATE_REGISTER_NOTIFICATION_ING;
		break;
	case BT_AVRCP_STATE_STATUS_CHANGED_ED:
		bt_avrcp_register_notification(session, BT_AVRCP_EVENT_PLAYBACK_STATUS_CHANGED);
		break;
	case BT_AVRCP_STATE_TRACK_CHANGED_ED:
		bt_avrcp_register_notification(session, BT_AVRCP_EVENT_TRACK_CHANGED);
		break;
	}

	return 0;
}
