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
#include <sys/prctl.h>

#include <zephyr/kernel.h>

static sys_dlist_t g_task_list = SYS_DLIST_STATIC_INIT(&g_task_list);

typedef struct
{
	void *argv[4];
} k_thread_main_t;

bool k_is_in_isr(void)
{
	return false;
}

k_tid_t k_thread_current(void)
{
	struct k_thread *thread;
	void *pid = (void *)gettid();

#if !defined(CONFIG_ZEPHYR_WORK_QUEUE)
	if (pid == k_sys_work_q.thread.init_data)
		return &k_sys_work_q.thread;

#endif /* !CONFIG_ZEPHYR_WORK_QUEUE */

	SYS_DLIST_FOR_EACH_CONTAINER(&g_task_list, thread, base.qnode_dlist) {
		if (thread->init_data == pid) {
			return thread;
		}
	}

	return NULL;
}

static void *k_thread_main(void * args)
{
	struct sched_param param;
	k_thread_main_t *_main;
	void *_argv[4];

	_main = args;
	if (_main == NULL)
		return NULL;

	memcpy(_argv, _main->argv, sizeof(_argv));

	free(_main);

	sched_getparam(0, &param);
	sched_setscheduler(0, SCHED_FIFO, &param);

#if CONFIG_BT_THREAD_NO_PREEM
	sched_lock();
#endif /* CONFIG_BT_THREAD_NO_PREEM */

	((k_thread_entry_t)_argv[0])(_argv[1], _argv[2], _argv[3]);
	return NULL;
}

k_tid_t k_thread_create(struct k_thread *new_thread,
				  k_thread_stack_t *stack,
				  size_t stack_size,
				  k_thread_entry_t entry,
				  void *p1, void *p2, void *p3,
				  int prio, uint32_t options, k_timeout_t delay)
{
	k_thread_main_t *_main;
	pthread_attr_t pattr;
	cpu_set_t cpuset0;
	pthread_t pid;
	struct sched_param param = {
		.sched_priority = prio,
	};
	int ret;

	_main = malloc(sizeof(*_main));
	if (_main == NULL)
		return (k_tid_t)(intptr_t)-ENOMEM;

	_main->argv[0] = entry;
	_main->argv[1] = p1;
	_main->argv[2] = p2;
	_main->argv[3] = p3;

	pthread_attr_init(&pattr);
	pthread_attr_setstack(&pattr, stack, stack_size);
	pthread_attr_setschedparam(&pattr, &param);

	ret = pthread_create(&pid, &pattr, k_thread_main, _main);
	pthread_attr_destroy(&pattr);
	if (ret < 0) {
		free(_main);
		return (k_tid_t)-1;
	}

#ifdef CONFIG_SMP
	CPU_ZERO(&cpuset0);
	CPU_SET(0,&cpuset0);

	pthread_setaffinity_np(pid, sizeof(cpu_set_t), &cpuset0);
#endif /* CONFIG_SMP */

	new_thread->init_data = (void *)pid;
	sys_dlist_append(&g_task_list, &new_thread->base.qnode_dlist);

	return (k_tid_t)new_thread;
}

int k_thread_name_set(k_tid_t thread, const char *str)
{
    return prctl(PR_SET_NAME_EXT, str, (int)thread->init_data);
}

void k_thread_start(k_tid_t thread)
{
}

void k_thread_suspend(k_tid_t thread)
{
}

void k_thread_resume(k_tid_t thread)
{
}

void k_thread_abort(k_tid_t thread)
{
}