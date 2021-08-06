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

static K_KERNEL_STACK_DEFINE(stack1, 1024);
static K_KERNEL_STACK_DEFINE(stack2, 1024);
static K_KERNEL_STACK_DEFINE(stack3, 1024);

static struct k_thread thread1_data;
static struct k_thread thread2_data;
static struct k_thread thread3_data;

static K_SEM_DEFINE(sem1, 0 ,1);
static K_SEM_DEFINE(sem2, 0 ,1);
static K_SEM_DEFINE(sem3, 0 ,1);

static int count = 10;

static void thread1(void *p1, void *p2, void *p3)
{
	uint64_t block;
	printk("start %s %lu\n", __FUNCTION__, k_uptime_get_32());

	for (int i = 1; i <= count; i++) {
		k_sem_give(&sem2);

		printk("%s #%d take forever\n", __FUNCTION__, i);
		k_sem_take(&sem1, K_FOREVER);
		printk("%s #%d wakeup\n", __FUNCTION__, i);
	}

	printk("start %s %lu\n", __FUNCTION__, k_uptime_get_32());

	block = sys_clock_timeout_end_calc(K_SECONDS(5));
	while (k_uptime_ticks() <= block)
		;

	printk("ready k_yield %s %lu\n", __FUNCTION__, k_uptime_get_32());

	k_yield();

	printk("end %s %lu\n", __FUNCTION__, k_uptime_get_32());
}

static void thread2(void *p1, void *p2, void *p3)
{
	printk("start %s %lu\n", __FUNCTION__, k_uptime_get_32());

	for (int i = 1; i <= count; i++) {
		printk("%s %d take forever\n", __FUNCTION__, i);
		k_sem_take(&sem2, K_FOREVER);
		printk("%s #%d wakeup\n", __FUNCTION__, i);

		printk("#%d before %s %lu\n", i, __FUNCTION__, k_uptime_get_32());
		k_sleep(K_MSEC(500));
		printk("#%d after %s %lu\n", i, __FUNCTION__, k_uptime_get_32());

		k_sem_give(&sem1);
	}

	k_sleep(K_SECONDS(1));

	printk("end %s %lu\n", __FUNCTION__, k_uptime_get_32());
	printk("PASSED\n");
}

static void thread3(void *p1, void *p2, void *p3)
{
	uint64_t block;

	printk("start %s %lu\n", __FUNCTION__, k_uptime_get_32());

	printk("%s will block 5 seconds, verify other low priority thread will not be scheduler\n",
	       __FUNCTION__);
	block = sys_clock_timeout_end_calc(K_SECONDS(5));
	while (k_uptime_ticks() <= block)
		;

	printk("block %s %lu\n", __FUNCTION__, k_uptime_get_32());
	k_sleep(K_SECONDS(10));

	printk("end %s %lu\n", __FUNCTION__, k_uptime_get_32());

	k_sleep(K_FOREVER);
}

int main(int argc, char *argv[])
{
	if (argc == 2) {
		count = atoi(argv[1]);
	}

	/* priority 0 */
	printk("create task 1 %p %d\n", stack1, K_KERNEL_STACK_SIZEOF(stack1));
	k_thread_create(&thread1_data, stack1, K_KERNEL_STACK_SIZEOF(stack1),
			(k_thread_entry_t)thread1, NULL, NULL, NULL, K_PRIO_COOP(0), 0, K_NO_WAIT);
	k_thread_name_set(&thread1_data, "thread1");

	/* priority 0 */
	printk("create task 2\n");
	k_thread_create(&thread2_data, stack2, K_KERNEL_STACK_SIZEOF(stack2),
			(k_thread_entry_t)thread2, NULL, NULL, NULL, K_PRIO_COOP(0), 0, K_NO_WAIT);
	k_thread_name_set(&thread2_data, "thread2");

	k_sleep(K_MSEC(100));

	/* priority 1 */
	printk("create task 3\n");
	k_thread_create(&thread3_data, stack3, K_KERNEL_STACK_SIZEOF(stack3),
			(k_thread_entry_t)thread3, NULL, NULL, NULL, K_PRIO_COOP(1), 0, K_NO_WAIT);
	k_thread_name_set(&thread3_data, "thread3");

	return 0;
}
