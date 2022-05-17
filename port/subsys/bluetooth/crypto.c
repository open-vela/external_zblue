/*
 * Copyright (c) 2022 Xiaomi Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>

#include <zephyr.h>
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/crypto.h>

#include <nuttx/crypto/crypto.h>

#include "hci_core.h"

int bt_rand(void *buf, size_t len)
{
	return bt_hci_le_rand(buf, len);
}

int bt_encrypt_le(const uint8_t key[16], const uint8_t plaintext[16],
		  uint8_t enc_data[16])
{
	int err;
	uint8_t swap_key[16], swap_plaintext[16];

	sys_memcpy_swap(swap_key, key, 16);
	sys_memcpy_swap(swap_plaintext, plaintext, 16);

	err = aes_cypher(enc_data, swap_plaintext, 16, NULL, swap_key, 16, AES_MODE_ECB, true);
	if (err) {
		return err;
	}

	sys_mem_swap(enc_data, 16);

	return 0;
}

int bt_encrypt_be(const uint8_t key[16], const uint8_t plaintext[16],
		  uint8_t enc_data[16])
{
	return aes_cypher(enc_data, plaintext, 16, NULL, key, 16, AES_MODE_ECB, true);
}
