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

static struct k_thread thread1_data;
static struct k_thread thread2_data;

static K_SEM_DEFINE(sem2, 0 ,1);

static K_QUEUE_DEFINE(queue1);

typedef struct qdata {
	sys_snode_t snode;
	uint32_t data;
} qdata_t;

static qdata_t data;
static qdata_t data1;
static qdata_t data2;

static int count = 10;

static void thread1(void *p1, void *p2, void *p3)
{
	void *ret;
	qdata_t *data;
	printk("start %s %lu\n", __FUNCTION__, k_uptime_get_32());

	k_sleep(K_MSEC(10));

	for (int i = 1; i <= count; i++) {
		printk("#%d Test empty queue get with no wait\n", i);
		ret = k_queue_get(&queue1, K_NO_WAIT);
		__ASSERT_NO_MSG(ret == NULL);
	}

	for (int i = 1; i <= count; i++) {
		printk("#%d Test empty queue get with small delay\n", i);
		ret = k_queue_get(&queue1, K_MSEC(100));
		__ASSERT_NO_MSG(ret == NULL);
	}

	for (int i = 1; i <= count; i++) {
		printk("#%d Test queue get with item\n", i);
		k_sem_give(&sem2);

		data = k_queue_get(&queue1, K_FOREVER);
		__ASSERT_NO_MSG(data != NULL);

		__ASSERT_NO_MSG(data->data == i);
	}

	for (int i = 1; i <= count; i++) {
		printk("#%d Test queue get with two item, one prepend\n", i);
		data = k_queue_get(&queue1, K_FOREVER);
		__ASSERT_NO_MSG(data != NULL);
		__ASSERT_NO_MSG(data->data == i + count);

		data = k_queue_get(&queue1, K_FOREVER);
		__ASSERT_NO_MSG(data != NULL);
		__ASSERT_NO_MSG(data->data == i);

		k_sem_give(&sem2);
	}

	for (int i = 1; i <= count; i++) {
		printk("#%d Test queue get with two item, one insert\n", i);
		data = k_queue_get(&queue1, K_FOREVER);
		__ASSERT_NO_MSG(data != NULL);
		__ASSERT_NO_MSG(data->data == i);

		data = k_queue_get(&queue1, K_FOREVER);
		__ASSERT_NO_MSG(data != NULL);
		__ASSERT_NO_MSG(data->data == i + count + count);

		data = k_queue_get(&queue1, K_FOREVER);
		__ASSERT_NO_MSG(data != NULL);
		__ASSERT_NO_MSG(data->data == i + count);

		k_sem_give(&sem2);
	}
	printk("end %s %lu\n", __FUNCTION__, k_uptime_get_32());
}

static void thread2(void *p1, void *p2, void *p3)
{
	printk("start %s %lu\n", __FUNCTION__, k_uptime_get_32());

	for (int i = 1; i <= count; i++) {
		k_sem_take(&sem2, K_FOREVER);

		data.data = i;
		k_queue_append(&queue1, &data.snode);
	}

	k_sleep(K_SECONDS(1));

	for (int i = 1; i <= count; i++) {
		__ASSERT_NO_MSG(k_queue_is_empty(&queue1));

		data.data = i;
		k_queue_append(&queue1, &data.snode);

		data1.data = i + count;
		k_queue_prepend(&queue1, &data1.snode);

		k_sem_take(&sem2, K_FOREVER);
	}

	for (int i = 1; i <= count; i++) {
		__ASSERT_NO_MSG(k_queue_is_empty(&queue1));

		data.data = i;
		k_queue_append(&queue1, &data.snode);

		data1.data = i + count;
		k_queue_append(&queue1, &data1.snode);

		data2.data = i + count + count;
		k_queue_insert(&queue1, &data.snode, &data2.snode);

		k_sem_take(&sem2, K_FOREVER);
	}

	printk("end %s %lu\n", __FUNCTION__, k_uptime_get_32());

	printk("PASSED\n");
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

	return 0;
}
