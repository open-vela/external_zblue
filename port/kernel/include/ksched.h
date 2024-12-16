/*
 * Copyright (c) 2016-2017 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_KERNEL_INCLUDE_KSCHED_H_
#define ZEPHYR_KERNEL_INCLUDE_KSCHED_H_

#include <zephyr/kernel_structs.h>
#include <zephyr/tracing/tracing.h>
#include <stdbool.h>

static inline void z_reschedule_unlocked(void)
{
	(void) k_yield();
}

/*
 * APIs for working with the Zephyr kernel scheduler. Intended for use in
 * management of IPC objects, either in the core kernel or other IPC
 * implemented by OS compatibility layers, providing basic wait/wake operations
 * with spinlocks used for synchronization.
 *
 * These APIs are public and will be treated as contract, even if the
 * underlying scheduler implementation changes.
 */

/**
 * Wake up a thread pending on the provided wait queue
 *
 * Given a wait_q, wake up the highest priority thread on the queue. If the
 * queue was empty just return false.
 *
 * Otherwise, do the following, in order,  holding _sched_spinlock the entire
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
 * Wakes the specified thread.
 *
 * Given a specific thread, wake it up. This routine assumes that the given
 * thread is not on the timeout queue.
 *
 * @param thread Given thread to wake up.
 * @param is_timeout True if called from the timer ISR; false otherwise.
 *
 */
void z_sched_wake_thread(struct k_thread *thread, bool is_timeout);

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

/**
 * Atomically put the current thread to sleep on a wait queue, with timeout
 *
 * The thread will be added to the provided waitqueue. The lock, which should
 * be held by the caller with the provided key, will be released once this is
 * completely done and we have swapped out.
 *
 * The return value and data pointer is set by whoever woke us up via
 * z_sched_wake.
 *
 * @param lock Address of spinlock to release when we swap out
 * @param key Key to the provided spinlock when it was locked
 * @param wait_q Wait queue to go to sleep on
 * @param timeout Waiting period to be woken up, or K_FOREVER to wait
 *                indefinitely.
 * @param data Storage location for data pointer set when thread was woken up.
 *             May be NULL if not used.
 * @retval Return value set by whatever woke us up, or -EAGAIN if the timeout
 *         expired without being woken up.
 */
int z_sched_wait(struct k_spinlock *lock, k_spinlock_key_t key,
		 _wait_q_t *wait_q, k_timeout_t timeout, void **data);

#endif /* ZEPHYR_KERNEL_INCLUDE_KSCHED_H_ */
