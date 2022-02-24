/*
 * Copyright (c) 2016, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_KERNEL_THREAD_H_
#define ZEPHYR_INCLUDE_KERNEL_THREAD_H_

#ifdef CONFIG_DEMAND_PAGING_THREAD_STATS
#include <sys/mem_manage.h>
#endif

/**
 * @typedef k_thread_entry_t
 * @brief Thread entry point function type.
 *
 * A thread's entry point function is invoked when the thread starts executing.
 * Up to 3 argument values can be passed to the function.
 *
 * The thread terminates execution permanently if the entry point function
 * returns. The thread is responsible for releasing any shared resources
 * it may own (such as mutexes and dynamically allocated memory), prior to
 * returning.
 *
 * @param p1 First argument.
 * @param p2 Second argument.
 * @param p3 Third argument.
 *
 * @return N/A
 */

#ifdef CONFIG_THREAD_MONITOR
struct __thread_entry {
	k_thread_entry_t pEntry;
	void *parameter1;
	void *parameter2;
	void *parameter3;
};
#endif

/* can be used for creating 'dummy' threads, e.g. for pending on objects */
struct _thread_base {

	/* this thread's entry in a ready/wait queue */
	union {
		sys_dnode_t qnode_dlist;
		struct rbnode qnode_rb;
	};

	/* wait queue on which the thread is pended (needed only for
	 * trees, not dumb lists)
	 */
	_wait_q_t *pended_on;

	/* user facing 'thread options'; values defined in include/kernel.h */
	uint8_t user_options;

	/* thread state */
	uint8_t thread_state;

	/*
	 * scheduler lock count and thread priority
	 *
	 * These two fields control the preemptibility of a thread.
	 *
	 * When the scheduler is locked, sched_locked is decremented, which
	 * means that the scheduler is locked for values from 0xff to 0x01. A
	 * thread is coop if its prio is negative, thus 0x80 to 0xff when
	 * looked at the value as unsigned.
	 *
	 * By putting them end-to-end, this means that a thread is
	 * non-preemptible if the bundled value is greater than or equal to
	 * 0x0080.
	 */
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			uint8_t sched_locked;
			int8_t prio;
#else /* LITTLE and PDP */
			int8_t prio;
			uint8_t sched_locked;
#endif
		};
		uint16_t preempt;
	};

#ifdef CONFIG_SCHED_DEADLINE
	int prio_deadline;
#endif

	uint32_t order_key;

#ifdef CONFIG_SMP
	/* True for the per-CPU idle threads */
	uint8_t is_idle;

	/* CPU index on which thread was last run */
	uint8_t cpu;

	/* Recursive count of irq_lock() calls */
	uint8_t global_lock_count;

#endif

#ifdef CONFIG_SCHED_CPU_MASK
	/* "May run on" bits for each CPU */
	uint8_t cpu_mask;
#endif

	/* data returned by APIs */
	void *swap_data;

#ifdef CONFIG_SYS_CLOCK_EXISTS
	/* this thread's entry in a timeout queue */
	struct _timeout timeout;
#endif
};

typedef struct _thread_base _thread_base_t;

#if defined(CONFIG_THREAD_STACK_INFO)
/* Contains the stack information of a thread */
struct _thread_stack_info {
	/* Stack start - Represents the start address of the thread-writable
	 * stack area.
	 */
	uintptr_t start;

	/* Thread writable stack buffer size. Represents the size of the actual
	 * buffer, starting from the 'start' member, that should be writable by
	 * the thread. This comprises of the thread stack area, any area reserved
	 * for local thread data storage, as well as any area left-out due to
	 * random adjustments applied to the initial thread stack pointer during
	 * thread initialization.
	 */
	size_t size;

	/* Adjustment value to the size member, removing any storage
	 * used for TLS or random stack base offsets. (start + size - delta)
	 * is the initial stack pointer for a thread. May be 0.
	 */
	size_t delta;
};

typedef struct _thread_stack_info _thread_stack_info_t;
#endif /* CONFIG_THREAD_STACK_INFO */

#if defined(CONFIG_USERSPACE)
struct _mem_domain_info {
	/** memory domain queue node */
	sys_dnode_t mem_domain_q_node;
	/** memory domain of the thread */
	struct k_mem_domain *mem_domain;
};

#endif /* CONFIG_USERSPACE */

#ifdef CONFIG_THREAD_USERSPACE_LOCAL_DATA
struct _thread_userspace_local_data {
#if defined(CONFIG_ERRNO) && !defined(CONFIG_ERRNO_IN_TLS)
	int errno_var;
#endif
};
#endif

#ifdef CONFIG_THREAD_RUNTIME_STATS
struct k_thread_runtime_stats {
	/* Thread execution cycles */
#ifdef CONFIG_THREAD_RUNTIME_STATS_USE_TIMING_FUNCTIONS
	timing_t execution_cycles;
#else
	uint64_t execution_cycles;
#endif
};

typedef struct k_thread_runtime_stats k_thread_runtime_stats_t;

struct _thread_runtime_stats {
	/* Timestamp when last switched in */
#ifdef CONFIG_THREAD_RUNTIME_STATS_USE_TIMING_FUNCTIONS
	timing_t last_switched_in;
#else
	uint32_t last_switched_in;
#endif

	k_thread_runtime_stats_t stats;
};
#endif

struct z_poller {
	bool is_polling;
	uint8_t mode;
};

/**
 * @ingroup thread_apis
 * Thread Structure
 */
struct k_thread {
	/** static thread init data */
	void *init_data;
	sys_snode_t node;
};

typedef struct k_thread _thread_t;
typedef struct k_thread *k_tid_t;

#endif
