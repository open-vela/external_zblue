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



#include <sys/time.h>

int k_sem_init(struct k_sem *sem,
		unsigned int initial_count, unsigned int limit)
{
	sem->limit = limit;
	return nxsem_init(&sem->sem, 0, initial_count);
}

void k_sem_give(struct k_sem *sem)
{
	int semcount;

	nxsem_get_value(&sem->sem, &semcount);

	if (semcount < 0 || (uint32_t)semcount < sem->limit)
		nxsem_post(&sem->sem);
}

int k_sem_take(struct k_sem *sem, k_timeout_t timeout)
{
	int ret;
	uint32_t ms;
	struct timespec abstime;

	if (K_TIMEOUT_EQ(timeout, K_FOREVER))
		return nxsem_wait_uninterruptible(&sem->sem);
	else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		ret = nxsem_trywait(&sem->sem);
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

	ret = nxsem_timedwait_uninterruptible(&sem->sem, &abstime);
	if (ret) {
		return -EAGAIN;
	}

	return 0;
}

unsigned int k_sem_count_get(struct k_sem *sem)
{
	int val;
	int ret;

	ret = nxsem_get_value(&sem->sem, &val);
	if (ret)
		val = ret;

	return val;
}

void k_sem_reset(struct k_sem *sem)
{
	k_sem_init(sem, 0, 0);
}
