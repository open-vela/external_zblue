/****************************************************************************
 * apps/external/zblue/port/kernel/timeout.c
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
#include <sys_clock.h>

int64_t z_tick_get(void)
{
	return clock_systime_ticks();
}

int64_t k_uptime_ticks(void)
{
	return z_tick_get();
}

uint32_t arch_k_cycle_get_32(void)
{
	return (uint32_t)z_tick_get();
}

int64_t sys_clock_tick_get(void)
{
	return z_tick_get();
}

uint64_t sys_clock_timeout_end_calc(k_timeout_t timeout)
{
	if (K_TIMEOUT_EQ(timeout, K_FOREVER))
		return UINT64_MAX;
	else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT))
		return z_tick_get();

	return z_tick_get() + timeout.ticks;
}

k_ticks_t z_timeout_remaining(const struct _timeout *timeout)
{
	clock_t qtime, curr, elapsed;
	struct k_work_delayable *dwork;

	dwork = CONTAINER_OF(timeout, struct k_work_delayable, timeout);

	return wd_gettime(&dwork->work.wdog);
}

void z_add_timeout(struct _timeout *to, _timeout_func_t fn,
		   k_timeout_t timeout)
{
	struct k_work_delayable *dwork;

	if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
		return;
	}

	dwork = CONTAINER_OF(to, struct k_work_delayable, timeout);

	(void)wd_start(&dwork->work.wdog, timeout.ticks, (wdentry_t)fn, (wdparm_t)to);
}

int z_abort_timeout(struct _timeout *to)
{
	struct k_work_delayable *dwork;

	dwork = CONTAINER_OF(to, struct k_work_delayable, timeout);

	return wd_cancel(&dwork->work.wdog);
}
