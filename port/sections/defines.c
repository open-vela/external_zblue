/******************************************************************************
 *
 * Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>

/* bt_l2cap_fixed_chan START */
extern struct bt_l2cap_fixed_chan z_att_fixed_chan;
extern struct bt_l2cap_fixed_chan le_fixed_chan;
extern struct bt_l2cap_fixed_chan smp_fixed_chan;

struct bt_l2cap_fixed_chan *_bt_l2cap_fixed_chan_list[] = {
#if defined(CONFIG_BT_CONN)
	&z_att_fixed_chan,
	&le_fixed_chan,
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_SMP)
	&smp_fixed_chan,
#endif /* CONFIG_BT_SMP */
	NULL,
};
/* bt_l2cap_fixed_chan END */

/* bt_l2cap_br_fixed_chan START */
#if defined(CONFIG_BT_CLASSIC)
extern struct bt_l2cap_br_fixed_chan br_fixed_chan;
extern struct bt_l2cap_br_fixed_chan smp_br_fixed_chan;

struct bt_l2cap_br_fixed_chan *_bt_l2cap_br_fixed_chan_list[] = {
	&br_fixed_chan,
#if defined(CONFIG_BT_SMP)
	&smp_br_fixed_chan,
#endif /* CONFIG_BT_SMP */
	NULL,
};
#endif /* CONFIG_BT_CLASSIC */
/* bt_l2cap_br_fixed_chan END */

/* bt_gatt_service_static START */
extern const struct bt_gatt_service_static cas_svc;
extern const struct bt_gatt_service_static pacs_svc;
extern const struct bt_gatt_service_static _2_gap_svc;
extern const struct bt_gatt_service_static _1_gatt_svc;
extern const struct bt_gatt_service_static cts_svc;
extern const struct bt_gatt_service_static dis_svc;
extern const struct bt_gatt_service_static hrs_svc;
extern const struct bt_gatt_service_static tps_svc;
extern const struct bt_gatt_service_static bas;
extern const struct bt_gatt_service_static ias_svc;
extern const struct bt_gatt_service_static mible_svc;
extern const struct bt_gatt_service_static mible_lib_svc;

const struct bt_gatt_service_static *_bt_gatt_service_static_list[] = {
#if defined(CONFIG_BT_CAP_ACCEPTOR) && !defined(CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER)
	&cas_svc,
#endif /* CONFIG_BT_CAP_ACCEPTOR && !CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER */
#if defined(CONFIG_BT_PACS)
	&pacs_svc,
#endif /* CONFIG_BT_PACS */
#if defined(CONFIG_BT_CONN)
	&_2_gap_svc,    &_1_gatt_svc,
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_CTS)
	&cts_svc,
#endif /* CONFIG_BT_CTS */
#if defined(CONFIG_BT_DIS)
	&dis_svc,
#endif /* CONFIG_BT_DIS */
#if defined(CONFIG_BT_HRS)
	&hrs_svc,
#endif /* CONFIG_BT_HRS */
#if defined(CONFIG_BT_TPS)
	&tps_svc,
#endif /* CONFIG_BT_TPS */
#if defined(CONFIG_BT_BAS)
	&bas,
#endif /* CONFIG_BT_BAS */
#if defined(CONFIG_BT_IAS)
	&ias_svc,
#endif /* CONFIG_BT_IAS */
#if defined(CONFIG_BT_MIBLE_TEST)
	&mible_svc,
#endif /*CONFIG_BT_MIBLE_TEST*/
#if defined(CONFIG_MIBLE_SDK)
	&mible_lib_svc,
#endif /* CONFIG_MIBLE_SDK */
	NULL,
};
/* bt_gatt_service_static END */

/* bt_conn_cb START */
extern const struct bt_conn_cb bt_conn_cb_mible_api;
const struct bt_conn_cb *_bt_conn_cb_list[] = {
#if defined(CONFIG_MIBLE_SDK)
	&bt_conn_cb_mible_api,
#endif /* CONFIG_MIBLE_SDK */
	NULL,
};
/* bt_conn_cb END */

/* net_buf_pool START */
extern struct net_buf_pool sine_tx_pool;
extern struct net_buf_pool tx_pool;
extern struct net_buf_pool vs_err_tx_pool;
extern struct net_buf_pool sync_evt_pool;
extern struct net_buf_pool discardable_pool;
extern struct net_buf_pool evt_pool;
extern struct net_buf_pool hci_rx_pool;
extern struct net_buf_pool fragments;
extern struct net_buf_pool reassembly_buf_pool;
extern struct net_buf_pool hci_cmd_pool;
extern struct net_buf_pool hci_acl_pool;
extern struct net_buf_pool hci_iso_pool;
extern struct net_buf_pool iso_rx_pool;
extern struct net_buf_pool iso_tx_pool;
extern struct net_buf_pool disc_pool;
extern struct net_buf_pool ag_pool;
extern struct net_buf_pool hf_pool;
extern struct net_buf_pool br_sig_pool;
extern struct net_buf_pool sdp_pool;
extern struct net_buf_pool data_pool;
extern struct net_buf_pool sdp_client_pool;
extern struct net_buf_pool pool;
extern struct net_buf_pool bis_tx_pool;
extern struct net_buf_pool data_tx_pool;
extern struct net_buf_pool data_rx_pool;
extern struct net_buf_pool friend_buf_pool;
extern struct net_buf_pool ot_chan_tx_pool;
extern struct net_buf_pool ot_chan_rx_pool;
extern struct net_buf_pool usb_out_buf_pool;
extern struct net_buf_pool prep_pool;
extern struct net_buf_pool att_pool;
extern struct net_buf_pool acl_in_pool;
extern struct net_buf_pool acl_tx_pool;
extern struct net_buf_pool dummy_pool;
extern struct net_buf_pool a2dp_tx_pool;

struct net_buf_pool *_net_buf_pool_list[] = {
#if defined(CONFIG_BT_HCI)
#if defined(CONFIG_BT_HCI_RAW)
	&hci_acl_pool,
	&hci_cmd_pool,
	&hci_rx_pool,
#if defined(CONFIG_BT_ISO)
	&hci_iso_pool,
#endif /* CONFIG_BT_ISO */
#endif /* CONFIG_BT_HCI_RAW */

#if defined(CONFIG_BT_HCI_HOST)
	&discardable_pool,
	&hci_cmd_pool,
	&sync_evt_pool,
#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
	&acl_in_pool,
	&evt_pool,
#else  /* CONFIG_BT_HCI_ACL_FLOW_CONTROL */
	&hci_rx_pool,
#endif /* CONFIG_BT_HCI_ACL_FLOW_CONTROL */
#if defined(CONFIG_BT_CONN)
	&acl_tx_pool,
	&att_pool,
#if CONFIG_BT_ATT_PREPARE_COUNT > 0
	&prep_pool,
#endif /* CONFIG_BT_ATT_PREPARE_COUNT > 0 */
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	&disc_pool,
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#if defined(CONFIG_BT_CONN_TX)
	&fragments,
#endif /* CONFIG_BT_CONN_TX */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_ISO)
#if defined(CONFIG_BT_ISO_RX)
	&iso_rx_pool,
#endif /* CONFIG_BT_ISO_RX */
#if defined(CONFIG_BT_ISO_UNICAST) || defined(CONFIG_BT_ISO_BROADCAST)
	&iso_tx_pool,
#endif /* CONFIG_BT_ISO_UNICAST || CONFIG_BT_ISO_BROADCAST */
#endif /* CONFIG_BT_ISO */
#if defined(CONFIG_BT_CHANNEL_SOUNDING)
	&reassembly_buf_pool,
#endif /* CONFIG_BT_CHANNEL_SOUNDING */
#endif /* CONFIG_BT_HCI_HOST */

#if defined(CONFIG_BT_CLASSIC)
	&br_sig_pool,
	&sdp_pool,
#if defined(CONFIG_BT_RFCOMM)
	&dummy_pool,
#endif /* CONFIG_BT_RFCOMM */
#if defined(CONFIG_BT_HFP_HF)
	&hf_pool,
#endif /* CONFIG_BT_HFP_HF */
#if defined(CONFIG_BT_HFP_AG)
	&ag_pool,
#endif /* CONFIG_BT_HFP_AG */
#endif /* CONFIG_BT_CLASSIC */
#endif /* CONFIG_BT_HCI */

#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_OTS)
	&ot_chan_rx_pool,
	&ot_chan_tx_pool,
#endif /* CONFIG_BT_OTS */
#endif /* CONFIG_BT_CONN */

#if defined(CONFIG_BT_MESH)
#if defined(CONFIG_BT_MESH_FRIEND)
	&friend_buf_pool,
#endif /* CONFIG_BT_MESH_FRIEND */
#endif /* CONFIG_BT_MESH */

#if defined(CONFIG_BT_SHELL)
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	&data_rx_pool,
	&data_tx_pool,
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#if defined(CONFIG_BT_CLASSIC)
	&data_pool,
	&sdp_client_pool,
#if defined(CONFIG_BT_A2DP)
	&a2dp_tx_pool,
#endif /* CONFIG_BT_A2DP */
#if defined(CONFIG_BT_RFCOMM)
	&pool,
#endif /* CONFIG_BT_RFCOMM */
#endif /* CONFIG_BT_CLASSIC */
#if defined(CONFIG_BT_ISO)
	&bis_tx_pool,
	&tx_pool,
#endif /* CONFIG_BT_ISO */
#if defined(CONFIG_BT_BAP_STREAM)
	&sine_tx_pool,
#endif /* CONFIG_BT_BAP_STREAM */
#if defined(CONFIG_BT_AUDIO_TX)
	&tx_pool,
#endif /* CONFIG_BT_AUDIO_TX */
#if defined(CONFIG_USB_DEVICE_AUDIO)
	&usb_out_buf_pool,
#endif /* CONFIG_USB_DEVICE_AUDIO */
#endif /* CONFIG_BT_SHELL */
	NULL,
};
/* net_buf_pool END */

/* const union shell_cmd_entry START */

extern const union shell_cmd_entry shell_cmd_bt;
extern const union shell_cmd_entry shell_cmd_bap_broadcast_assistant;
extern const union shell_cmd_entry shell_cmd_bap_scan_delegator;
extern const union shell_cmd_entry shell_cmd_bap;
extern const union shell_cmd_entry shell_cmd_cap_acceptor;
extern const union shell_cmd_entry shell_cmd_cap_commander;
extern const union shell_cmd_entry shell_cmd_cap_initiator;
extern const union shell_cmd_entry shell_cmd_csip_set_coordinator;
extern const union shell_cmd_entry shell_cmd_csip_set_member;
extern const union shell_cmd_entry shell_cmd_gmap;
extern const union shell_cmd_entry shell_cmd_has_client;
extern const union shell_cmd_entry shell_cmd_has;
extern const union shell_cmd_entry shell_cmd_mcc;
extern const union shell_cmd_entry shell_cmd_media;
extern const union shell_cmd_entry shell_cmd_micp_mic_ctlr;
extern const union shell_cmd_entry shell_cmd_micp_mic_dev;
extern const union shell_cmd_entry shell_cmd_mpl;
extern const union shell_cmd_entry shell_cmd_pbp;
extern const union shell_cmd_entry shell_cmd_tbs_client;
extern const union shell_cmd_entry shell_cmd_tbs;
extern const union shell_cmd_entry shell_cmd_tmap;
extern const union shell_cmd_entry shell_cmd_vcp_vol_ctlr;
extern const union shell_cmd_entry shell_cmd_vcp_vol_rend;
extern const union shell_cmd_entry shell_cmd_ticker;
extern const union shell_cmd_entry shell_cmd_a2dp;
extern const union shell_cmd_entry shell_cmd_avrcp;
extern const union shell_cmd_entry shell_cmd_br;
extern const union shell_cmd_entry shell_cmd_rfcomm;
extern const union shell_cmd_entry shell_cmd_cs;
extern const union shell_cmd_entry shell_cmd_gatt;
extern const union shell_cmd_entry shell_cmd_iso;
extern const union shell_cmd_entry shell_cmd_l2cap;
extern const union shell_cmd_entry shell_cmd_mesh;
extern const union shell_cmd_entry shell_cmd_ias_client;
extern const union shell_cmd_entry shell_cmd_ias;

const union shell_cmd_entry *_shell_root_cmds_list[] = {
#if defined(CONFIG_BT_SHELL)
    &shell_cmd_bt,
#if defined(CONFIG_BT_CLASSIC)
    &shell_cmd_br,
#if defined(CONFIG_BT_RFCOMM)
    &shell_cmd_rfcomm,
#endif /* CONFIG_BT_RFCOMM */
#if defined(CONFIG_BT_A2DP)
    &shell_cmd_a2dp,
#endif /* CONFIG_BT_A2DP */
#if defined(CONFIG_BT_AVRCP)
    &shell_cmd_avrcp,
#endif /* CONFIG_BT_AVRCP */
#endif /* CONFIG_BT_CLASSIC */
#if defined(CONFIG_BT_CONN)
    &shell_cmd_gatt,
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
    &shell_cmd_l2cap,
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#if defined(CONFIG_BT_IAS)
    &shell_cmd_ias,
#endif /* CONFIG_BT_IAS */
#if defined(CONFIG_BT_IAS_CLIENT)
    &shell_cmd_ias_client,
#endif /* CONFIG_BT_IAS_CLIENT */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_ISO)
    &shell_cmd_iso,
#endif /* CONFIG_BT_ISO */
#if defined(CONFIG_BT_CHANNEL_SOUNDING)
    &shell_cmd_cs,
#endif /* CONFIG_BT_CHANNEL_SOUNDING */
#if defined(CONFIG_BT_AUDIO)
#if defined(CONFIG_BT_BAP_STREAM)
    &shell_cmd_bap,
#endif /* CONFIG_BT_BAP_STREAM */
#if defined(CONFIG_BT_BAP_BROADCAST_ASSISTANT)
    &shell_cmd_bap_broadcast_assistant,
#endif /* CONFIG_BT_BAP_BROADCAST_ASSISTANT */
#if defined(CONFIG_BT_BAP_SCAN_DELEGATOR)
    &shell_cmd_bap_scan_delegator,
#endif /* CONFIG_BT_BAP_SCAN_DELEGATOR */
#if defined(CONFIG_BT_CAP_ACCEPTOR)
    &shell_cmd_cap_acceptor,
#endif /* CONFIG_BT_CAP_ACCEPTOR */
#if defined(CONFIG_BT_CAP_COMMANDER)
    &shell_cmd_cap_commander,
#endif /* CONFIG_BT_CAP_COMMANDER */
#if defined(CONFIG_BT_CAP_INITIATOR)
    &shell_cmd_cap_initiator,
#endif /* CONFIG_BT_CAP_INITIATOR */
#if defined(CONFIG_BT_CSIP_SET_MEMBER)
    &shell_cmd_csip_set_member,
#endif /* CONFIG_BT_CSIP_SET_MEMBER */
#if defined(CONFIG_BT_CSIP_SET_COORDINATOR)
    &shell_cmd_csip_set_coordinator,
#endif /* CONFIG_BT_CSIP_SET_COORDINATOR */
#if defined(CONFIG_BT_GMAP)
    &shell_cmd_gmap,
#endif /* CONFIG_BT_GMAP */
#if defined(CONFIG_BT_HAS_CLIENT)
    &shell_cmd_has_client,
#endif /* CONFIG_BT_HAS_CLIENT */
#if defined(CONFIG_BT_HAS_PRESET_SUPPORT)
    &shell_cmd_has,
#endif /* CONFIG_BT_HAS_PRESET_SUPPORT */
#if defined(CONFIG_BT_MCC)
    &shell_cmd_mcc,
#endif /* CONFIG_BT_MCC */
#if defined(CONFIG_BT_MCS)
    &shell_cmd_media,
#endif /* CONFIG_BT_MCS */
#if defined(CONFIG_BT_MICP_MIC_CTLR)
    &shell_cmd_micp_mic_ctlr,
#endif /* CONFIG_BT_MICP_MIC_CTLR */
#if defined(CONFIG_BT_MICP_MIC_DEV)
    &shell_cmd_micp_mic_dev,
#endif /* CONFIG_BT_MICP_MIC_DEV */
#if defined(CONFIG_BT_MPL)
    &shell_cmd_mpl,
#endif /* CONFIG_BT_MPL */
#if defined(CONFIG_BT_PBP)
    &shell_cmd_pbp,
#endif /* CONFIG_BT_PBP */
#if defined(CONFIG_BT_TBS_CLIENT)
    &shell_cmd_tbs_client,
#endif /* CONFIG_BT_TBS_CLIENT */
#if defined(CONFIG_BT_TBS)
    &shell_cmd_tbs,
#endif /* CONFIG_BT_TBS */
#if defined(CONFIG_BT_TMAP)
    &shell_cmd_tmap,
#endif /* CONFIG_BT_TMAP */
#if defined(CONFIG_BT_VCP_VOL_CTLR)
    &shell_cmd_vcp_vol_ctlr,
#endif /* CONFIG_BT_VCP_VOL_CTLR */
#if defined(CONFIG_BT_VCP_VOL_REND)
    &shell_cmd_vcp_vol_rend,
#endif /* CONFIG_BT_VCP_VOL_REND */
#endif /* CONFIG_BT_AUDIO */
#if defined(CONFIG_BT_MESH_SHELL)
    &shell_cmd_mesh,
#endif /* CONFIG_BT_MESH_SHELL */
#endif /* CONFIG_BT_SHELL */
    NULL,
};
/* const union shell_cmd_entry END */

/* k_mem_slab START */
extern struct k_mem_slab lc3_data_slab;
extern struct k_mem_slab req_slab;
extern struct k_mem_slab att_slab;
extern struct k_mem_slab chan_slab;
extern struct k_mem_slab local_adv_pool;
extern struct k_mem_slab relay_adv_pool;
extern struct k_mem_slab friend_adv_pool;
extern struct k_mem_slab loopback_buf_pool;
extern struct k_mem_slab segs;
extern struct k_mem_slab mible_timers;

struct k_mem_slab *_k_mem_slab_list[] = {
#if defined(CONFIG_LIBLC3)
	&lc3_data_slab,
#endif /* CONFIG_LIBLC3 */
#if defined(CONFIG_BT_CONN)
	&req_slab,
	&att_slab,
	&chan_slab,
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_MESH)
	&local_adv_pool,
#if defined(CONFIG_BT_MESH_RELAY_BUF_COUNT)
	&relay_adv_pool,
#endif /* CONFIG_BT_MESH_RELAY_BUF_COUNT */
#if defined(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
	&friend_adv_pool,
#endif /* CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE */
	&loopback_buf_pool,
	&segs,
#endif /* CONFIG_BT_MESH */
#if defined(CONFIG_MIBLE_SDK)
	&mible_timers,
#endif
	NULL,
};
/* k_mem_slab END */

/* bt_mesh_friend_cb START */
extern const struct bt_mesh_friend_cb bt_mesh_friend_cb_friend_cb;
const struct bt_mesh_friend_cb *_bt_mesh_friend_cb_list[] = {
#if defined(CONFIG_BT_TESTER)
#if defined(CONFIG_BT_MESH)
	&bt_mesh_friend_cb_friend_cb,
#endif /* CONFIG_BT_MESH */
#endif /* CONFIG_BT_TESTER */
	NULL,
};
/* bt_mesh_friend_cb END */

/* bt_mesh_subnet_cb START */
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_app_keys;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_beacon;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_sbr;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_friend;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_lpn;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_proxy_cli;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_gatt_services;
const struct bt_mesh_subnet_cb *_bt_mesh_subnet_cb_list[] = {
#if defined(CONFIG_BT_MESH)
	&bt_mesh_subnet_cb_app_keys,
	&bt_mesh_subnet_cb_beacon,
#if defined(CONFIG_BT_MESH_BRG_CFG_SRV)
	&bt_mesh_subnet_cb_sbr,
#endif /* CONFIG_BT_MESH_BRG_CFG_SRV */
#if defined(CONFIG_BT_MESH_FRIEND)
	&bt_mesh_subnet_cb_friend,
#endif /* CONFIG_BT_MESH_FRIEND */
#if defined(CONFIG_BT_MESH_LPN)
	&bt_mesh_subnet_cb_lpn,
#endif /* CONFIG_BT_MESH_LPN */
#if defined(CONFIG_BT_MESH_PROXY_CLIENT)
	&bt_mesh_subnet_cb_proxy_cli,
#endif /* CONFIG_BT_MESH_PROXY_CLIENT */
#if defined(CONFIG_BT_MESH_GATT_PROXY)
	&bt_mesh_subnet_cb_gatt_services,
#endif /* CONFIG_BT_MESH_GATT_PROXY */
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_subnet_cb END */

/* bt_mesh_proxy_cb START */
const struct bt_mesh_proxy_cb *_bt_mesh_proxy_cb_list[] = {
	NULL,
};
/* bt_mesh_proxy_cb END */

/* bt_mesh_hb_cb START */
extern const struct bt_mesh_hb_cb hb_cb;
const struct bt_mesh_hb_cb *_bt_mesh_hb_cb_list[] = {
#if defined(CONFIG_BT_MESH)
#if defined(CONFIG_BT_MESH_DEMO)
	&hb_cb,
#endif /* CONFIG_BT_MESH_DEMO */
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_hb_cb END */

/* bt_mesh_app_key_cb START */
extern const struct bt_mesh_app_key_cb bt_mesh_app_key_cb_app_key_evt;
const struct bt_mesh_app_key_cb *_bt_mesh_app_key_cb_list[] = {
#if defined(CONFIG_BT_MESH)
	&bt_mesh_app_key_cb_app_key_evt,
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_app_key_cb END */

/* init_entry START */
extern const struct init_entry __init_init_mem_slab_obj_core_list;
extern const struct init_entry __init_k_sys_work_q_init;
extern const struct init_entry __init_broadcast_sink_init;
extern const struct init_entry __init_bt_conn_tx_workq_init;
extern const struct init_entry __init_long_wq_init;
extern const struct init_entry __init_bt_monitor_init;
extern const struct init_entry __init_hrs_init;
extern const struct init_entry __init_bas_init;
extern const struct init_entry __init_bt_nus_auto_start;
extern const struct init_entry __init_bt_gatt_ots_l2cap_init;
extern const struct init_entry __init_bt_gatt_ots_instances_prepare;
extern const struct init_entry __init___device_dts_ord_DT_N_INST_0_zephyr_bt_hci_ttyHCI_ORD;

const struct init_entry *_init_entry_list[] = {
	&__init_init_mem_slab_obj_core_list,
	&__init_k_sys_work_q_init,
#if defined(CONFIG_BT_BAP_BROADCAST_ASSISTANT)
	&__init_broadcast_sink_init,
#endif /* CONFIG_BT_BAP_BROADCAST_ASSISTANT */
#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_CONN_TX_NOTIFY_WQ)
	&__init_bt_conn_tx_workq_init,
#endif /* CONFIG_BT_CONN_TX_NOTIFY_WQ */
#if defined(CONFIG_BT_HRS)
	&__init_hrs_init,
#endif /* CONFIG_BT_HRS */
#if defined(CONFIG_BT_BAS)
	&__init_bas_init,
#endif /* CONFIG_BT_BAS */
#if defined(CONFIG_BT_ZEPHYR_NUS)
	&__init_bt_nus_auto_start,
#endif /* CONFIG_BT_ZEPHYR_NUS */
#if defined(CONFIG_BT_OTS)
	&__init_bt_gatt_ots_l2cap_init,
	&__init_bt_gatt_ots_instances_prepare,
#endif /* CONFIG_BT_OTS */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_LONG_WQ)
	&__init_long_wq_init,
#endif /* CONFIG_BT_LONG_WQ */
#if defined(CONFIG_BT_MONITOR)
	&__init_bt_monitor_init,
#endif /* CONFIG_BT_MONITOR */
#if defined(CONFIG_BT_H4)
	&__init___device_dts_ord_DT_N_INST_0_zephyr_bt_hci_ttyHCI_ORD,
#endif /* CONFIG_BT_H4 */
	NULL,
};
/* init_entry END */

/* bt_ias_cb START */
#if defined(CONFIG_BT_IAS)
struct bt_ias_cb *_bt_ias_cb_list[] = {
	NULL,
};
#endif /* CONFIG_BT_IAS */
/* bt_ias_cb END */

/* settings_handler_static START */
#if defined(CONFIG_SETTINGS)
extern const struct settings_handler_static settings_handler_bt;
extern const struct settings_handler_static settings_handler_bt_link_key;
extern const struct settings_handler_static settings_handler_bt_keys;
extern const struct settings_handler_static settings_handler_bt_ccc;
extern const struct settings_handler_static settings_handler_bt_sc;
extern const struct settings_handler_static settings_handler_bt_cf;
extern const struct settings_handler_static settings_handler_bt_hash;
extern const struct settings_handler_static settings_handler_bt_has;
extern const struct settings_handler_static settings_handler_bt_dis;
extern const struct settings_handler_static settings_handler_bt_mesh_dfu_slots;
extern const struct settings_handler_static settings_handler_bt_mesh;
extern const struct settings_handler_static settings_handler_bt_mesh_sig_mod;
extern const struct settings_handler_static settings_handler_bt_mesh_vnd_mod;
extern const struct settings_handler_static settings_handler_bt_mesh_comp;
extern const struct settings_handler_static settings_handler_bt_mesh_metadata;
extern const struct settings_handler_static settings_handler_bt_mesh_app;
extern const struct settings_handler_static settings_handler_bt_mesh_brg_en;
extern const struct settings_handler_static settings_handler_bt_mesh_brg_tbl;
extern const struct settings_handler_static settings_handler_bt_mesh_cdb;
extern const struct settings_handler_static settings_handler_bt_mesh_cfg;
extern const struct settings_handler_static settings_handler_bt_mesh_pub;
extern const struct settings_handler_static settings_handler_bt_mesh_net;
extern const struct settings_handler_static settings_handler_bt_mesh_iv;
extern const struct settings_handler_static settings_handler_bt_mesh_seq;
extern const struct settings_handler_static settings_handler_bt_mesh_dev_key;
extern const struct settings_handler_static settings_handler_bt_mesh_rpl;
extern const struct settings_handler_static settings_handler_bt_mesh_sseq;
extern const struct settings_handler_static settings_handler_bt_mesh_srpl;
extern const struct settings_handler_static settings_handler_bt_mesh_subnet;
extern const struct settings_handler_static settings_handler_bt_mesh_va;

const struct settings_handler_static *_settings_handler_static_list[] = {
#if defined(CONFIG_BT_SETTINGS)
	&settings_handler_bt,
#if defined(CONFIG_BT_CLASSIC)
	&settings_handler_bt_link_key,
#endif /* CONFIG_BT_CLASSIC */

#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_SMP)
	&settings_handler_bt_keys,
#endif /* CONFIG_BT_SMP */
	&settings_handler_bt_ccc,
#if defined(CONFIG_BT_GATT_SERVICE_CHANGED)
	&settings_handler_bt_sc,
#endif /* CONFIG_BT_GATT_SERVICE_CHANGED */
#if defined(CONFIG_BT_GATT_CACHING)
	&settings_handler_bt_cf,
#endif /* CONFIG_BT_GATT_CACHING */
	&settings_handler_bt_hash,
#if defined(CONFIG_BT_DIS)
#if defined(CONFIG_BT_DIS_SETTINGS)
		&settings_handler_bt_dis,
#endif /* CONFIG_BT_DIS_SETTINGS */
#endif /* CONFIG_BT_DIS */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_MESH)
#if defined(CONFIG_BT_MESH_DFU_SLOTS)
	&settings_handler_bt_mesh_dfu_slots,
#endif /* CONFIG_BT_MESH_DFU_SLOTS */
	&settings_handler_bt_mesh,
	&settings_handler_bt_mesh_sig_mod,
	&settings_handler_bt_mesh_vnd_mod,
	&settings_handler_bt_mesh_comp,
#if defined(CONFIG_BT_MESH_LARGE_COMP_DATA_SRV)
	&settings_handler_bt_mesh_metadata,
#endif /* CONFIG_BT_MESH_LARGE_COMP_DATA_SRV */
	&settings_handler_bt_mesh_app,
#if defined(CONFIG_BT_MESH_BRG_CFG_SRV)
	&settings_handler_bt_mesh_brg_en,
	&settings_handler_bt_mesh_brg_tbl,
#endif /* CONFIG_BT_MESH_BRG_CFG_SRV */
#if defined(CONFIG_BT_MESH_CDB)
	&settings_handler_bt_mesh_cdb,
#endif /* CONFIG_BT_MESH_CDB */
	&settings_handler_bt_mesh_cfg,
	&settings_handler_bt_mesh_pub,
	&settings_handler_bt_mesh_net,
	&settings_handler_bt_mesh_iv,
	&settings_handler_bt_mesh_seq,
#if defined(CONFIG_BT_MESH_RPR_SRV)
	&settings_handler_bt_mesh_dev_key,
#endif /* CONFIG_BT_MESH_RPR_SRV */
#if defined(CONFIG_BT_MESH_RPL_STORAGE_MODE_SETTINGS)
	&settings_handler_bt_mesh_rpl,
#endif /* CONFIG_BT_MESH_RPL_STORAGE_MODE_SETTINGS */
#if defined(CONFIG_BT_MESH_SOLICITATION)
#if defined(CONFIG_BT_MESH_PROXY_SOLICITATION)
	&settings_handler_bt_mesh_sseq,
#endif /* CONFIG_BT_MESH_PROXY_SOLICITATION */
#if defined(CONFIG_BT_MESH_OD_PRIV_PROXY_SRV)
	&settings_handler_bt_mesh_srpl,
#endif /* CONFIG_BT_MESH_OD_PRIV_PROXY_SRV */
#endif /* CONFIG_BT_MESH_SOLICITATION */
	&settings_handler_bt_mesh_subnet,
#if CONFIG_BT_MESH_LABEL_COUNT > 0
	&settings_handler_bt_mesh_va,
#endif /* CONFIG_BT_MESH_LABEL_COUNT > 0 */
#endif /* CONFIG_BT_MESH */
#endif /* CONFIG_BT_SETTINGS */

	NULL,
};
#endif /* CONFIG_SETTINGS */
/* settings_handler_static END */