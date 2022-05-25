/****************************************************************************
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

#include <stdio.h>
#include <stdlib.h>
#include <kernel.h>
#include <logging/log.h>
#include <fs/nvs.h>

static int delay_ms;
static struct nvs_fs fs;
static uint16_t filling_id = 0;

static bool mem_verify(const uint8_t *src, uint8_t var, int len)
{
	for (int i = 0; i < len; i++) {
		if (src[i] != var) {
			return false;
		}
	}

	return true;
}

void test_nvs_full_sector(void)
{
	int err;
	ssize_t len, to_len;
	uint16_t i, data_read;
	uint8_t value[49];

	fs.sector_size = 4096;
	fs.sector_count = 3;

	err = nvs_mount(&fs);
	__ASSERT(err == 0,  "nvs_mount call failure: %d", err);

	filling_id = 0;

	err = nvs_clear(&fs);
	__ASSERT(err == 0,  "nvs_clear call failure: %d", err);

	err = nvs_mount(&fs);
	__ASSERT(err == 0,  "nvs_mount call failure: %d", err);

	while (1) {
		if (delay_ms) {
			k_sleep(K_MSEC(delay_ms));
		}

		to_len = filling_id % sizeof(value);
		to_len = MAX(1, to_len);

		(void)memset(value, filling_id & 0xff, to_len);

		printk("Write id 0x%04x len %d\n", filling_id, to_len);

		len = nvs_write(&fs, filling_id, value,
				to_len);
		if (len == -ENOSPC) {
			break;
		}

		if (!len) {
			filling_id++;
			continue;
		}

		__ASSERT(len == to_len, "nvs_write failed: %d",
			 len);

		filling_id++;
	}

	if (delay_ms) {
		k_sleep(K_MSEC(delay_ms));
	}

	/* check whether can delete whatever from full storage */
	err = nvs_delete(&fs, 1);
	__ASSERT(err == 0,  "nvs_delete call failure: %d", err);

	/* the last sector is full now, test re-initialization */
	err = nvs_mount(&fs);
	__ASSERT(err == 0,  "nvs_mount call failure: %d", err);

	len = nvs_write(&fs, filling_id, &filling_id, sizeof(filling_id));
	__ASSERT(len == sizeof(filling_id), "nvs_write failed: %d", len);

	/* sanitycheck on NVS content */
	for (i = 0; i < filling_id; i++) {
		if (delay_ms) {
			k_sleep(K_MSEC(delay_ms));
		}

		len = nvs_read(&fs, i, &value, sizeof(value));
		if (i == 1) {
			__ASSERT(len == -ENOENT,
				 "nvs_read shouldn't found the entry: %d",
				 len);
		} else {
			uint8_t val = i & 0xff;

			data_read = MAX((i % sizeof(value)), 1);
			__ASSERT(len == data_read,
				 "nvs_read failed: %d", len);
			__ASSERT(mem_verify(value, val, data_read),
				 "read unexpected data: %d instead of %d",
				 value[0], val);
		}

		printk("Read id 0x%04x len %d\n", i, len);
	}
}

void test_delete(void)
{
	int err;
	ssize_t len;
	uint16_t data_read;
	uint32_t ate_wra, data_wra;

	fs.sector_count = 3;

	err = nvs_mount(&fs);
	__ASSERT(err == 0,  "nvs_mount call failure: %d", err);

	len = nvs_read(&fs, filling_id, &data_read, sizeof(data_read));
	__ASSERT(len == sizeof(data_read), "nvs_read found the entry: %d",
		 len);

	/* delete existing entry */
	err = nvs_delete(&fs, 0);
	__ASSERT(err == 0,  "nvs_delete call failure: %d", err);

	len = nvs_read(&fs, 0, &data_read, sizeof(data_read));
	__ASSERT(len == -ENOENT, "nvs_read shouldn't found the entry: %d",
		 len);

	ate_wra = fs.ate_wra;
	data_wra = fs.data_wra;

	if (delay_ms) {
		k_sleep(K_MSEC(delay_ms));
	}

	/* delete already deleted entry */
	err = nvs_delete(&fs, 0);
	__ASSERT(err == 0,  "nvs_delete call failure: %d", err);
	__ASSERT(ate_wra == fs.ate_wra && data_wra == fs.data_wra,
		  "delete already deleted entry should not make"
		  " any footprint in the storage");

	/* delete all on NVS content */
	for (uint16_t i = filling_id; i > 0; i--) {
		if (delay_ms) {
			k_sleep(K_MSEC(delay_ms));
		}

		printk("delete id 0x%04x\n", i);

		err = nvs_delete(&fs, i);
		__ASSERT(err == 0,  "nvs_delete call failure: %d", err);
	}	
}

int main(int argc, char *argv[])
{
	int count = 1;
	const struct flash_area *map;

	if (argc >= 2) {
		count = atoi(argv[1]);
	}

	if (argc >= 3) {
		delay_ms = atoi(argv[2]);
	}

	printk("#Test NVM with cycles %d delay %d\n",
		count, delay_ms);

	flash_area_open(0, &map);

	for (int i = 0; i < count; i++) {
		printk("#test_nvs_full_sector count:%d\n", i);
		test_nvs_full_sector();

		printk("#test_delete count:%d\n", i);
		test_delete();
	}

	printk("OVER\n");

	return 0;
}
