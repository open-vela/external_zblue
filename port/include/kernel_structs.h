/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The purpose of this file is to provide essential/minimal kernel structure
 * definitions, so that they can be used without including kernel.h.
 *
 * The following rules must be observed:
 *  1. kernel_structs.h shall not depend on kernel.h both directly and
 *    indirectly (i.e. it shall not include any header files that include
 *    kernel.h in their dependency chain).
 *  2. kernel.h shall imply kernel_structs.h, such that it shall not be
 *    necessary to include kernel_structs.h explicitly when kernel.h is
 *    included.
 */

#ifndef ZEPHYR_KERNEL_INCLUDE_KERNEL_STRUCTS_H_
#define ZEPHYR_KERNEL_INCLUDE_KERNEL_STRUCTS_H_

#include <kernel_includes.h>
#include <sys/atomic.h>
#include <zephyr/types.h>
#include <sys/dlist.h>
#include <sys/util.h>

#define K_NUM_PRIORITIES \
	(CONFIG_NUM_COOP_PRIORITIES + CONFIG_NUM_PREEMPT_PRIORITIES + 1)

#define K_NUM_PRIO_BITMAPS ((K_NUM_PRIORITIES + 31) >> 5)

/* Not a real thread */
#define _THREAD_DUMMY (BIT(0))

/* Thread is waiting on an object */
#define _THREAD_PENDING (BIT(1))

/* Thread has not yet started */
#define _THREAD_PRESTART (BIT(2))

/* Thread has terminated */
#define _THREAD_DEAD (BIT(3))

/* Thread is suspended */
#define _THREAD_SUSPENDED (BIT(4))

/* Thread is being aborted (SMP only) */
#define _THREAD_ABORTING (BIT(5))

/* Thread was aborted in interrupt context (SMP only) */
#define _THREAD_ABORTED_IN_ISR (BIT(6))

/* Thread is present in the ready queue */
#define _THREAD_QUEUED (BIT(7))

/* end - states */

/* lowest value of _thread_base.preempt at which a thread is non-preemptible */
#define _NON_PREEMPT_THRESHOLD 0x0080

/* highest value of _thread_base.preempt at which a thread is preemptible */
#define _PREEMPT_THRESHOLD (_NON_PREEMPT_THRESHOLD - 1)

/* kernel spinlock type */

struct k_spinlock {
  spinlock_t lock;
};

#endif /* ZEPHYR_KERNEL_INCLUDE_KERNEL_STRUCTS_H_ */
