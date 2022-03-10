/****************************************************************************
 * port/drivers/console/uart_pipe.c
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
#include <string.h>
#include <poll.h>
#include <kernel.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/printk.h>
#include <drivers/console/uart_pipe.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(uart_pipe, CONFIG_UART_CONSOLE_LOG_LEVEL);

static void poll_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(tester, poll_handler);

static struct file            g_filep;
static uint8_t               *recv_buf;
static size_t                 recv_buf_len;
static uart_pipe_recv_cb      app_cb;
static size_t                 recv_off;

static void poll_handler(struct k_work *work)
{
	ssize_t ret;

	ret = file_read(&g_filep, recv_buf + recv_off, recv_buf_len - recv_off);
	if (ret <= 0) {
		goto repeat;
	}

	/*
	 * Call application callback with received data. Application
	 * may provide new buffer or alter data offset.
	 */
	recv_off += ret;
	recv_buf = app_cb(recv_buf, &recv_off);

repeat:
	k_work_reschedule(&tester, K_MSEC(20));
}

int uart_pipe_send(const uint8_t *data, int len)
{
	ssize_t offset = 0, ret;
	
	while(len) {
		ret = file_write(&g_filep, data + offset, len);
		if (ret < 0) {
			__ASSERT_NO_MSG(false);
		}

		len -= ret;
		offset += ret;
	}
	
	return 0;
}

void uart_pipe_register(uint8_t *buf, size_t len, uart_pipe_recv_cb cb)
{
	int ret;

	recv_buf = buf;
	recv_buf_len = len;
	app_cb = cb;

	ret = file_open(&g_filep, CONFIG_UART_PIPE_ON_DEV_NAME,
			O_RDWR | O_BINARY | O_NONBLOCK);
	if(ret < 0) {
		return;
	}

	k_work_submit(&tester.work);
}
