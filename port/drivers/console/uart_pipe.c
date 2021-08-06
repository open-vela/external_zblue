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

static K_THREAD_STACK_DEFINE(pipe_thread_stack, CONFIG_UART_PIPE_RX_STACK_SIZE);
static struct k_thread        pipe_thread_data;

static struct file            g_filep;
static uint8_t               *recv_buf;
static size_t                 recv_buf_len;
static uart_pipe_recv_cb      app_cb;
static size_t                 recv_off;

static int data_send_internal(FAR const uint8_t *buf, size_t count, bool wait)
{
	ssize_t ret, nwritten = 0;

	while (nwritten != count) {
		ret = file_write(&g_filep, buf + nwritten, count - nwritten);
		if (ret < 0) {
			return ret;
		}

		nwritten += ret;
	}

	return nwritten;
}

static int data_send(FAR const uint8_t *buf, size_t count)
{
	return data_send_internal(buf, count, true);
}

static void pipe_thread(void *p1, void *p2, void *p3)
{
	uint8_t reset_cmd[5] = "\xee\xff\x00\x00\x00";
	int got;

	for (;;) {
		got = file_read(&g_filep, recv_buf + recv_off, recv_buf_len - recv_off);
		if (got <= 0) {
			break;
		}

		if (got == sizeof(reset_cmd) && !memcmp(recv_buf + recv_off, reset_cmd, got)) {
			app_cb(recv_buf + recv_off, (unsigned int *)&got);
			continue;
		}

		LOG_HEXDUMP_DBG(recv_buf + recv_off, got, "RX");

		/*
		 * Call application callback with received data. Application
		 * may provide new buffer or alter data offset.
		 */
		recv_off += got;
		recv_buf = app_cb(recv_buf, &recv_off);
	}
}

int uart_pipe_send(const uint8_t *data, int len)
{
	return data_send(data, len);
}

void uart_pipe_register(uint8_t *buf, size_t len, uart_pipe_recv_cb cb)
{
	int ret;

	ret = file_open(&g_filep, CONFIG_UART_PIPE_ON_DEV_NAME, O_RDWR | O_BINARY);
	if(ret < 0)
		return;

	recv_buf = buf;
	recv_buf_len = len;
	app_cb = cb;

	k_thread_create(&pipe_thread_data, pipe_thread_stack,
			K_THREAD_STACK_SIZEOF(pipe_thread_stack),
			pipe_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_UART_PIPE_RX_PRIO), 0, K_NO_WAIT);

	k_thread_name_set(&pipe_thread_data, "BT PIPE");
}
