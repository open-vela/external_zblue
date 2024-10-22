/** @file
 * @brief Audio/Video Distribution Transport Protocol header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AVDTP_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AVDTP_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
	BT_AVDTP_SIGNALING_SESSION	= 0x0,
	BT_AVDTP_MEDIA_SESSION		= 0x1,
	BT_AVDTP_REPORTING_SESSION	= 0x2,
	BT_AVDTP_RECOVERY_SESSION	= 0x3,
	BT_AVDTP_MAX_SESSION		= 4,
};

enum {
	BT_AVDTP_MEDIA_TYPE_AUDIO = 0,
	BT_AVDTP_MEDIA_TYPE_VIDEO,
	BT_AVDTP_MEDIA_TYPE_MULTIMEDIA,
};

enum {
	BT_AVDTP_AV_CP_TYPE_NONE = 0,
	BT_AVDTP_AV_CP_TYPE_DTCP = 1,
	BT_AVDTP_AV_CP_TYPE_SCMS_T = 2,
};

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
/** @brief AVDTP SEID Information */
struct bt_avdtp_seid_info {
	/** Reserved */
	uint8_t rfa0:1;
	/** End Point usage status */
	uint8_t inuse:1;
	/** Stream End Point ID */
	uint8_t id:6;
	/** Reserved */
	uint8_t rfa1:3;
	/** TSEP of the End Point */
	uint8_t tsep:1;
	/** Media-type of the End Point */
	uint8_t media_type:4;
} __packed;

struct bt_a2dp_media_codec_head {
	uint8_t rfa0:4;
	uint8_t media_type:4;
	uint8_t codec_type;
} __packed;

struct bt_a2dp_media_sbc_codec {
	uint8_t rfa0:4;
	uint8_t media_type:4;
	uint8_t codec_type;
	uint8_t channel_mode:4;
	uint8_t freq:4;
	uint8_t alloc_method:2;
	uint8_t subbands:2;
	uint8_t block_len:4;
	uint8_t min_bitpool;
	uint8_t max_bitpool;
} __packed;

struct bt_a2dp_media_aac_codec {
	uint8_t rfa0:4;
	uint8_t media_type:4;
	uint8_t codec_type;
	uint8_t obj_type;
	uint8_t freq0;
	uint8_t rfa1:2;
	uint8_t channels:2;
	uint8_t freq1:4;
	uint8_t bit_rate0:7;
	uint8_t vbr:1;
	uint8_t bit_rate1;
	uint8_t bit_rate2;
} __packed;

struct bt_a2dp_media_codec {
	union {
		struct bt_a2dp_media_codec_head head;
		struct bt_a2dp_media_sbc_codec sbc;
		struct bt_a2dp_media_aac_codec aac;
	};
};

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
/** @brief AVDTP SEID Information */
struct bt_avdtp_seid_info {
	/** Stream End Point ID */
	uint8_t id:6;
	/** End Point usage status */
	uint8_t inuse:1;
	/** Reserved */
	uint8_t rfa0:1;
	/** Media-type of the End Point */
	uint8_t media_type:4;
	/** TSEP of the End Point */
	uint8_t tsep:1;
	/** Reserved */
	uint8_t rfa1:3;
} __packed;
#endif

/** @brief AVDTP Local SEP*/
struct bt_avdtp_seid_lsep {
	/** Stream End Point information */
	struct bt_avdtp_seid_info sid;

	/** Pointer to media codec */
	struct bt_a2dp_media_codec *codec;

	/** Content protection: support SCMS-T */
	uint8_t a2dp_cp_scms_t:1;

	/** A2dp delay report */
	uint8_t a2dp_delay_report:1;

	/** For upper layer halt/resume register endpoint */
	uint8_t ep_halt:1;

	/** Pointer to next local Stream End Point structure */
	struct bt_avdtp_seid_lsep *next;
};

/** @brief AVDTP Stream */
struct bt_avdtp_stream {
	struct bt_avdtp_seid_info lsid;		/* Configured Local sep info */
	struct bt_avdtp_seid_info rsid;		/* Configured Remote sep info*/
	struct bt_a2dp_media_codec codec;	/* Codec config */
	uint8_t cp_type;						/* Content protection type */
	uint8_t delay_report:1;				/* Support delay report */
	uint8_t stream_state;					/* current state of the stream */
	uint8_t acp_state;						/* Device as acp state */
	uint8_t int_state;						/* Device as int state */
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AVDTP_H_ */
