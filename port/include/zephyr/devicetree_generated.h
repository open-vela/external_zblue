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

extern struct device __device_dts_ord_DT_N_INST_0_zephyr_bt_hci_ttyHCI0_ORD;

#define DT_CHOSEN_zephyr_bt_hci_EXISTS  1
#define DT_CHOSEN_zephyr_bt_hci0   DT_N_INST_0_zephyr_bt_hci_ttyHCI0

#define DT_N_INST_0_zephyr_bt_hci_ttyHCI0_FULL_NAME CONFIG_BT_UART_ON_DEV_NAME

#if defined(CONFIG_BT_MC_DEVICE_INST)
extern struct device __device_dts_ord_DT_N_INST_1_zephyr_bt_hci_ttyHCI1_ORD;

#define DT_CHOSEN_zephyr_bt_hci1   DT_N_INST_1_zephyr_bt_hci_ttyHCI1

#define DT_N_INST_1_zephyr_bt_hci_ttyHCI1_FULL_NAME "/dev/ttyHCI1"
#endif /* CONFIG_BT_MC_DEVICE_INST */