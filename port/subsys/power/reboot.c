/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file common target reboot functionality
 *
 * @details See misc/Kconfig and the reboot help for details.
 */

#include <kernel.h>
#include <sys/printk.h>
#include <power/reboot.h>
#include <sys/boardctl.h>

void sys_reboot(int type)
{
	(void)irq_lock();

#if defined(CONFIG_BOARDCTL_RESET)
	boardctl(BOARDIOC_RESET, type);
#elif defined(CONFIG_BOARDCTL_POWEROFF)
	boardctl(BOARDIOC_POWEROFF, type);
#endif

	/* should never get here */
	printk("Failed to reboot: spinning endlessly...\n");
	for (;;) { }
}
