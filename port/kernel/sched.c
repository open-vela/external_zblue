/****************************************************************************
 * apps/external/zblue/port/kernel/sched.c
 *
 *   Copyright (C) 2020 Xiaomi InC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
#include <assert.h>
#include <kernel.h>

void k_sched_lock(void)
{
	sched_lock();
}

void k_sched_unlock(void)
{
	sched_unlock();
}

unsigned int arch_irq_lock(void)
{
	sched_lock();

	return 0;
}

void arch_irq_unlock(unsigned int key)
{
	sched_unlock();
}

bool arch_irq_unlocked(unsigned int key)
{
	return false;
}

void z_fatal_error(unsigned int reason, const z_arch_esf_t *esf)
{
	assert(false);
}

void assert_post_action(const char *file, unsigned int line)
{
	assert(false);
}

void k_yield(void)
{
	sched_yield();
}

int32_t k_sleep(k_timeout_t timeout)
{
	usleep(k_ticks_to_us_ceil32(timeout.ticks));
	return 0;
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

bool z_sched_wake_all(_wait_q_t *wait_q, int swap_retval,
				      void *swap_data)
{
	bool woken = false;

	while (z_sched_wake(wait_q, swap_retval, swap_data)) {
		woken = true;
	}

	/* True if we woke at least one thread up */
	return woken;
}