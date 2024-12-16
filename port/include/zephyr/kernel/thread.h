/*
 * Copyright (c) 2016, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_KERNEL_THREAD_H_
#define ZEPHYR_INCLUDE_KERNEL_THREAD_H_

#ifdef CONFIG_DEMAND_PAGING_THREAD_STATS
#include <zephyr/kernel/mm/demand_paging.h>
#endif /* CONFIG_DEMAND_PAGING_THREAD_STATS */

#include <zephyr/kernel/stats.h>
#include <zephyr/arch/arch_interface.h>

/* can be used for creating 'dummy' threads, e.g. for pending on objects */
struct _thread_base {

	/* this thread's entry in a ready/wait queue */
	sys_dnode_t qnode_dlist;

	/* user facing 'thread options'; values defined in include/kernel.h */
	uint8_t user_options;

#ifdef CONFIG_SYS_CLOCK_EXISTS
	/* this thread's entry in a timeout queue */
	struct _timeout timeout;
#endif /* CONFIG_SYS_CLOCK_EXISTS */
};

typedef struct k_thread_runtime_stats {
}  k_thread_runtime_stats_t;

typedef struct _thread_base _thread_base_t;

/**
 * @ingroup thread_apis
 * Thread Structure
 */
struct k_thread {

	struct _thread_base base;

	/** static thread init data */
	void *init_data;

	/** resource pool */
	struct k_heap *resource_pool;
};

typedef struct k_thread _thread_t;
typedef struct k_thread *k_tid_t;

#endif /* ZEPHYR_INCLUDE_KERNEL_THREAD_H_ */
