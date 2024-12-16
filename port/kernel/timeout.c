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

int64_t sys_clock_tick_get(void)
{
#if defined(CONFIG_SYSTEM_TIME64)
	return clock_systime_ticks();
#else
	static int64_t g_tick;

	unsigned int state = enter_critical_section();

	uint32_t tick = clock_systime_ticks();

	g_tick += (uint32_t)(tick - (uint32_t)g_tick);

	leave_critical_section(state);

	return g_tick;
#endif
}

int64_t k_uptime_ticks(void)
{
	return sys_clock_tick_get();
}

k_timeout_t sys_timepoint_timeout(k_timepoint_t timepoint)
{
	uint64_t now, remaining;

	if (timepoint.tick == UINT64_MAX) {
		return K_FOREVER;
	}
	if (timepoint.tick == 0) {
		return K_NO_WAIT;
	}

	now = sys_clock_tick_get();
	remaining = (timepoint.tick > now) ? (timepoint.tick - now) : 0;
	return K_TICKS(remaining);
}

k_timepoint_t sys_timepoint_calc(k_timeout_t timeout)
{
	k_timepoint_t timepoint;

	if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
		timepoint.tick = UINT64_MAX;
	} else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		timepoint.tick = 0;
	} else {
		k_ticks_t dt = timeout.ticks;

		if (IS_ENABLED(CONFIG_TIMEOUT_64BIT) && Z_TICK_ABS(dt) >= 0) {
			timepoint.tick = Z_TICK_ABS(dt);
		} else {
			timepoint.tick = sys_clock_tick_get() + MAX(1, dt);
		}
	}

	return timepoint;
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