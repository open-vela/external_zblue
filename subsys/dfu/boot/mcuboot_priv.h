/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2016-2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DFU_BOOT_MCUBOOT_H_
#define ZEPHYR_DFU_BOOT_MCUBOOT_H_

#include <storage/flash_map.h>

/* FLASH_AREA_ID() values used below are auto-generated by DT */
#ifdef CONFIG_TRUSTED_EXECUTION_NONSECURE
#define FLASH_AREA_IMAGE_PRIMARY FLASH_AREA_ID(image_0_nonsecure)
#define FLASH_AREA_IMAGE_SECONDARY FLASH_AREA_ID(image_1_nonsecure)
#else
#define FLASH_AREA_IMAGE_PRIMARY FLASH_AREA_ID(image_0)
#if FLASH_AREA_LABEL_EXISTS(image_1)
#define FLASH_AREA_IMAGE_SECONDARY FLASH_AREA_ID(image_1)
#endif
#endif /* CONFIG_TRUSTED_EXECUTION_NONSECURE */

#endif /* ZEPHYR_DFU_BOOT_MCUBOOT_H_ */
