/****************************************************************************
 * apps/external/zblue/port/kernel/queue.c
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
#include <toolchain.h>
#include <spinlock.h>

static void handle_poll_events(struct k_queue *queue, uint32_t state)
{
	_handle_obj_poll_events(&queue->poll_events, state);
}

void k_queue_cancel_wait(struct k_queue *queue)
{
}

void k_queue_init(struct k_queue *queue)
{
	k_spinlock_key_t key = k_spin_lock(&queue->lock);
	sq_init(&queue->data_q);
	k_spin_unlock(&queue->lock, key);
}

void k_queue_insert(struct k_queue *queue, void *prev, void *data)
{
	k_spinlock_key_t key = k_spin_lock(&queue->lock);
	sq_addafter(prev, data, &queue->data_q);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
}

void k_queue_append(struct k_queue *queue, void *data)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&queue->lock);
	sq_addlast(data, &queue->data_q);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
}

void k_queue_prepend(struct k_queue *queue, void *data)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&queue->lock);
	sq_addfirst(data, &queue->data_q);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
}

static void *k_queue_poll(struct k_queue *queue, k_timeout_t timeout)
{
	struct k_poll_event event;
	k_spinlock_key_t key;
	void *data;
	int err;

	k_poll_event_init(&event, K_POLL_TYPE_FIFO_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY, queue);

	event.state = K_POLL_STATE_NOT_READY;
	err = k_poll(&event, 1, timeout);

	if (err && err != -EAGAIN)
		return NULL;

	key = k_spin_lock(&queue->lock);
	data = sq_remfirst(&queue->data_q);
	k_spin_unlock(&queue->lock, key);

	return data;
}

void *k_queue_get(struct k_queue *queue, k_timeout_t timeout)
{
	k_spinlock_key_t key;
	void *data;

	if (sq_empty(&queue->data_q)) {
		if (timeout == K_NO_WAIT)
			return NULL;
	} else {
		key = k_spin_lock(&queue->lock);
		data = sq_remfirst(&queue->data_q);
		k_spin_unlock(&queue->lock, key);
		return data;
	}

	return k_queue_poll(queue, timeout);
}

int k_queue_append_list(struct k_queue *queue, void *head, void *tail)
{
	sys_slist_append_list(&queue->data_q, head, tail);
	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
	return 0;
}

int k_queue_is_empty(struct k_queue *queue)
{
	return sys_slist_is_empty(&queue->data_q);
}
