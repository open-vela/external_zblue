/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief Header files included by kernel.h.
 */

#ifndef ZEPHYR_INCLUDE_KERNEL_INCLUDES_H_
#define ZEPHYR_INCLUDE_KERNEL_INCLUDES_H_

#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/spinlock.h>
#include <nuttx/wqueue.h>

#include <queue.h>
#include <semaphore.h>

#ifndef CONFIG_LEGACY_TIMEOUT_API
#define CONFIG_LEGACY_TIMEOUT_API
#endif

#ifndef CONFIG_SYS_CLOCK_EXISTS
#define CONFIG_SYS_CLOCK_EXISTS
#endif

#ifndef CONFIG_ATOMIC_OPERATIONS_C
#define CONFIG_ATOMIC_OPERATIONS_C
#endif

#include <stddef.h>
#include <zephyr/types.h>
#include <limits.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <sys/atomic.h>
#include <sys/__assert.h>
#include <sys/dlist.h>
#include <sys/slist.h>
#include <sys/util.h>
#include <kernel_structs.h>
#include <mempool_heap.h>
#include <syscall.h>
#include <sys/printk.h>
#include <sys_clock.h>
#include <spinlock.h>
#include <sys/thread_stack.h>

#endif /* ZEPHYR_INCLUDE_KERNEL_INCLUDES_H_ */
