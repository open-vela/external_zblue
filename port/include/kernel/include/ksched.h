/*
 * Copyright (c) 2016-2017 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_KERNEL_INCLUDE_KSCHED_H_
#define ZEPHYR_KERNEL_INCLUDE_KSCHED_H_

#include <kernel_structs.h>
#include <timeout_q.h>
#include <tracing/tracing.h>
#include <stdbool.h>

/**
 * Wake up a thread pending on the provided wait queue
 *
 * Given a wait_q, wake up the highest priority thread on the queue. If the
 * queue was empty just return false.
 *
 * Otherwise, do the following, in order,  holding sched_spinlock the entire
 * time so that the thread state is guaranteed not to change:
 * - Set the thread's swap return values to swap_retval and swap_data
 * - un-pend and ready the thread, but do not invoke the scheduler.
 *
 * Repeated calls to this function until it returns false is a suitable
 * way to wake all threads on the queue.
 *
 * It is up to the caller to implement locking such that the return value of
 * this function (whether a thread was woken up or not) does not immediately
 * become stale. Calls to wait and wake on the same wait_q object must have
 * synchronization. Calling this without holding any spinlock is a sign that
 * this API is not being used properly.
 *
 * @param wait_q Wait queue to wake up the highest prio thread
 * @param swap_retval Swap return value for woken thread
 * @param swap_data Data return value to supplement swap_retval. May be NULL.
 * @retval true If a thread was woken up
 * @retval false If the wait_q was empty
 */
bool z_sched_wake(_wait_q_t *wait_q, int swap_retval, void *swap_data);

/**
 * Wake up all threads pending on the provided wait queue
 *
 * Convenience function to invoke z_sched_wake() on all threads in the queue
 * until there are no more to wake up.
 *
 * @param wait_q Wait queue to wake up the highest prio thread
 * @param swap_retval Swap return value for woken thread
 * @param swap_data Data return value to supplement swap_retval. May be NULL.
 * @retval true If any threads were woken up
 * @retval false If the wait_q was empty
 */
static inline bool z_sched_wake_all(_wait_q_t *wait_q, int swap_retval,
				    void *swap_data)
{
	bool woken = false;

	while (z_sched_wake(wait_q, swap_retval, swap_data)) {
		woken = true;
	}

	/* True if we woke at least one thread up */
	return woken;
}

#endif /* ZEPHYR_KERNEL_INCLUDE_KSCHED_H_ */
