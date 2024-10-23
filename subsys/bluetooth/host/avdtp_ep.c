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

#include <bluetooth/hci.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_AVDTP)
#define LOG_MODULE_NAME bt_avdtp_ep
#include "common/log.h"

#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "avdtp_internal.h"

#include <bluetooth/a2dp-codec.h>
#include <bluetooth/avdtp.h>

#define AVDTP_EP_DEBUG_LOG		1
#if AVDTP_EP_DEBUG_LOG
#define avdtp_ep_log(fmt, ...)		printk(fmt, ##__VA_ARGS__)
#else
#define avdtp_ep_log(fmt, ...)
#endif

#define BT_AVDTP_MIN_SEID 0x01
#define BT_AVDTP_MAX_SEID 0x3E

static uint8_t bt_avdtp_seid = BT_AVDTP_MIN_SEID;
static struct bt_avdtp_seid_lsep *lseps;

#define LSEP_FOREACH(lsep) \
		for ((lsep) = (lseps); (lsep); (lsep) = (lsep)->next)

bool bt_avdtp_ep_empty(void)
{
	return (lseps == NULL) ? true : false;
}

struct bt_avdtp_seid_lsep *find_lsep_by_seid(uint8_t seid)
{
	struct bt_avdtp_seid_lsep *lsep;

	LSEP_FOREACH(lsep) {
		if (lsep->sid.id == seid)
			return lsep;
	}

	return NULL;
}

struct bt_avdtp_seid_lsep *find_free_lsep_by_role(uint8_t role)
{
	struct bt_avdtp_seid_lsep *lsep;

	LSEP_FOREACH(lsep) {
		if ((lsep->sid.tsep == role) && (!lsep->sid.inuse) && (!lsep->ep_halt))
			return lsep;
	}

	return NULL;
}

struct bt_avdtp_seid_lsep *find_free_lsep_by_role_codectype(uint8_t role, uint8_t codectype)
{
	struct bt_avdtp_seid_lsep *lsep;

	LSEP_FOREACH(lsep) {
		if ((lsep->sid.tsep == role) &&
			(lsep->codec->head.codec_type == codectype) &&
			(!lsep->sid.inuse) && (!lsep->ep_halt))
			return lsep;
	}

	return NULL;
}

bool lsep_seid_inused(uint8_t seid)
{
	struct bt_avdtp_seid_lsep *lsep;

	LSEP_FOREACH(lsep) {
		if (lsep->sid.id == seid)
			return (lsep->sid.inuse) ? true : false;
	}

	return false;
}

bool lsep_set_seid_used_by_seid(uint8_t seid, struct bt_avdtp_stream *stream)
{
	struct bt_avdtp_seid_lsep *lsep;

	lsep = find_lsep_by_seid(seid);
	if (lsep) {
		lsep->sid.inuse = 1;
		stream->lsid.id = lsep->sid.id;
		stream->lsid.tsep = lsep->sid.tsep;
		return true;
	}

	return false;
}

bool lsep_set_seid_used_by_stream(struct bt_avdtp_stream *stream)
{
	struct bt_avdtp_seid_lsep *lsep;

	LSEP_FOREACH(lsep) {
		if ((!lsep->sid.inuse) && (!lsep->ep_halt) &&
			(lsep->sid.tsep == stream->lsid.tsep) &&
			(lsep->codec->head.codec_type == stream->codec.head.codec_type)) {
			lsep->sid.inuse = 1;
			stream->lsid.id = lsep->sid.id;
			return true;
		}
	}

	return false;
}

void lsep_set_seid_free(uint8_t seid)
{
	struct bt_avdtp_seid_lsep *lsep;

	lsep = find_lsep_by_seid(seid);
	if (lsep) {
		lsep->sid.inuse = 0;
	}
}

static int find_cap_codec(const uint8_t *data, uint8_t len,
							struct bt_a2dp_media_codec **codec)
{
	struct bt_avdtp_cap *cap;
	uint8_t i;

	for (i = 0; i < len; ) {
		cap = (struct bt_avdtp_cap *)&data[i];
		if (cap->cat == BT_AVDTP_SERVICE_CAT_MEDIA_CODEC) {
			*codec = (struct bt_a2dp_media_codec *)cap->data;
			return 0;
		}

		i += cap->len + 2;
	}

	return -EEXIST;
}

static int check_local_remote_codec_sbc(struct bt_a2dp_media_codec *lcodec,
							struct bt_a2dp_media_codec *rcodec,
							struct bt_a2dp_media_codec *setcodec, uint8_t sig_id)
{
	uint8_t bit_map;

	if (!((lcodec->sbc.channel_mode & rcodec->sbc.channel_mode) &&
		(lcodec->sbc.freq & rcodec->sbc.freq) &&
		(lcodec->sbc.alloc_method & rcodec->sbc.alloc_method) &&
		(lcodec->sbc.subbands & rcodec->sbc.subbands) &&
		(lcodec->sbc.block_len & rcodec->sbc.block_len))) {
		return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
	}

	/* Need compare bitpool ?? */
	if (sig_id == BT_AVDTP_SET_CONFIGURATION ||
		sig_id == BT_AVDTP_RECONFIGURE) {
		if (rcodec->sbc.min_bitpool < lcodec->sbc.min_bitpool ||
			rcodec->sbc.max_bitpool > lcodec->sbc.max_bitpool) {
			return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
		}
	}

	if (!setcodec) {
		return 0;
	}

	memset(setcodec, 0, sizeof(struct bt_a2dp_media_codec));
	setcodec->sbc.media_type = rcodec->sbc.media_type;
	setcodec->sbc.codec_type = rcodec->sbc.codec_type;

	bit_map = lcodec->sbc.freq & rcodec->sbc.freq;
	if (bit_map & BT_A2DP_SBC_44100) {
		setcodec->sbc.freq = BT_A2DP_SBC_44100;
	} else if (bit_map & BT_A2DP_SBC_48000) {
		setcodec->sbc.freq = BT_A2DP_SBC_48000;
	} else if (bit_map & BT_A2DP_SBC_32000) {
		setcodec->sbc.freq = BT_A2DP_SBC_32000;
	} else {
		setcodec->sbc.freq = BT_A2DP_SBC_16000;
	}

	bit_map = lcodec->sbc.channel_mode & rcodec->sbc.channel_mode;
	if (bit_map & BT_A2DP_SBC_JOINT_STEREO) {
		setcodec->sbc.channel_mode = BT_A2DP_SBC_JOINT_STEREO;
	} else if (bit_map & BT_A2DP_SBC_STEREO) {
		setcodec->sbc.channel_mode = BT_A2DP_SBC_STEREO;
	} else if (bit_map & BT_A2DP_SBC_DUAL_CHANNEL) {
		setcodec->sbc.channel_mode = BT_A2DP_SBC_DUAL_CHANNEL;
	} else {
		setcodec->sbc.channel_mode = BT_A2DP_SBC_MONO;
	}

	bit_map = lcodec->sbc.block_len & rcodec->sbc.block_len;
	if (bit_map & BT_A2DP_SBC_BLOCK_LENGTH_16) {
		setcodec->sbc.block_len = BT_A2DP_SBC_BLOCK_LENGTH_16;
	} else if (bit_map & BT_A2DP_SBC_BLOCK_LENGTH_12) {
		setcodec->sbc.block_len = BT_A2DP_SBC_BLOCK_LENGTH_12;
	} else if (bit_map & BT_A2DP_SBC_BLOCK_LENGTH_8) {
		setcodec->sbc.block_len = BT_A2DP_SBC_BLOCK_LENGTH_8;
	} else {
		setcodec->sbc.block_len = BT_A2DP_SBC_BLOCK_LENGTH_4;
	}

	bit_map = lcodec->sbc.subbands & rcodec->sbc.subbands;
	if (bit_map & BT_A2DP_SBC_SUBBANDS_8) {
		setcodec->sbc.subbands = BT_A2DP_SBC_SUBBANDS_8;
	} else {
		setcodec->sbc.subbands = BT_A2DP_SBC_SUBBANDS_4;
	}

	bit_map = lcodec->sbc.alloc_method & rcodec->sbc.alloc_method;
	if (bit_map & BT_A2DP_SBC_ALLOCATION_METHOD_LOUDNESS) {
		setcodec->sbc.alloc_method = BT_A2DP_SBC_ALLOCATION_METHOD_LOUDNESS;
	} else {
		setcodec->sbc.alloc_method = BT_A2DP_SBC_ALLOCATION_METHOD_SNR;
	}

	setcodec->sbc.min_bitpool = MAX(rcodec->sbc.min_bitpool, lcodec->sbc.min_bitpool);
	setcodec->sbc.max_bitpool = MIN(rcodec->sbc.max_bitpool, lcodec->sbc.max_bitpool);

	return 0;
}

static int cal_bitmap_bits(uint32_t bitmap, uint8_t bit_len)
{
	int i, bits = 0;

	for (i=0; (i<32 && i<bit_len); i++) {
		if (bitmap & (0x1 << i)) {
			bits++;
		}
	}

	return bits;
}

static int check_local_remote_codec_aac(struct bt_a2dp_media_codec *lcodec,
							struct bt_a2dp_media_codec *rcodec,
							struct bt_a2dp_media_codec *setcodec, uint8_t sig_id)
{
	uint8_t get_cap_rsp, check_bitrate, check_obj, check_freq, check_channels, i;
	uint16_t Lfreq, Rfreq;
	uint32_t Lbitrate, Rbitrate;

	/* sig_id just get cap respone or set/reset config command */
	get_cap_rsp = ((sig_id == BT_AVDTP_GET_CAPABILITIES) || (sig_id == BT_AVDTP_GET_ALL_CAPABILITIES)) ? 1 : 0;

	Lfreq = (lcodec->aac.freq0 << 4) | lcodec->aac.freq1;
	Rfreq = (rcodec->aac.freq0 << 4) | rcodec->aac.freq1;
	Lbitrate = (lcodec->aac.bit_rate0 << 16) | (lcodec->aac.bit_rate1 << 8) | lcodec->aac.bit_rate2;
	Rbitrate = (rcodec->aac.bit_rate0 << 16) | (rcodec->aac.bit_rate1 << 8) | rcodec->aac.bit_rate2;

	check_bitrate = 1;

	if (!get_cap_rsp) {
		if (lcodec->aac.vbr == 0) {
			if (rcodec->aac.vbr == 0) {
				check_bitrate = 1;
			} else {
				check_bitrate = 0;
			}
		} else {
			if (rcodec->aac.vbr == 0) {
				check_bitrate = 1;
			} else {
				check_bitrate = ((~Lbitrate) & Rbitrate) ? 0 : 1;
			}
		}
	}

	if (cal_bitmap_bits(Rbitrate, 23) == 0) {
		check_bitrate = 0;
	}

	check_obj = (lcodec->aac.obj_type & rcodec->aac.obj_type) ? 1 : 0;
	check_channels = (lcodec->aac.channels & rcodec->aac.channels) ? 1 : 0;
	check_freq = (Lfreq & Rfreq) ? 1 : 0;
	if (!get_cap_rsp) {
		check_obj = (cal_bitmap_bits(rcodec->aac.obj_type, 8) == 1) ? check_obj : 0;
		check_channels = (cal_bitmap_bits(rcodec->aac.channels, 2) == 1) ? check_channels : 0;
		check_freq = (cal_bitmap_bits(Rfreq, 16) == 1) ? check_freq : 0;
	}

	if (!(check_obj && check_channels && check_freq && check_bitrate)) {
		return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
	}

	if (!setcodec) {
		return 0;
	}

	memset(setcodec, 0, sizeof(struct bt_a2dp_media_codec));
	if (!get_cap_rsp) {
		memcpy(&setcodec->aac, &rcodec->aac, sizeof(rcodec->aac));
		return 0;
	}

	setcodec->aac.media_type = rcodec->aac.media_type;
	setcodec->aac.codec_type = rcodec->aac.codec_type;
	for (i = 0; i < 4; i++) {
		if (lcodec->aac.obj_type & rcodec->aac.obj_type & (BT_A2DP_AAC_OBJ_MPEG2_AAC_LC >> i)) {
			setcodec->aac.obj_type = (BT_A2DP_AAC_OBJ_MPEG2_AAC_LC >> i);
			break;
		}
	}

	if (lcodec->aac.channels & rcodec->aac.channels & BT_A2DP_AAC_CHANNELS_2) {
		setcodec->aac.channels = BT_A2DP_AAC_CHANNELS_2;
	} else {
		setcodec->aac.channels = BT_A2DP_AAC_CHANNELS_1;
	}

	Lfreq = Lfreq & Rfreq;
	for (i = 0; i < 12; i++) {
		if (Lfreq & (BT_A2DP_AAC_96000 << i)) {
			setcodec->aac.freq0 = ((BT_A2DP_AAC_96000 << i) >> 4) & 0xFF;
			setcodec->aac.freq1 = (BT_A2DP_AAC_96000 << i) & 0xF;
			break;
		}
	}

	setcodec->aac.vbr = lcodec->aac.vbr & rcodec->aac.vbr;
	Lbitrate = Lbitrate & Rbitrate;
	setcodec->aac.bit_rate0 = (Lbitrate >> 16) & 0x7F;
	setcodec->aac.bit_rate1 = (Lbitrate >> 8) & 0xFF;
	setcodec->aac.bit_rate2 = Lbitrate & 0xFF;

	return 0;
}

static int check_local_remote_codec(struct bt_a2dp_media_codec *lcodec,
							struct bt_a2dp_media_codec *rcodec,
							struct bt_a2dp_media_codec *setcodec, uint8_t sig_id)
{
	if (!lcodec || !rcodec ||
		(lcodec->head.media_type != rcodec->head.media_type) ||
		(lcodec->head.codec_type != rcodec->head.codec_type)) {
		return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
	}

	switch (lcodec->head.codec_type) {
	case BT_A2DP_SBC:
		return check_local_remote_codec_sbc(lcodec, rcodec, setcodec, sig_id);

	case BT_A2DP_MPEG2:
		return check_local_remote_codec_aac(lcodec, rcodec, setcodec, sig_id);

	default:
		return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
	}
}

static int check_local_remote_cp_type(struct net_buf *buf, struct bt_avdtp_seid_lsep *lsep, uint8_t *cp_type, uint8_t sig_id)
{
	struct bt_avdtp_cap *cap;
	uint8_t i, len, *data, parse_cp_type = BT_AVDTP_AV_CP_TYPE_NONE;
	uint16_t rx_cp_type = 0xFFFF;

	data = buf->data;
	len = buf->len;

	for (i = 0; i < len; ) {
		cap = (struct bt_avdtp_cap *)&data[i];
		if (cap->cat == BT_AVDTP_SERVICE_CAT_CONTENT_PROTECTION) {
			rx_cp_type = cap->data[0] | (cap->data[1] << 8);
			if (rx_cp_type == BT_AVDTP_AV_CP_TYPE_SCMS_T) {
				parse_cp_type = BT_AVDTP_AV_CP_TYPE_SCMS_T;
				break;
			}
		}

		i += cap->len + 2;
	}

	if (rx_cp_type == 0xFFFF && sig_id == BT_AVDTP_RECONFIGURE) {
		/* Reconfig command without CAT_CONTENT_PROTECTION
		 * Not need change  cp_type.
		 */
		return 0;
	}

	if (parse_cp_type == BT_AVDTP_AV_CP_TYPE_NONE) {
		*cp_type = parse_cp_type;
		return 0;
	} else if (sig_id == BT_AVDTP_SET_CONFIGURATION ||
				sig_id == BT_AVDTP_RECONFIGURE) {
		if (parse_cp_type == BT_AVDTP_AV_CP_TYPE_SCMS_T && lsep->a2dp_cp_scms_t) {
			*cp_type = parse_cp_type;
			return 0;
		} else {
			*cp_type = BT_AVDTP_AV_CP_TYPE_NONE;
			return -EINVAL;
		}
	} else {
		if (parse_cp_type == BT_AVDTP_AV_CP_TYPE_SCMS_T && lsep->a2dp_cp_scms_t) {
			*cp_type = parse_cp_type;
		} else {
			*cp_type = BT_AVDTP_AV_CP_TYPE_NONE;
		}
		return 0;
	}
}

static uint8_t check_local_remote_delay_report(struct net_buf *buf, struct bt_avdtp_seid_lsep *lsep,
										uint8_t sig_id, uint8_t old_delay_report)
{
	struct bt_avdtp_cap *cap;
	uint8_t i, len, *data, rx_delay_report = 0;

	if (!lsep->a2dp_delay_report) {
		return rx_delay_report;
	}

	data = buf->data;
	len = buf->len;

	for (i = 0; i < len; ) {
		cap = (struct bt_avdtp_cap *)&data[i];
		if (cap->cat == BT_AVDTP_SERVICE_CAT_DELAYREPORTING) {
			rx_delay_report = 1;
			break;
		}

		i += cap->len + 2;
	}

	if (rx_delay_report == 0 && sig_id == BT_AVDTP_RECONFIGURE) {
		/* Reconfig command without delay report,
		 * Not need change  delay report.
		 */
		return old_delay_report;
	}

	return rx_delay_report;
}

static int bt_avdtp_ep_check_cfg_media_codec(struct bt_a2dp_media_codec *rcodec)
{
	switch (rcodec->head.codec_type) {
	case BT_A2DP_SBC:
	case BT_A2DP_MPEG2:
		break;
	case BT_A2DP_MPEG1:
	case BT_A2DP_ATRAC:
		return -BT_AVDTP_ERR_NOT_SUPPORTED_CODEC_TYPE;
	case BT_A2DP_VENDOR:
	default:
		return -BT_AVDTP_ERR_INVALID_CODEC_TYPE;
	};

	if (rcodec->head.codec_type == BT_A2DP_SBC) {
		if (cal_bitmap_bits(rcodec->sbc.freq, 4) != 1) {
			return -BT_AVDTP_ERR_INVALID_SAMPLING_FREQUENCY;
		} else if (cal_bitmap_bits(rcodec->sbc.channel_mode, 4) != 1) {
			return -BT_AVDTP_ERR_INVALID_CHANNEL_MODE;
		} else if (cal_bitmap_bits(rcodec->sbc.block_len, 4) != 1) {
			return -BT_AVDTP_ERR_INVALID_BLOCK_LENGTH;
		} else if (cal_bitmap_bits(rcodec->sbc.subbands, 2) != 1) {
			return -BT_AVDTP_ERR_INVALID_SUBBANDS;
		} else if (cal_bitmap_bits(rcodec->sbc.alloc_method, 2) != 1) {
			return -BT_AVDTP_ERR_INVALID_ALLOCATION_METHOD;
		} else if (rcodec->sbc.min_bitpool < BT_AVDTP_MIX_BITPOOL || rcodec->sbc.min_bitpool > BT_AVDTP_MAX_BITPOOL) {
			return -BT_AVDTP_ERR_INVALID_MINIMUM_BITPOOL_VALUE;
		} else if (rcodec->sbc.max_bitpool < BT_AVDTP_MIX_BITPOOL || rcodec->sbc.max_bitpool > BT_AVDTP_MAX_BITPOOL) {
			return -BT_AVDTP_ERR_INVALID_MAXIMUM_BITPOOL_VALUE;
		}
	}

	return 0;
}

int bt_avdtp_ep_check_set_codec_cp(struct bt_avdtp *session, struct net_buf *buf, uint8_t acp_seid, uint8_t sig_id)
{
	int ret;
	struct bt_avdtp_seid_lsep *lsep;
	struct bt_a2dp_media_codec *rcodec;
	struct bt_a2dp_media_codec setcodec;
	struct bt_avdtp_conn *pAvdtp_conn = AVDTP_CONN_BY_SIGNAL(session);

	if (find_cap_codec(buf->data, buf->len, &rcodec)) {
		return -BT_AVDTP_ERR_BAD_SERV_CATEGORY;
	}

	if (sig_id == BT_AVDTP_SET_CONFIGURATION ||
		sig_id == BT_AVDTP_RECONFIGURE) {
		ret = bt_avdtp_ep_check_cfg_media_codec(rcodec);
		if (ret) {
			return ret;
		}
	}

	if (acp_seid) {
		lsep = find_lsep_by_seid(acp_seid);
	} else {
		lsep = find_free_lsep_by_role_codectype(pAvdtp_conn->stream.lsid.tsep, rcodec->head.codec_type);
	}

	if (!lsep) {
		return -BT_AVDTP_ERR_SEP_IN_USE;
	}

	if (check_local_remote_codec(lsep->codec, rcodec, &setcodec, sig_id)) {
		return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
	}

	if (check_local_remote_cp_type(buf, lsep, &pAvdtp_conn->stream.cp_type, sig_id)) {
		return -BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION;
	}

	pAvdtp_conn->stream.delay_report = check_local_remote_delay_report(buf, lsep, sig_id, pAvdtp_conn->stream.delay_report);

	memcpy(&pAvdtp_conn->stream.codec, &setcodec, sizeof(struct bt_a2dp_media_codec));
	return 0;
}

void bt_avdtp_ep_append_seid(struct net_buf *resp_buf)
{
	struct bt_avdtp_seid_lsep *lsep;
	int add = 0;

	LSEP_FOREACH(lsep) {
		/* TO DO:macbook may choose inuse seid,so here we don't add inuse seid */
		if (!lsep->sid.inuse && !lsep->ep_halt) {
			net_buf_add_mem(resp_buf, &lsep->sid,
				sizeof(struct bt_avdtp_seid_info));
			add = 1;
		}
	}

	/* There shall be at least one SEP in an AVDTP_DISCOVER_RSP
	 * AVDTP spec 8.6.2
	 */
	if (!add) {
		LSEP_FOREACH(lsep) {
			if (!lsep->ep_halt) {
				net_buf_add_mem(resp_buf, &lsep->sid,
					sizeof(struct bt_avdtp_seid_info));
			}
		}
	}
}

void bt_avdtp_ep_append_capabilities(struct net_buf *resp_buf, uint8_t reqSeid)
{
	struct bt_avdtp_cap cap;
	struct bt_avdtp_seid_lsep *lsep = find_lsep_by_seid(reqSeid);	/* Run to here, lsep exist */
	uint8_t codec_len;

	/* Add MEDIA_TRANSPORT */
	cap.cat = BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT;
	cap.len = 0;
	net_buf_add_mem(resp_buf, &cap, sizeof(struct bt_avdtp_cap));

	/* Add MEDIA_CODEC */
	if (lsep->codec->head.codec_type == BT_A2DP_SBC) {
		codec_len = sizeof(struct bt_a2dp_media_sbc_codec);
	} else if (lsep->codec->head.codec_type == BT_A2DP_MPEG2) {
		codec_len = sizeof(struct bt_a2dp_media_aac_codec);
	} else {
		codec_len = 0;
	}

	cap.cat = BT_AVDTP_SERVICE_CAT_MEDIA_CODEC;
	cap.len = codec_len;
	net_buf_add_mem(resp_buf, &cap, sizeof(struct bt_avdtp_cap));
	if (codec_len) {
		net_buf_add_mem(resp_buf, (const void *)lsep->codec, codec_len);
	}

	/* Add content protection SCMS-T */
	if (lsep->a2dp_cp_scms_t) {
		cap.cat = BT_AVDTP_SERVICE_CAT_CONTENT_PROTECTION;
		cap.len = 2;
		net_buf_add_mem(resp_buf, &cap, sizeof(struct bt_avdtp_cap));
		net_buf_add_le16(resp_buf, BT_AVDTP_AV_CP_TYPE_SCMS_T);
	}

	/* Add Delay Reporting Capabilities */
	if (lsep->a2dp_delay_report) {
		cap.cat = BT_AVDTP_SERVICE_CAT_DELAYREPORTING;
		cap.len = 0;
		net_buf_add_mem(resp_buf, &cap, sizeof(struct bt_avdtp_cap));
	}
}

uint8_t bt_avdtp_ep_get_codec_len(struct bt_a2dp_media_codec *codec)
{
	switch (codec->head.codec_type) {
	case BT_A2DP_SBC:
		return sizeof(struct bt_a2dp_media_sbc_codec);
	case BT_A2DP_MPEG2:
		return sizeof(struct bt_a2dp_media_aac_codec);
	default:
		break;
	}

	return 0;
}

int bt_avdtp_ep_register_sep(uint8_t media_type, uint8_t role,
			  struct bt_avdtp_seid_lsep *lsep)
{
	BT_DBG("");

	if (!lsep) {
		return -EIO;
	}

	if (bt_avdtp_seid == BT_AVDTP_MAX_SEID) {
		return -EIO;
	}

	lsep->sid.id = bt_avdtp_seid++;
	lsep->sid.inuse = 0;
	lsep->sid.media_type = media_type;
	lsep->sid.tsep = role;
	lsep->ep_halt = 0;

	lsep->next = lseps;
	lseps = lsep;

	return 0;
}

int bt_avdtp_ep_halt_sep(struct bt_avdtp_seid_lsep *lsep, bool halt)
{
	if (!lsep) {
		return -EIO;
	}

	if (halt) {
		if (lsep->sid.inuse) {
			BT_ERR("sep busy!");
			return -EBUSY;
		} else {
			lsep->ep_halt = 1;
		}
	} else {
		lsep->ep_halt = 0;
	}

	return 0;
}

void bt_avdtp_ep_env_init(void)
{
	bt_avdtp_seid = BT_AVDTP_MIN_SEID;
	lseps = NULL;
}
