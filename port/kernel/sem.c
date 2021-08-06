/****************************************************************************
 * apps/external/zblue/port/kernel/sem.c
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

#include <kernel.h>
#include <kernel_structs.h>
#include <toolchain.h>

#include <sys/time.h>

int k_sem_init(struct k_sem *sem,
		unsigned int initial_count, unsigned int limit)
{
	sem->limit = limit;
	return sem_init(&sem->sem, 0, initial_count);
}

void k_sem_give(struct k_sem *sem)
{
	int semcount;

	sem_getvalue(&sem->sem, &semcount);

	if (semcount < (int)sem->limit)
		sem_post(&sem->sem);
}

int k_sem_take(struct k_sem *sem, k_timeout_t timeout)
{
	int ret;
	uint32_t ms;
	struct timespec abstime;

	if (K_TIMEOUT_EQ(timeout, K_FOREVER))
		return sem_wait(&sem->sem);
	else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		ret = sem_trywait(&sem->sem);
		if (ret) {
			return -EBUSY;
		}

		return 0;
	}

	clock_gettime(CLOCK_REALTIME, &abstime);

	ms = k_ticks_to_ms_ceil32(timeout.ticks);

	abstime.tv_sec += ms / MSEC_PER_SEC;
	abstime.tv_nsec += (ms % MSEC_PER_SEC) * NSEC_PER_MSEC;
	if (abstime.tv_nsec >= NSEC_PER_SEC) {
		abstime.tv_sec += 1;
		abstime.tv_nsec -= NSEC_PER_SEC;
	}

	ret = sem_timedwait(&sem->sem, &abstime);
	if (ret) {
		return -EAGAIN;
	}

	return 0;
}

unsigned int k_sem_count_get(struct k_sem *sem)
{
	int val;
	int ret;

	ret = sem_getvalue(&sem->sem, &val);
	if (ret)
		val = ret;

	return val;
}

void k_sem_reset(struct k_sem *sem)
{
	k_sem_init(sem, 0, 0);
}
