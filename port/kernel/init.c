/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Kernel initialization module
 *
 * This module contains routines that are used to initialize the kernel.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>

static int do_device_init(const struct init_entry *entry)
{
	const struct device *dev = entry->dev;
	int rc = 0;

	dev->state->init_res = 0;

	if (entry->init_fn.dev != NULL) {
		rc = entry->init_fn.dev(dev);
		/* Mark device initialized. If initialization
		 * failed, record the error condition.
		 */
		if (rc != 0) {
			if (rc < 0) {
				rc = -rc;
			}
			if (rc > UINT8_MAX) {
				rc = UINT8_MAX;
			}
			dev->state->init_res = rc;
		}
	}

	dev->state->initialized = true;

	return rc;
}

void z_sys_init(void)
{
	STRUCT_SECTION_FOREACH(init_entry, entry) {
		const struct device *dev = entry->dev;
		int result;

		if (dev != NULL) {
			result = do_device_init(entry);
		} else {
			result = entry->init_fn.sys();
		}
	}
}
