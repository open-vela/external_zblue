/****************************************************************************
 * apps/external/zblue/port/common/defines.c
 *
 *   Copyright (C) 2020 Xiaomi InC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <kernel.h>
#include <net/buf.h>

/* bt_l2cap_fixed_chan START */
extern const struct bt_l2cap_fixed_chan att_fixed_chan;
extern const struct bt_l2cap_fixed_chan le_fixed_chan;
extern const struct bt_l2cap_fixed_chan smp_fixed_chan;
extern const struct bt_l2cap_fixed_chan smp_br_fixed_chan;
const struct bt_l2cap_fixed_chan * const _bt_l2cap_fixed_chan_list[] =
{
#if defined(CONFIG_BT_HCI_HOST)
	&smp_fixed_chan,
#if defined(CONFIG_BT_CONN)
	&att_fixed_chan,
	&le_fixed_chan,
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_SMP)
#if defined(CONFIG_BT_BREDR)
	&smp_br_fixed_chan,
#endif /* CONFIG_BT_BREDR */
#endif /* CONFIG_BT_SMP */
#endif /* CONFIG_BT_HCI_HOST */
	NULL
};
/* bt_l2cap_fixed_chan END */

/* bt_gatt_service_static START */
extern const struct bt_gatt_service_static _1_gatt_svc;
extern const struct bt_gatt_service_static _2_gap_svc;
extern const struct bt_gatt_service_static bas;
extern const struct bt_gatt_service_static dis_svc;
extern const struct bt_gatt_service_static hrs_svc;
extern const struct bt_gatt_service_static tps_svc;
extern const struct bt_gatt_service_static cts_cvs;
extern const struct bt_gatt_service_static vnd_svc;
const struct bt_gatt_service_static * const _bt_gatt_service_static_list[] =
{
#if defined(CONFIG_BT_HCI_HOST)
#if defined(CONFIG_BT_CONN)
	&_1_gatt_svc,
	&_2_gap_svc,
#endif /* CONFIG_BT_CONN */
#endif /* CONFIG_BT_HCI_HOST */
#if defined(CONFIG_BT_BAS)
	&bas,
#endif /* CONFIG_BT_BAS */
#if defined(CONFIG_BT_DIS)
	&dis_svc,
#endif /* CONFIG_BT_DIS */
#if defined(CONFIG_BT_HRS)
	&hrs_svc,
#endif /* CONFIG_BT_HRS */
#if defined(CONFIG_BT_TPS)
	&tps_svc,
#endif /* CONFIG_BT_TPS */
#if defined(CONFIG_BT_SAMPLE)
#if defined(CONFIG_BT_SAMPLE_PERIPHERAL)
	&cts_cvs,
	&vnd_svc,
#endif /* CONFIG_BT_SAMPLE_PERIPHERAL */
#endif /* CONFIG_BT_SAMPLE */
	NULL
};
/* bt_gatt_service_static END */

/* net_buf_pool START */
extern struct net_buf_pool acl_in_pool;
extern struct net_buf_pool acl_tx_pool;
extern struct net_buf_pool dummy_pool;
extern struct net_buf_pool adv_buf_pool;
extern struct net_buf_pool loopback_buf_pool;
extern struct net_buf_pool br_sig_pool;
extern struct net_buf_pool discardable_pool;
extern struct net_buf_pool evt_pool;
extern struct net_buf_pool disc_pool;
extern struct net_buf_pool frag_pool;
extern struct net_buf_pool friend_buf_pool;
extern struct net_buf_pool hci_acl_pool;
extern struct net_buf_pool hci_cmd_pool;
extern struct net_buf_pool hci_iso_pool;
extern struct net_buf_pool hci_rx_pool;
extern struct net_buf_pool hf_pool;
extern struct net_buf_pool iso_tx_pool;
extern struct net_buf_pool iso_rx_pool;
extern struct net_buf_pool iso_frag_pool;
extern struct net_buf_pool num_complete_pool;
extern struct net_buf_pool prep_pool;
extern struct net_buf_pool server_pool;
extern struct net_buf_pool sdp_pool;
extern struct net_buf_pool ot_chan_tx_pool;
extern struct net_buf_pool tx_pool;
extern struct net_buf_pool bis_tx_pool;
extern struct net_buf_pool data_tx_pool;
extern struct net_buf_pool data_rx_pool;
extern struct net_buf_pool pool;
extern struct net_buf_pool data_pool;
struct net_buf_pool * const _net_buf_pool_list[] =
{
#if defined(CONFIG_BT_HCI)
	&hci_cmd_pool,
#if defined(CONFIG_BT_HCI_RAW)
	&hci_rx_pool,
	&hci_acl_pool,
#if defined(CONFIG_BT_ISO)
	&hci_iso_pool,
#endif /* CONFIG_BT_ISO */
#elif defined(CONFIG_BT_HCI_HOST)
#if defined(CONFIG_BT_RFCOMM)
	&dummy_pool,
#endif /* CONFIG_BT_RFCOMM */
#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
	&acl_in_pool,
	&evt_pool,
#else /* !CONFIG_BT_HCI_ACL_FLOW_CONTROL */
	&hci_rx_pool,
#endif /* CONFIG_BT_HCI_ACL_FLOW_CONTROL */
#if defined(CONFIG_BT_CONN)
	&acl_tx_pool,
	&num_complete_pool,
#if CONFIG_BT_L2CAP_TX_FRAG_COUNT > 0
	&frag_pool,
#endif /* CONFIG_BT_L2CAP_TX_FRAG_COUNT > 0 */
#if CONFIG_BT_ATT_PREPARE_COUNT > 0
	&prep_pool,
#endif /* CONFIG_BT_ATT_PREPARE_COUNT > 0 */
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	&disc_pool,
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_BUF_EVT_DISCARDABLE_COUNT)
	&discardable_pool,
#endif /* CONFIG_BT_BUF_EVT_DISCARDABLE_COUNT */
#if defined(CONFIG_BT_ISO)
	&iso_tx_pool,
	&iso_rx_pool,
	&iso_frag_pool,
#endif /* CONFIG_BT_ISO */
#endif /* CONFIG_BT_HCI_HOST */
#if defined(CONFIG_BT_MESH)
	&adv_buf_pool,
	&loopback_buf_pool,
#endif /* CONFIG_BT_MESH */
#if defined(CONFIG_BT_MESH_FRIEND)
	&friend_buf_pool,
#endif /* CONFIG_BT_MESH_FRIEND */
#endif /* CONFIG_BT_HCI_RAW */
#if defined(CONFIG_BT_TESTER)
	&server_pool,
	&data_pool,
#endif /* CONFIG_BT_TESTER */
#if defined(CONFIG_BT_SHELL)
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	&data_tx_pool,
	&data_rx_pool,
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#if defined(CONFIG_BT_ISO)
	&tx_pool,
	&bis_tx_pool,
#endif /* CONFIG_BT_ISO */
#if defined(CONFIG_BT_RFCOMM)
	&pool,
#endif /* CONFIG_BT_RFCOMM */
#endif /* CONFIG_BT_SHELL */
#if defined(CONFIG_BT_OTS)
	&ot_chan_tx_pool,
#endif /* CONFIG_BT_OTS */
#if defined(CONFIG_BT_RFCOMM)
	&dummy_pool,
#endif /* CONFIG_BT_RFCOMM */
#if defined(CONFIG_BT_BREDR)
	&br_sig_pool,
	&sdp_pool,
#endif /* CONFIG_BT_BREDR */
#if defined(CONFIG_BT_HFP_HF)
	&hf_pool,
#endif /* CONFIG_BT_HFP_HF */
	NULL
};
/* net_buf_pool END */

/* settings_handler_static START */
extern const struct settings_handler_static settings_handler_bt_ccc;
extern const struct settings_handler_static settings_handler_bt_sc;
extern const struct settings_handler_static settings_handler_bt_cf;
extern const struct settings_handler_static settings_handler_bt_hash;
extern const struct settings_handler_static settings_handler_bt_link_key;
extern const struct settings_handler_static settings_handler_bt_keys;
extern const struct settings_handler_static settings_handler_bt;
extern const struct settings_handler_static settings_handler_bt_dis;
extern const struct settings_handler_static settings_handler_bt_mesh;
extern const struct settings_handler_static settings_handler_bt_mesh_sig_mod;
extern const struct settings_handler_static settings_handler_bt_mesh_vnd_mod;
extern const struct settings_handler_static settings_handler_bt_mesh_app;
extern const struct settings_handler_static settings_handler_bt_mesh_cdb;
extern const struct settings_handler_static settings_handler_bt_mesh_cfg;
extern const struct settings_handler_static settings_handler_bt_mesh_pub;
extern const struct settings_handler_static settings_handler_bt_mesh_net;
extern const struct settings_handler_static settings_handler_bt_mesh_iv;
extern const struct settings_handler_static settings_handler_bt_mesh_seq;
extern const struct settings_handler_static settings_handler_bt_mesh_rpl;
extern const struct settings_handler_static settings_handler_bt_mesh_subnet;
extern const struct settings_handler_static settings_handler_bt_mesh_va;
const struct settings_handler_static * const _settings_handler_static_list[] =
{
#if defined(CONFIG_BT_SETTINGS)
#if defined(CONFIG_BT_SMP)
	&settings_handler_bt_keys,
#endif /* CONFIG_BT_SMP */
#if defined(CONFIG_BT_DIS)
#if defined(CONFIG_BT_DIS_SETTINGS)
	&settings_handler_bt_dis,
#endif /* CONFIG_BT_DIS_SETTINGS */
#endif /* CONFIG_BT_DIS */
#if defined(CONFIG_BT_CONN)
	&settings_handler_bt_ccc,
#if defined(CONFIG_BT_GATT_SERVICE_CHANGED)
	&settings_handler_bt_sc,
#endif /* CONFIG_BT_GATT_SERVICE_CHANGED */
#if defined(CONFIG_BT_GATT_CACHING)
	&settings_handler_bt_cf,
	&settings_handler_bt_hash,
#endif /* CONFIG_BT_GATT_CACHING */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_BREDR)
	&settings_handler_bt_link_key,
#endif /* CONFIG_BT_BREDR */
	&settings_handler_bt,
#if defined(CONFIG_BT_MESH)
	&settings_handler_bt_mesh,
	&settings_handler_bt_mesh_sig_mod,
	&settings_handler_bt_mesh_vnd_mod,
	&settings_handler_bt_mesh_app,
#if defined(CONFIG_BT_MESH_CDB)
	&settings_handler_bt_mesh_cdb,
#endif /* CONFIG_BT_MESH_CDB */
	&settings_handler_bt_mesh_cfg,
	&settings_handler_bt_mesh_pub,
	&settings_handler_bt_mesh_net,
	&settings_handler_bt_mesh_iv,
	&settings_handler_bt_mesh_seq,
	&settings_handler_bt_mesh_rpl,
	&settings_handler_bt_mesh_subnet,
#if CONFIG_BT_MESH_LABEL_COUNT > 0
	&settings_handler_bt_mesh_va,
#endif /* CONFIG_BT_MESH_LABEL_COUNT > 0 */
#endif /* CONFIG_BT_MESH */
#endif /* CONFIG_BT_SETTINGS */
	NULL,
};
/* settings_handler END */

/* shell_cmd_entry START */
extern const struct shell_cmd_entry shell_cmd_mesh;
extern const struct shell_cmd_entry shell_cmd_br;
extern const struct shell_cmd_entry shell_cmd_gatt;
extern const struct shell_cmd_entry shell_cmd_iso;
extern const struct shell_cmd_entry shell_cmd_l2cap;
extern const struct shell_cmd_entry shell_cmd_rfcomm;
extern const struct shell_cmd_entry shell_cmd_vcs_client;
extern const struct shell_cmd_entry shell_cmd_vcs;
extern const struct shell_cmd_entry shell_cmd_fs;
const struct shell_cmd_entry * const _shell_cmd_entry_list[] =
{
#if defined(CONFIG_BT_SHELL)
#if defined(CONFIG_BT_BREDR)
	&shell_cmd_br,
#endif /* CONFIG_BT_BREDR */
#if defined(CONFIG_BT_CONN)
	&shell_cmd_gatt,
	&shell_cmd_l2cap,
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_ISO)
	&shell_cmd_iso,
#endif /* CONFIG_BT_ISO */
#if defined(CONFIG_BT_RFCOMM)
	&shell_cmd_rfcomm,
#endif /* CONFIG_BT_RFCOMM */
#if defined(CONFIG_BT_VCS_CLIENT)
	&shell_cmd_vcs_client,
#endif /* CONFIG_BT_VCS_CLIENT */
#if defined(CONFIG_BT_VCS)
	&shell_cmd_vcs,
#endif /* CONFIG_BT_VCS */
#if defined(CONFIG_BT_MESH_SHELL)
#if defined(CONFIG_BT_MESH)
	&shell_cmd_mesh,
#endif /* CONFIG_BT_MESH */
#endif /* CONFIG_BT_MESH_SHELL */
#if defined(CONFIG_SETTINGS_FS)
	&shell_cmd_fs,
#endif /* CONFIG_SETTINGS_FS */
#endif /* CONFIG_BT_SHELL */
	NULL,
};
/* shell_cmd_entry END */

/* k_mem_slab START */
extern struct k_mem_slab req_slab;
extern struct k_mem_slab att_slab;
extern struct k_mem_slab chan_slab;
extern struct k_mem_slab segs;
extern struct k_mem_slab mslab1;
struct k_mem_slab * const _k_mem_slab_list[] =
{
#if defined(CONFIG_BT_CONN)
	&req_slab,
	&att_slab,
	&chan_slab,
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_MESH)
	&segs,
#endif /* CONFIG_BT_MESH */
#if defined(CONFIG_ZTEST_MEMSLAB)
	&mslab1,
#endif /* CONFIG_ZTEST_MEMSLAB */
	NULL,
};
/* k_mem_slab END */

/* bt_mesh_subnet_cb START */
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_subnet_evt_app_keys;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_subnet_evt_beacon;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_subnet_evt_friend;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_subnet_evt_lpn;
extern const struct bt_mesh_subnet_cb bt_mesh_subnet_cb_subnet_evt_proxy;
const struct bt_mesh_subnet_cb * const _bt_mesh_subnet_cb_list[] =
{
#if defined(CONFIG_BT_MESH)
	&bt_mesh_subnet_cb_subnet_evt_app_keys,
	&bt_mesh_subnet_cb_subnet_evt_beacon,
#if defined(CONFIG_BT_MESH_FRIEND)
	&bt_mesh_subnet_cb_subnet_evt_friend,
#endif /* CONFIG_BT_MESH_FRIEND */
#if defined(CONFIG_BT_MESH_LPN)
	&bt_mesh_subnet_cb_subnet_evt_lpn,
#endif /* CONFIG_BT_MESH_LPN */
#if defined(CONFIG_BT_MESH_PROXY)
	&bt_mesh_subnet_cb_subnet_evt_proxy,
#endif /* CONFIG_BT_MESH_PROXY */
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_subnet_cb END */

/* bt_mesh_app_key_cb START */
extern const struct bt_mesh_app_key_cb bt_mesh_app_key_cb_app_key_evt;
const struct bt_mesh_app_key_cb * const _bt_mesh_app_key_cb_list[] =
{
#if defined(CONFIG_BT_MESH)
	&bt_mesh_app_key_cb_app_key_evt,
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_app_key_cb END */

/* bt_mesh_hb_cb START */
extern const struct bt_mesh_hb_cb hb_cb;
const struct bt_mesh_hb_cb * const _bt_mesh_hb_cb_list[] =
{
#if defined(CONFIG_BT_MESH)
#if defined(CONFIG_BT_MESH_DEMO)
	&hb_cb,
#endif /* CONFIG_BT_MESH_DEMO */
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_hb_cb END */

/* bt_mesh_proxy_cb START */
const struct bt_mesh_proxy_cb * const _bt_mesh_proxy_cb_list[] =
{
#if defined(CONFIG_BT_MESH)
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_proxy_cb END */

/* bt_mesh_friend_cb START */
extern const struct bt_mesh_friend_cb bt_mesh_friend_cb_friend_cb;
const struct bt_mesh_friend_cb * const _bt_mesh_friend_cb_list[] =
{
#if defined(CONFIG_BT_TESTER)
#if defined(CONFIG_BT_MESH)
	&bt_mesh_friend_cb_friend_cb,
#endif /* CONFIG_BT_MESH */
#endif /* CONFIG_BT_TESTER */
	NULL,
};
/* bt_mesh_friend_cb END */

/* bt_mesh_lpn_cb START */
extern const struct bt_mesh_lpn_cb bt_mesh_lpn_cb_lpn_cb;
extern const struct bt_mesh_lpn_cb bt_mesh_lpn_cb_tester_lpn_cb;
const struct bt_mesh_lpn_cb * const _bt_mesh_lpn_cb_list[] =
{
#if defined(CONFIG_BT_SHELL)
#if defined(CONFIG_BT_MESH_SHELL)
#if defined(CONFIG_BT_MESH_LOW_POWER)
	&bt_mesh_lpn_cb_lpn_cb,
#endif /* CONFIG_BT_MESH_LOW_POWER */
#endif /* CONFIG_BT_MESH_SHELL */
#endif /* CONFIG_BT_SHELL */
#if defined(CONFIG_BT_TESTER)
#if defined(CONFIG_BT_MESH)
	&bt_mesh_lpn_cb_tester_lpn_cb,
#endif /* CONFIG_BT_MESH */
#endif /* CONFIG_BT_TESTER */
	NULL,
};
/* bt_mesh_lpn_cb END */

/* kernel START */
extern const struct init_entry __init_sys_init_init_mem_slab_module;
extern const struct init_entry __init_sys_init_bt_uart_init;
extern const struct init_entry __init_sys_init_k_sys_work_q_init;
const struct init_entry * const _init_entry_list[] =
{
	&__init_sys_init_init_mem_slab_module,
#if defined(CONFIG_BT_HCI)
	&__init_sys_init_bt_uart_init,
#endif /* CONFIG_BT_HCI */
#if defined(CONFIG_ZEPHYR_WORK_QUEUE)
	&__init_sys_init_k_sys_work_q_init,
#endif /* CONFIG_ZEPHYR_WORK_QUEUE */
	NULL,
};
/* kernel END */
