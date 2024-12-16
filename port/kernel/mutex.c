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
	uint32_t ms;
	struct timespec abstime;

	if (K_TIMEOUT_EQ(timeout, K_FOREVER))
		return pthread_mutex_lock(&mutex->mutex);
	else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT))
		return pthread_mutex_trylock(&mutex->mutex);

	clock_gettime(CLOCK_REALTIME, &abstime);

	ms = k_ticks_to_ms_ceil32(timeout.ticks);

	abstime.tv_sec += ms / MSEC_PER_SEC;
	abstime.tv_nsec += (ms % MSEC_PER_SEC) * NSEC_PER_MSEC;
	if (abstime.tv_nsec >= NSEC_PER_SEC) {
		abstime.tv_sec += 1;
		abstime.tv_nsec -= NSEC_PER_SEC;
	}

	return pthread_mutex_timedlock(&mutex->mutex, &abstime);
}

int k_mutex_unlock(struct k_mutex *mutex)
{
	return pthread_mutex_unlock(&mutex->mutex);
}