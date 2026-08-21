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

#include <unistd.h>
#include <sched.h>

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
	/* The init table is a set of one-shot global initializers, so it must
	 * run once per live owner. Two of its entries - k_sys_work_q_init and
	 * long_wq_init - start a work-queue thread on a K_THREAD_STACK_DEFINE()
	 * array, which is one static buffer, and k_work_queue_start() also
	 * re-initializes the queue semaphore. Two callers therefore put two
	 * threads on the same stack and memset a semaphore that already has a
	 * waiter parked on it; the second thread hard-faults as soon as it
	 * touches the queue. That is what killed the "BT LW WQ" thread a
	 * couple of seconds into boot when both the sf32lb52 bth4 driver and
	 * bluetoothd's bt_sal_init() called this.
	 *
	 * The threads are pthreads of the caller, though, so they die with it.
	 * A guard that only ever runs the table once would leave a restarted
	 * bluetoothd with no work queues at all, hence the liveness check
	 * rather than a plain flag: re-run only when the process that
	 * initialized us is gone.
	 */
	static pid_t owner = -1;
	struct sched_param param;

	if (owner >= 0 &&
	    (owner == getpid() || sched_getparam(owner, &param) == 0)) {
		return;
	}

	owner = getpid();

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
