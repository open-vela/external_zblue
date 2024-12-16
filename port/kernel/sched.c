/******************************************************************************
 *
 * Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

#include <zephyr/kernel.h>

int32_t k_sleep(k_timeout_t timeout)
{
#if 0
	/* in case of K_FOREVER, we suspend */
	if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {

        k_thread_suspend(k_sched_current_thread_query());

		return (int32_t) K_TICKS_FOREVER;
	}
#endif

	return nxsig_usleep(k_ticks_to_us_ceil32(timeout.ticks));
}

void k_yield(void)
{
#if CONFIG_BT_THREAD_NO_PREEM
	sched_unlock();
#endif /* CONFIG_BT_THREAD_NO_PREEM */
	sched_yield();
#if CONFIG_BT_THREAD_NO_PREEM
	sched_lock();
#endif /* CONFIG_BT_THREAD_NO_PREEM */
}

k_tid_t k_sched_current_thread_query(void)
{
extern k_tid_t k_thread_current(void);
#ifdef CONFIG_SMP
	/* In SMP, _current is a field read from _current_cpu, which
	 * can race with preemption before it is read.  We must lock
	 * local interrupts when reading it.
	 */
	unsigned int k = arch_irq_lock();
#endif /* CONFIG_SMP */

    k_tid_t ret = k_thread_current();

#ifdef CONFIG_SMP
	arch_irq_unlock(k);
#endif /* CONFIG_SMP */
	return ret;
}

void k_sched_lock(void)
{
	if (!up_interrupt_context()) {
		sched_lock();
	}
}

void k_sched_unlock(void)
{
	if (!up_interrupt_context()) {
		sched_unlock();
	}
}

struct wait_sync{
	sys_dlist_t node;
	struct k_sem wait;
};

int z_sched_wait(struct k_spinlock *lock, k_spinlock_key_t key,
		 _wait_q_t *wait_q, k_timeout_t timeout, void **data)
{
	struct wait_sync sync = {
		.node = SYS_DLIST_STATIC_INIT(NULL),
		.wait = Z_SEM_INITIALIZER(sync.wait, 0, 1),
	};

	sys_dlist_append(&wait_q->waitq, &sync.node);

	k_spin_unlock(lock, key);
	return k_sem_take(&sync.wait, timeout);
}

bool z_sched_wake(_wait_q_t *wait_q, int swap_retval, void *swap_data)
{
	sys_dlist_t *nd;
	struct wait_sync *sync;

	nd = sys_dlist_get(&wait_q->waitq);
	if (!nd) {
		return false;
	}

	sync = CONTAINER_OF(nd, struct wait_sync, node);

	k_sem_give(&sync->wait);

	return true;
}