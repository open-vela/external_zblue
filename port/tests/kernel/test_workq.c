/****************************************************************************
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

#include <stdio.h>
#include <stdlib.h>
#include <kernel.h>
#include <logging/log.h>

static K_SEM_DEFINE(sem, 0 ,1);

static void k_work_handler_1(struct k_work *work)
{
	printk("#handler:%lums \n", k_uptime_get_32());

	k_sem_give(&sem);
}

static K_WORK_DELAYABLE_DEFINE(work1, k_work_handler_1);

int main(int argc, char *argv[])
{
	int count = 1;
	int err;

	if (argc == 2) {
		count = atoi(argv[1]);
	}

	printk("#Test k_work_submit\n");

	for (int i = 1; i <= count; i++) {
		printk("#%d time:%lums \n", i, k_uptime_get_32());

		k_work_schedule(&work1, K_NO_WAIT);

		err = k_sem_take(&sem, K_FOREVER);
		__ASSERT_NO_MSG(err == 0);
	}

	__ASSERT_NO_MSG(k_sem_count_get(&sem) == 0);

	printk("#Test k_work_submit with delay\n");

	for (int i = 1; i <= count; i++) {
		printk("#%d time:%lums \n", i, k_uptime_get_32());

		k_work_schedule(&work1, K_MSEC(100 + i));

		err = k_sem_take(&sem, K_MSEC(10));
		printk("#take timeout %d time:%lums \n", err, k_uptime_get_32());
		__ASSERT_NO_MSG(err != 0);

		err = k_sem_take(&sem, K_FOREVER);
		__ASSERT_NO_MSG(err == 0);
	}

	__ASSERT_NO_MSG(k_sem_count_get(&sem) == 0);

	printk("#Test k_work_submit with canceled\n");

	for (int i = 1; i <= count; i++) {
		printk("#%d time:%lums \n", i, k_uptime_get_32());

		k_work_schedule(&work1, K_MSEC(10));

		k_sleep(K_MSEC(2));

		k_work_cancel_delayable(&work1);

		err = k_sem_take(&sem, K_MSEC(100));
		printk("#take timeout %d time:%lums \n", err, k_uptime_get_32());
		__ASSERT_NO_MSG(err != 0);
	}

	__ASSERT_NO_MSG(k_sem_count_get(&sem) == 0);

	printk("#Test k_work_submit with rescheduler\n");

	for (int i = 1; i <= count; i++) {
		printk("#%d time:%lums \n", i, k_uptime_get_32());

		k_work_schedule(&work1, K_MSEC(10));

		k_sleep(K_MSEC(2));

		k_work_reschedule(&work1, K_MSEC(100));

		err = k_sem_take(&sem, K_MSEC(20));

		printk("#take timeout %d time:%lums \n", err, k_uptime_get_32());
		__ASSERT_NO_MSG(err != 0);

		err = k_sem_take(&sem, K_FOREVER);
		__ASSERT_NO_MSG(err == 0);
	}

	printk("#Test k_work_submit remaining get\n");

	for (int i = 1; i <= count; i++) {
		printk("#%d time:%lums \n", i, k_uptime_get_32());

		k_work_schedule(&work1, K_MSEC(200));

		k_sleep(K_MSEC(10));

		printk("#remaining after sleep %lu time:%lums \n",
		       k_work_delayable_remaining_get(&work1), k_uptime_get_32());

		err = k_sem_take(&sem, K_FOREVER);
		__ASSERT_NO_MSG(err == 0);

		printk("#remaining after timeout %lu time:%lums \n",
		       k_work_delayable_remaining_get(&work1), k_uptime_get_32());
	}

	printk("PASSED\n");

	return 0;
}
