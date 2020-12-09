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
static struct work_s g_work;
static bool init_sys_work_flag;

static void k_sys_work_init(FAR void *arg)
{
	k_sys_work_q.thread.pid = getpid();
}

void k_work_init(struct k_work *work, k_work_handler_t handler)
{
	memset(work, 0, sizeof(*work));
	work->handler = handler;

	if (init_sys_work_flag == false) {
		init_sys_work_flag = true;
		work_queue(LPWORK, &g_work, k_sys_work_init, NULL, 0);
	}
}

void k_delayed_work_init(struct k_delayed_work *work, k_work_handler_t handler)
{
	k_work_init(&work->work, handler);
}

int k_delayed_work_cancel(struct k_delayed_work *work)
{
	return work_cancel(LPWORK, &work->work.nwork);
}

int k_delayed_work_submit_to_queue(struct k_work_q *work_q,
		struct k_delayed_work *work,
		k_timeout_t delay)
{
	struct work_s *nwork = &work->work.nwork;

	if (work_available(nwork))
		return work_queue(LPWORK, nwork, (worker_t)work->work.handler, work, MSEC2TICK(delay));

	return 0;
}

void k_work_submit_to_queue(struct k_work_q *work_q, struct k_work *work)
{
	if (work_available(&work->nwork))
		work_queue(LPWORK, &work->nwork, (worker_t)work->handler, work, 0);
}

int32_t k_delayed_work_remaining_get(struct k_delayed_work *work)
{
	k_timeout_t qtime, curr;
	struct work_s *nwork;

	nwork = &work->work.nwork;
	if (work_available(nwork))
		return 0;

	curr  = clock_systime_ticks();
	qtime = nwork->qtime;

	if (curr > qtime + nwork->delay)
		return 0;

	return TICK2MSEC(curr - qtime);
}
