/** @file
 * @brief Advance Audio Distribution Profile - SBC Codec header.
 */
/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_A2DP_CODEC_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_A2DP_CODEC_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @brief A2dp connect channel Role */
enum A2DP_ROLE_TYPE {
	/** Unkown role  */
	BT_A2DP_CH_UNKOWN = 0x00,
	/** Source Role */
	BT_A2DP_CH_SOURCE = 0x01,
	/** Sink Role */
	BT_A2DP_CH_SINK = 0x02,
	/** For a2dp media session connect  */
	BT_A2DP_CH_MEDIA = 0x03
};

/** @brief Stream End Point Media Type */
enum MEDIA_TYPE {
	/** Audio Media Type */
	BT_A2DP_AUDIO = 0x00,
	/** Video Media Type */
	BT_A2DP_VIDEO = 0x01,
	/** Multimedia Media Type */
	BT_A2DP_MULTIMEDIA = 0x02
};

/** @brief Stream End Point Role */
enum A2DP_EP_ROLE_TYPE {
	/** Source Role */
	BT_A2DP_EP_SOURCE = 0x00,
	/** Sink Role */
	BT_A2DP_EP_SINK = 0x01
};

/** @brief Codec ID */
enum bt_a2dp_codec_id {
	/** Codec SBC */
	BT_A2DP_SBC = 0x00,
	/** Codec MPEG-1 */
	BT_A2DP_MPEG1 = 0x01,
	/** Codec MPEG-2 */
	BT_A2DP_MPEG2 = 0x02,
	/** Codec ATRAC */
	BT_A2DP_ATRAC = 0x04,
	/** Codec Non-A2DP */
	BT_A2DP_VENDOR = 0xff
};

enum{
	BT_A2DP_SBC_48000 = 1,
	BT_A2DP_SBC_44100 = 2,
	BT_A2DP_SBC_32000 = 4,
	BT_A2DP_SBC_16000 = 8,
	BT_A2DP_SBC_FREQ_MASK = 0xF,
};

enum{
	BT_A2DP_SBC_JOINT_STEREO  = 1,
	BT_A2DP_SBC_STEREO        = 2,
	BT_A2DP_SBC_DUAL_CHANNEL  = 4,
	BT_A2DP_SBC_MONO          = 8,
	BT_A2DP_SBC_CHANNEL_MODE_MASK = 0xF,
};

enum{
	BT_A2DP_SBC_BLOCK_LENGTH_16 = 1,
	BT_A2DP_SBC_BLOCK_LENGTH_12 = 2,
	BT_A2DP_SBC_BLOCK_LENGTH_8  = 4,
	BT_A2DP_SBC_BLOCK_LENGTH_4  = 8,
	BT_A2DP_SBC_BLOCK_LENGTH_MASK = 0xF,
};

enum{
	BT_A2DP_SBC_SUBBANDS_8 = 1,
	BT_A2DP_SBC_SUBBANDS_4 = 2,
	BT_A2DP_SBC_SUBBANDS_MASK = 0x3,
};

enum{
	BT_A2DP_SBC_ALLOCATION_METHOD_LOUDNESS = 1,
	BT_A2DP_SBC_ALLOCATION_METHOD_SNR      = 2,
	BT_A2DP_SBC_ALLOCATION_METHOD_MASK		= 0x3,
};

enum{
	BT_A2DP_AAC_OBJ_MPEG2_AAC_LC = (0x1 << 7),
	BT_A2DP_AAC_OBJ_MPEG4_AAC_LC = (0x1 << 6),
	BT_A2DP_AAC_OBJ_MPEG4_AAC_LTP = (0x1 << 5),
	BT_A2DP_AAC_OBJ_MPEG4_AAC_SCALABLE = (0x1 << 4),
};

enum{
	BT_A2DP_AAC_8000 = (0x1 << 11),
	BT_A2DP_AAC_11025 = (0x1 << 10),
	BT_A2DP_AAC_12000 = (0x1 << 9),
	BT_A2DP_AAC_16000 = (0x1 << 8),
	BT_A2DP_AAC_22050 = (0x1 << 7),
	BT_A2DP_AAC_24000 = (0x1 << 6),
	BT_A2DP_AAC_32000 = (0x1 << 5),
	BT_A2DP_AAC_44100 = (0x1 << 4),
	BT_A2DP_AAC_48000 = (0x1 << 3),
	BT_A2DP_AAC_64000 = (0x1 << 2),
	BT_A2DP_AAC_88200 = (0x1 << 1),
	BT_A2DP_AAC_96000 = (0x1 << 0),
};

enum {
	BT_A2DP_AAC_CHANNELS_1 = (0x1 << 1),
	BT_A2DP_AAC_CHANNELS_2 = (0x1 << 0),
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_A2DP_CODEC_H_ */
