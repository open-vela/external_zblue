/* hci_ecc.h - HCI ECC emulation */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void bt_hci_ecc_init(struct bt_dev *hdev);
int bt_hci_ecc_send(struct bt_dev *hdev, struct net_buf *buf);
void bt_hci_ecc_supported_commands(uint8_t *supported_commands);
