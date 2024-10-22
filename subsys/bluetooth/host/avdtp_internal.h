/*
 * avdtp_internal.h - avdtp handling

 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bluetooth/avdtp.h>

/* @brief A2DP ROLE's */
#define A2DP_SRC_ROLE 0x00
#define A2DP_SNK_ROLE 0x01

/* @brief AVDTP Role */
#define BT_AVDTP_INT 0x00
#define BT_AVDTP_ACP 0x01

#define BT_L2CAP_PSM_AVDTP 0x0019

/* AVDTP SIGNAL HEADER - Packet Type*/
#define BT_AVDTP_PACKET_TYPE_SINGLE   0x00
#define BT_AVDTP_PACKET_TYPE_START    0x01
#define BT_AVDTP_PACKET_TYPE_CONTINUE 0x02
#define BT_AVDTP_PACKET_TYPE_END      0x03

/* AVDTP SIGNAL HEADER - MESSAGE TYPE */
#define BT_AVDTP_CMD        0x00
#define BT_AVDTP_GEN_REJECT 0x01
#define BT_AVDTP_ACCEPT     0x02
#define BT_AVDTP_REJECT     0x03

/* @brief AVDTP SIGNAL HEADER - Signal Identifier */
#define BT_AVDTP_DISCOVER             0x01
#define BT_AVDTP_GET_CAPABILITIES     0x02
#define BT_AVDTP_SET_CONFIGURATION    0x03
#define BT_AVDTP_GET_CONFIGURATION    0x04
#define BT_AVDTP_RECONFIGURE          0x05
#define BT_AVDTP_OPEN                 0x06
#define BT_AVDTP_START                0x07
#define BT_AVDTP_CLOSE                0x08
#define BT_AVDTP_SUSPEND              0x09
#define BT_AVDTP_ABORT                0x0a
#define BT_AVDTP_SECURITY_CONTROL     0x0b
#define BT_AVDTP_GET_ALL_CAPABILITIES 0x0c
#define BT_AVDTP_DELAYREPORT          0x0d

/* Actions add command, just for notify upper layer */
#define BT_AVDTP_PENDING_AHEAD_START  0x80

/* @brief AVDTP STREAM STATE */
#define BT_AVDTP_STREAM_STATE_UNUSED      0x00
#define BT_AVDTP_STREAM_STATE_IDLE        0x01
#define BT_AVDTP_STREAM_STATE_CONFIGURED  0x02
#define BT_AVDTP_STREAM_STATE_OPEN        0x03
#define BT_AVDTP_STREAM_STATE_STREAMING   0x04
#define BT_AVDTP_STREAM_STATE_CLOSING     0x05
#define BT_AVDTP_STREAM_STATE_CLOSED      0x06
#define BT_AVDTP_STREAM_STATE_SUSPEND     0x07
#define BT_AVDTP_STREAM_STATE_ABORTING    0x08

#define BT_AVDTP_SIG_ID_TO_STATE_ING(x)		  (((x) << 4) | 0x01)
#define BT_AVDTP_SIG_ID_TO_STATE_ED(x)		  (((x) << 4) | 0x02)
#define BT_AVDTP_SIG_ID_TO_STATE_EXT(x)		  (((x) << 4) | 0x03)
#define BT_AVDTP_IS_ACPINT_STATE_ING(x)			((x&0x0F) == 0x01)
#define BT_AVDTP_IS_ACPINT_STATE_ED(x)			((x&0x0F) == 0x02)
#define BT_AVDTP_IS_ACPINT_STATE_EXT(x)			((x&0x0F) == 0x03)

/* @brief AVDTP ACCEPTOR/INITIATOR STATE */
#define BT_AVDTP_ACPINT_STATE_IDLE              0x00		/* ACP/INT in idle state, not send/receive cmd */
#define BT_AVDTP_ACPINT_STATE_DISCOVERING       BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_DISCOVER)		/* Send/Receive discover command */
#define BT_AVDTP_ACPINT_STATE_DISCOVERED        BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_DISCOVER)		/* Discover command process finish */
#define BT_AVDTP_ACPINT_STATE_GET_CAPING        BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_GET_CAPABILITIES)
#define BT_AVDTP_ACPINT_STATE_GET_CAPED         BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_GET_CAPABILITIES)
#define BT_AVDTP_ACPINT_STATE_GET_CAPEXT        BT_AVDTP_SIG_ID_TO_STATE_EXT(BT_AVDTP_GET_CAPABILITIES)
#define BT_AVDTP_ACPINT_STATE_SET_CFGING        BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_SET_CONFIGURATION)
#define BT_AVDTP_ACPINT_STATE_SET_CFGED         BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_SET_CONFIGURATION)
#define BT_AVDTP_ACPINT_STATE_GET_CFGING        BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_GET_CONFIGURATION)
#define BT_AVDTP_ACPINT_STATE_GET_CFGED         BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_GET_CONFIGURATION)
#define BT_AVDTP_ACPINT_STATE_RECFGING          BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_RECONFIGURE)
#define BT_AVDTP_ACPINT_STATE_RECFGED           BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_RECONFIGURE)
#define BT_AVDTP_ACPINT_STATE_OPENING           BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_OPEN)
#define BT_AVDTP_ACPINT_STATE_OPENED            BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_OPEN)
#define BT_AVDTP_ACPINT_STATE_STARTING          BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_START)
#define BT_AVDTP_ACPINT_STATE_STARTED           BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_START)
#define BT_AVDTP_ACPINT_STATE_CLOSEING          BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_CLOSE)
#define BT_AVDTP_ACPINT_STATE_CLOSEED           BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_CLOSE)
#define BT_AVDTP_ACPINT_STATE_SUSPENDING        BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_SUSPEND)
#define BT_AVDTP_ACPINT_STATE_SUSPENDED         BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_SUSPEND)
#define BT_AVDTP_ACPINT_STATE_ABORTING          BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_ABORT)
#define BT_AVDTP_ACPINT_STATE_ABORTED           BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_ABORT)
#define BT_AVDTP_ACPINT_STATE_SECTRLING         BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_SECURITY_CONTROL)
#define BT_AVDTP_ACPINT_STATE_SECTRLED          BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_SECURITY_CONTROL)
#define BT_AVDTP_ACPINT_STATE_GET_ACFGING       BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_GET_ALL_CAPABILITIES)
#define BT_AVDTP_ACPINT_STATE_GET_ACFGED        BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_GET_ALL_CAPABILITIES)
#define BT_AVDTP_ACPINT_STATE_GET_ACFGEXT       BT_AVDTP_SIG_ID_TO_STATE_EXT(BT_AVDTP_GET_ALL_CAPABILITIES)
#define BT_AVDTP_ACPINT_STATE_DEREPORTING       BT_AVDTP_SIG_ID_TO_STATE_ING(BT_AVDTP_DELAYREPORT)
#define BT_AVDTP_ACPINT_STATE_DEREPORTED        BT_AVDTP_SIG_ID_TO_STATE_ED(BT_AVDTP_DELAYREPORT)

/* @brief AVDTP Media TYPE */
#define BT_AVDTP_SERVICE_CAT_MEDIA_TRANSPORT    0x01
#define BT_AVDTP_SERVICE_CAT_REPORTING          0x02
#define BT_AVDTP_SERVICE_CAT_RECOVERY           0x03
#define BT_AVDTP_SERVICE_CAT_CONTENT_PROTECTION 0x04
#define BT_AVDTP_SERVICE_CAT_HDR_COMPRESSION    0x05
#define BT_AVDTP_SERVICE_CAT_MULTIPLEXING       0x06
#define BT_AVDTP_SERVICE_CAT_MEDIA_CODEC        0x07
#define BT_AVDTP_SERVICE_CAT_DELAYREPORTING     0x08
#define BT_AVDTP_SERVICE_CAT_MAX                BT_AVDTP_SERVICE_CAT_DELAYREPORTING

/* AVDTP Error Codes */
#define BT_AVDTP_SUCCESS                        0x00
#define BT_AVDTP_ERR_BAD_HDR_FORMAT             0x01
#define BT_AVDTP_ERR_BAD_LENGTH                 0x11
#define BT_AVDTP_ERR_BAD_ACP_SEID               0x12
#define BT_AVDTP_ERR_SEP_IN_USE                 0x13
#define BT_AVDTP_ERR_SEP_NOT_IN_USE             0x14
#define BT_AVDTP_ERR_BAD_SERV_CATEGORY          0x17
#define BT_AVDTP_ERR_BAD_PAYLOAD_FORMAT         0x18
#define BT_AVDTP_ERR_NOT_SUPPORTED_COMMAND      0x19
#define BT_AVDTP_ERR_INVALID_CAPABILITIES       0x1a
#define BT_AVDTP_ERR_BAD_RECOVERY_TYPE          0x22
#define BT_AVDTP_ERR_BAD_MEDIA_TRANSPORT_FORMAT 0x23
#define BT_AVDTP_ERR_BAD_RECOVERY_FORMAT        0x25
#define BT_AVDTP_ERR_BAD_ROHC_FORMAT            0x26
#define BT_AVDTP_ERR_BAD_CP_FORMAT              0x27
#define BT_AVDTP_ERR_BAD_MULTIPLEXING_FORMAT    0x28
#define BT_AVDTP_ERR_UNSUPPORTED_CONFIGURAION   0x29
#define BT_AVDTP_ERR_BAD_STATE                  0x31
#define BT_AVDTP_ERR_INVALID_CODEC_TYPE                      0xC1
#define BT_AVDTP_ERR_NOT_SUPPORTED_CODEC_TYPE                0xC2
#define BT_AVDTP_ERR_INVALID_SAMPLING_FREQUENCY              0xC3
#define BT_AVDTP_ERR_NOT_SUPPORTED_SAMPLING_FREQUENCY        0xC4
#define BT_AVDTP_ERR_INVALID_CHANNEL_MODE                    0xC5
#define BT_AVDTP_ERR_NOT_SUPPORTED_CHANNEL_MODE              0xC6
#define BT_AVDTP_ERR_INVALID_SUBBANDS                        0xC7
#define BT_AVDTP_ERR_NOT_SUPPORTED_SUBBANDS                  0xC8
#define BT_AVDTP_ERR_INVALID_ALLOCATION_METHOD               0xC9
#define BT_AVDTP_ERR_NOT_SUPPORTED_ALLOCATION_METHOD         0xCA
#define BT_AVDTP_ERR_INVALID_MINIMUM_BITPOOL_VALUE           0xCB
#define BT_AVDTP_ERR_NOT_SUPPORTED_MINIMUM_BITPOOL_VALUE     0xCC
#define BT_AVDTP_ERR_INVALID_MAXIMUM_BITPOOL_VALUE           0xCD
#define BT_AVDTP_ERR_NOT_SUPPORTED_MAXIMUM_BITPOOL_VALUE     0xCE
#define BT_AVDTP_ERR_INVALID_BLOCK_LENGTH                    0xDD
#define BT_AVDTP_ERR_INVALID_CP_TYPE                         0xE0
#define BT_AVDTP_ERR_INVALID_CP_FORMAT                       0xE1
#define BT_AVDTP_ERR_INVALID_CODEC_PARAMETER                 0xE2
#define BT_AVDTP_ERR_NOT_SUPPORTED_CODEC_PARAMETER           0xE3

#define BT_AVDTP_MIX_BITPOOL         2
#define BT_AVDTP_MAX_BITPOOL         53

#define BT_AVDTP_MAX_MTU CONFIG_BT_L2CAP_RX_MTU
#define BT_AVDTP_GET_SEID_MAX                   10

enum {
	BT_AVDTP_STATE_CLOSE = 0,
	BT_AVDTP_STATE_DISCOVER,
	BT_AVDTP_STATE_GET_CAPABILITIES,
	BT_AVDTP_STATE_CONFIGURATION,
	BT_AVDTP_STATE_RECONFIGURE,
	BT_AVDTP_STATE_STREAM_OPEN,
	BT_AVDTP_STATE_STREAM_START,
	BT_AVDTP_STATE_STREAM_SUSPEND,
	BT_AVDTP_STATE_STREAM_CLOSE,
	BT_AVDTP_STATE_DELAYREPORT,
};

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
struct bt_avdtp_get_capabilities_req {
	uint8_t rfa0:2;
	uint8_t seid:6;
} __packed;

typedef struct bt_avdtp_get_capabilities_req bt_avdtp_open_req;

struct bt_avdtp_setconf_req {
	uint8_t rfa0:2;
	uint8_t acp_seid:6;
	uint8_t rfa1:2;
	uint8_t int_seid:6;
	uint8_t caps[0];
} __packed;

struct bt_avdtp_reconf_req {
	uint8_t rfa0:2;
	uint8_t acp_seid:6;
	uint8_t caps[0];
} __packed;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
struct bt_avdtp_setconf_req {
	uint8_t acp_seid:6;
	uint8_t rfa0:2;
	uint8_t int_seid:6;
	uint8_t rfa1:2;
	uint8_t caps[0];
} __packed;

struct bt_avdtp_reconf_req {
	uint8_t acp_seid:6;
	uint8_t rfa0:2;
	uint8_t caps[0];
} __packed;

#endif

struct bt_avdtp;
struct bt_avdtp_req;
struct bt_avdtp_cap;

typedef int (*bt_avdtp_func_t)(struct bt_avdtp *session,
			       struct bt_avdtp_req *req);

struct bt_avdtp_req {
	uint8_t cmdtid;
	uint8_t sig;
	uint8_t tid;
	uint8_t msg_type;
	bt_avdtp_func_t func;
	struct k_delayed_work timeout_work;
	bt_avdtp_func_t state_sm_func;
};

struct bt_avdtp_conf_rej {
	uint8_t category;
	uint8_t error;
} __packed;

struct bt_avdtp_single_sig_hdr {
	uint8_t hdr;
	uint8_t signal_id;
} __packed;

#define BT_AVDTP_SIG_HDR_LEN sizeof(struct bt_avdtp_single_sig_hdr)

struct bt_avdtp_cap {
	uint8_t cat;
	uint8_t len;
	uint8_t data[0];
};

struct bt_avdtp_sep {
	uint8_t seid;
	uint8_t len;
	struct bt_avdtp_cap caps[0];
};

/** @brief Global AVDTP session structure. */
struct bt_avdtp {
	struct bt_l2cap_br_chan br_chan;
	uint8_t session_priority:3;
	uint8_t role:3;				/* Source/sink/media role */
	uint8_t intacp_role:1;			/* As initial or accept */
	uint8_t connected:1;			/* A2dp connect or not */
};

struct bt_avdtp_conn {
	struct bt_avdtp signal_session;
	struct bt_avdtp media_session;
	struct bt_avdtp_stream stream;
	struct bt_avdtp_req req;
	struct bt_avdtp_seid_info get_seid[BT_AVDTP_GET_SEID_MAX];
	uint8_t get_seid_num:4;
	uint8_t get_rsid_cap_index:4;
	uint8_t pending_ahead_start:1;
	struct net_buf *pending_resp_buf;
};

#define AVDTP_CONN_BY_SIGNAL(_ch) CONTAINER_OF(_ch, struct bt_avdtp_conn, signal_session)
#define AVDTP_CONN_BY_MEDIA(_ch) CONTAINER_OF(_ch, struct bt_avdtp_conn, media_session)
#define AVDTP_CONN_BY_STREAM(_ch) CONTAINER_OF(_ch, struct bt_avdtp_conn, stream)
#define AVDTP_CONN_BY_REQ(_ch) CONTAINER_OF(_ch, struct bt_avdtp_conn, req)

struct bt_avdtp_event_cb {
	int (*accept)(struct bt_conn *conn, struct bt_avdtp **session);
	void (*connected)(struct bt_avdtp *session);
	void (*disconnected)(struct bt_avdtp *session);
	void (*do_media_connect)(struct bt_avdtp *session, bool isconnect);
	void (*media_handler)(struct bt_avdtp *session, struct net_buf *buf);
	int (*media_state_req)(struct bt_avdtp *session, uint8_t sig_id);
	int (*intiator_connect_result)(struct bt_avdtp *session, bool success);
	void (*seted_codec)(struct bt_avdtp *session, struct bt_a2dp_media_codec *codec, uint8_t cp_type);
};

/* Initialize AVDTP layer*/
int bt_avdtp_init(void);

/* Application register with AVDTP layer */
int bt_avdtp_register(struct bt_avdtp_event_cb *cb);

/* AVDTP connect */
int bt_avdtp_connect(struct bt_conn *conn, struct bt_avdtp *session, uint8_t role);

/* AVDTP disconnect */
int bt_avdtp_disconnect(struct bt_avdtp *session);

/* AVDTP SEP register function */
int bt_avdtp_ep_register_sep(uint8_t media_type, uint8_t role,
				struct bt_avdtp_seid_lsep *lsep);

int bt_avdtp_ep_halt_sep(struct bt_avdtp_seid_lsep *lsep, bool halt);

/* AVDTP Discover Request */
int bt_avdtp_discover(struct bt_avdtp *session);
int bt_avdtp_get_capabilities(struct bt_avdtp *session);
int bt_avdtp_get_all_capabilities(struct bt_avdtp *session);
int bt_avdtp_set_configuration(struct bt_avdtp *session);
int bt_avdtp_reconfig(struct bt_avdtp *session, struct bt_a2dp_media_codec *codec);
int bt_avdtp_open(struct bt_avdtp *session);
int bt_avdtp_start(struct bt_avdtp *session);
int bt_avdtp_suspend(struct bt_avdtp *session);
int bt_avdtp_close(struct bt_avdtp *session);
int bt_avdtp_abort(struct bt_avdtp *session);
int bt_avdtp_delayreport(struct bt_avdtp *session, uint16_t delay_time);
struct bt_a2dp_media_codec *bt_avdtp_get_seted_codec(struct bt_avdtp *session);

bool bt_avdtp_ep_empty(void);
struct bt_avdtp_seid_lsep *find_lsep_by_seid(uint8_t seid);
struct bt_avdtp_seid_lsep *find_free_lsep_by_role(uint8_t role);
struct bt_avdtp_seid_lsep *find_free_lsep_by_role_codectype(uint8_t role, uint8_t codectype);
bool lsep_seid_inused(uint8_t seid);
bool lsep_set_seid_used_by_seid(uint8_t seid, struct bt_avdtp_stream *stream);
bool lsep_set_seid_used_by_stream(struct bt_avdtp_stream *stream);
void lsep_set_seid_free(uint8_t seid);
int bt_avdtp_ep_check_set_codec_cp(struct bt_avdtp *session, struct net_buf *buf, uint8_t acp_seid, uint8_t sig_id);
void bt_avdtp_ep_append_seid(struct net_buf *resp_buf);
void bt_avdtp_ep_append_capabilities(struct net_buf *resp_buf, uint8_t reqSeid);
uint8_t bt_avdtp_ep_get_codec_len(struct bt_a2dp_media_codec *codec);
void bt_avdtp_ep_env_init(void);
