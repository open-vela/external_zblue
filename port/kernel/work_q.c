/****************************************************************************
 * apps/external/zblue/port/kernel/work_q.c
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

#include <sys/types.h>
#include <unistd.h>
#include <string.h>

struct k_work_q k_sys_work_q;

static void work_cb(void *arg)
{
	struct k_work *work = arg;

	k_sys_work_q.thread.init_data = (void *)getpid();

	if (work->handler) {
		work->handler(work);
	}
}

static int work_submit(struct k_work_q *work_q,
		       struct k_work *work,
		       k_timeout_t delay)
{
	if (work_available(&work->nwork)) {
		work_queue(LPWORK, &work->nwork, work_cb, work, delay.ticks);
	}

	return 0;
}

int k_work_cancel_delayable(struct k_work_delayable *dwork)
{
	return work_cancel(LPWORK, &dwork->work.nwork);
}

bool k_work_cancel_delayable_sync(struct k_work_delayable *dwork,
				  struct k_work_sync *sync)
{
	return (bool)k_work_cancel_delayable(dwork);
}

int k_work_reschedule_for_queue(struct k_work_q *work_q,
				struct k_work_delayable *dwork,
				k_timeout_t delay)
{
	k_work_cancel_delayable(dwork);

	return work_submit(work_q, &dwork->work, delay);
}

int k_work_schedule(struct k_work_delayable *dwork,
		    k_timeout_t delay)
{
	return work_submit(NULL, &dwork->work, delay);
}

int k_work_reschedule(struct k_work_delayable *dwork,
		      k_timeout_t delay)
{
	return k_work_reschedule_for_queue(NULL, dwork, delay);
}

void k_work_init(struct k_work *work, k_work_handler_t handler)
{
	memset(work, 0, sizeof(*work));
	work->handler = handler;
}

int k_work_submit_to_queue(struct k_work_q *work_q, struct k_work *work)
{
	return work_submit(work_q, work, K_NO_WAIT);
}

int k_work_submit(struct k_work *work)
{
	return k_work_submit_to_queue(NULL, work);
}

void k_work_init_delayable(struct k_work_delayable *dwork,
			   k_work_handler_t handler)
{
	k_work_init(&dwork->work, handler);
}

int k_work_delayable_busy_get(const struct k_work_delayable *dwork)
{
	return !work_available(&dwork->work.nwork);
}
