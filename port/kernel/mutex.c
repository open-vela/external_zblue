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

int k_mutex_init(struct k_mutex *mutex)
{
	pthread_mutexattr_t attr;
	int ret;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	ret = pthread_mutex_init(&mutex->mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	return ret;
}

int k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout)
{
	struct timespec abstime;

	if (timeout == K_FOREVER)
		return pthread_mutex_lock(&mutex->mutex);
	else if (timeout == K_NO_WAIT)
		return pthread_mutex_trylock(&mutex->mutex);

	clock_gettime(CLOCK_REALTIME, &abstime);

	abstime.tv_sec += timeout / 1000;
	abstime.tv_nsec += (timeout % 1000) * 1000 * 1000;
	if (abstime.tv_nsec >= (1000 * 1000000)) {
		abstime.tv_sec += 1;
		abstime.tv_nsec -= (1000 * 1000000);
	}

	return pthread_mutex_timedlock(&mutex->mutex, &abstime);
}

int k_mutex_unlock(struct k_mutex *mutex)
{
	return pthread_mutex_unlock(&mutex->mutex);
}
