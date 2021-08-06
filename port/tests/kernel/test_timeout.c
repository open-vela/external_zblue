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

int main(int argc, char *argv[])
{
	int count = 1;

	if (argc == 2) {
		count = atoi(argv[1]);
	}

	printk("************TOTAL CYCLES %d*************\n", count);

	for (int i = 1; i <= count; i++) {
		uint32_t delay = 100 + i + 1;
		uint32_t start_timestamps = k_uptime_get_32(), end_timestamps;

		printk("#%d START TEST time:%lums ticks:%lld delta:%lld\n", i, start_timestamps,
		       k_uptime_ticks(), sys_clock_timeout_end_calc(K_MSEC(i)));

		k_sleep(K_MSEC(delay));

		end_timestamps = k_uptime_get_32();

		printk("#%d END TEST time:%lums ticks:%lld\n", i, end_timestamps, k_uptime_ticks());

		end_timestamps -= start_timestamps;
		end_timestamps -= i;
		__ASSERT_NO_MSG(end_timestamps >= 100);
	}

	printk("PASSED\n");

	return 0;
}
