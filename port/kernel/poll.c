/****************************************************************************
 * apps/external/zblue/port/kernel/poll.c
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

typedef struct {
	dq_entry_t          next;
	struct k_poll_event *events;
	int                 num_events;
	struct k_sem        sem;
} event_callback_t;

static dq_queue_t g_event_callback_list;

void k_poll_event_init(struct k_poll_event *event, uint32_t type, int mode, void *obj)
{
	event->type  = type;
	event->state = K_POLL_STATE_NOT_READY;
	event->obj   = obj;
}

void _handle_obj_poll_events(dq_queue_t *events, uint32_t state)
{
	struct k_poll_event *event;
	event_callback_t *callback;
	FAR dq_entry_t *entry;
	unsigned int key;
	int i;

	key = irq_lock();

	event = (struct k_poll_event *)dq_remfirst(events);
	if (!event)
		goto skip;

	if (event->type == K_POLL_TYPE_SIGNAL)
		event->signal->signaled = 1;
	else if (event->type == K_POLL_TYPE_SEM_AVAILABLE)
		k_sem_give(event->sem);

	for (entry = dq_peek(&g_event_callback_list); entry; entry = dq_next(entry)) {
		callback = (event_callback_t *)entry;
		for (i = 0; i < callback->num_events; i++) {
			if (event == &callback->events[i] && event->type == state) {
				k_sem_give(&callback->sem);
				break;
			}
		}
	}

skip:
	irq_unlock(key);
}

int k_poll_signal_raise(struct k_poll_signal *signal, int result)
{
	_handle_obj_poll_events(&signal->poll_events, K_POLL_STATE_SIGNALED);
	return 0;
}

static void k_poll_event_process(struct k_poll_event *event, bool queue)
{
	dq_queue_t *events;

	if (event->type == K_POLL_TYPE_DATA_AVAILABLE)
		events = &(event->queue->poll_events);
	else if (event->type == K_POLL_TYPE_SIGNAL)
		events = &(event->signal->poll_events);
	else if (event->type == K_POLL_TYPE_SEM_AVAILABLE)
		events = &(event->sem->poll_events);
	else
		return;

	if (queue)
		dq_addlast(&event->_node, events);
	else
		dq_rem(&event->_node, events);
}

static int k_poll_event_ready(struct k_poll_event *event)
{
	switch (event->type) {
		case K_POLL_TYPE_DATA_AVAILABLE:
			if (!sq_empty(&event->queue->data_q)) {
				event->state |= event->type;
				return true;
			}
			break;
		case K_POLL_TYPE_SEM_AVAILABLE:
			if (k_sem_count_get(event->sem) > 0) {
				k_sem_reset(event->sem);
				event->state |= event->type;
				return true;
			}
			break;
		case K_POLL_TYPE_SIGNAL:
			if (event->signal->signaled != 0) {
				event->signal->signaled = 0;
				event->state |= event->type;
				return true;
			}
			break;
	}

	return false;
}

static bool k_poll_events(struct k_poll_event *events, int num_events, int32_t timeout)
{
	int i;

	for (i = 0; i < num_events; i++)
		if (k_poll_event_ready(&events[i]))
			return false;

	return (timeout == K_NO_WAIT) ? false : true;
}

int k_poll(struct k_poll_event *events, int num_events, int32_t timeout)
{
	event_callback_t callback;
	int key, i;

	key = irq_lock();

	if (!k_poll_events(events, num_events, timeout))
		goto nowait;

	for (i = 0; i < num_events; i++)
		k_poll_event_process(&events[i], true);

	callback.events     = events;
	callback.num_events = num_events;
	k_sem_init(&callback.sem, 0, 1);
	dq_addlast(&callback.next, &g_event_callback_list);

	irq_unlock(key);

	k_sem_take(&callback.sem, timeout);

	key = irq_lock();

	dq_rem(&callback.next, &g_event_callback_list);
	k_sem_delete(&callback.sem);

	k_poll_events(events, num_events, K_NO_WAIT);

	for (i = 0; i < num_events; i++)
		k_poll_event_process(&events[i], false);

nowait:
	irq_unlock(key);

	return 0;
}
