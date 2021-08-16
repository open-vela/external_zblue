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
	sys_dnode_t         next;
	struct k_poll_event *events;
	int                 num_events;
	struct k_sem        sem;
} poll_event_cb_t;

static sys_dlist_t poll_list = SYS_DLIST_STATIC_INIT(&poll_list);

void k_poll_event_init(struct k_poll_event *event, uint32_t type, int mode, void *obj)
{
	event->type  = type;
	event->state = K_POLL_STATE_NOT_READY;
	event->obj   = obj;
}

static bool event_match(struct k_poll_event *event, uint32_t state)
{
	if (state == K_POLL_STATE_SIGNALED &&
	    event->type == K_POLL_TYPE_SIGNAL) {
		return true;
	} else if (state == K_POLL_STATE_DATA_AVAILABLE &&
		   event->type == K_POLL_TYPE_DATA_AVAILABLE) {
		return true;
	} else if (state == K_POLL_STATE_MSGQ_DATA_AVAILABLE &&
		   event->type == K_POLL_TYPE_MSGQ_DATA_AVAILABLE) {
		return true;
	} else {
		return false;
	}
}

void z_handle_obj_poll_events(sys_dlist_t *events, uint32_t state)
{
	struct k_poll_event *event;
	poll_event_cb_t *cb, *cb_next;
	unsigned int key;
	int i;

	key = irq_lock();

	event = (struct k_poll_event *)sys_dlist_get(events);
	if (!event) {
		goto skip;
	}

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&poll_list, cb, cb_next, next) {
		for (i = 0; i < cb->num_events; i++) {
			if (event != &cb->events[i]) {
				continue;
			}

			if (state == K_POLL_STATE_CANCELLED) {
				k_sem_give(&cb->sem);
				break;
			}

			if (event_match(event, state)) {
				k_sem_give(&cb->sem);
				break;
			}
		}
	}

skip:
	irq_unlock(key);
}

int k_poll_signal_raise(struct k_poll_signal *signal, int result)
{
	signal->signaled = 1;
	z_handle_obj_poll_events(&signal->poll_events, K_POLL_STATE_SIGNALED);
	return 0;
}

static void poll_event_add(struct k_poll_event *event)
{
	sys_dnode_t *events;

	if (event->type == K_POLL_TYPE_DATA_AVAILABLE)
		events = &(event->queue->poll_events);
	else if (event->type == K_POLL_TYPE_SIGNAL)
		events = &(event->signal->poll_events);
	else if (event->type == K_POLL_TYPE_SEM_AVAILABLE)
		events = &(event->sem->poll_events);
	else
		return;

	sys_dlist_append(events, &event->_node);
}

static void poll_event_remove(struct k_poll_event *event)
{
	if (sys_dnode_is_linked(&event->_node)) {
		sys_dlist_remove(&event->_node);
	}
}

static int k_poll_event_ready(struct k_poll_event *event)
{
	switch (event->type) {
		case K_POLL_TYPE_DATA_AVAILABLE:
			if (!sys_sflist_is_empty(&event->queue->data_q)) {
				event->state = K_POLL_STATE_DATA_AVAILABLE;
				return true;
			}
			break;
		case K_POLL_TYPE_SEM_AVAILABLE:
			if (k_sem_count_get(event->sem) > 0) {
				k_sem_reset(event->sem);
				event->state = K_POLL_STATE_SEM_AVAILABLE;
				return true;
			}
			break;
		case K_POLL_TYPE_SIGNAL:
			if (event->signal->signaled != 0) {
				event->signal->signaled = 0;
				event->state = K_POLL_STATE_SIGNALED;
				return true;
			}
			break;
	}

	return false;
}

static bool poll_event_pending(struct k_poll_event *events, int num_events)
{
	int i;

	for (i = 0; i < num_events; i++) {
		if (k_poll_event_ready(&events[i])) {
			return true;
		}
	}

	return false;
}

int k_poll(struct k_poll_event *events, int num_events, k_timeout_t timeout)
{
	poll_event_cb_t cb;
	int key, i, err = 0;

	key = irq_lock();

	if (poll_event_pending(events, num_events) ||
	    K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		goto end;
	}

	for (i = 0; i < num_events; i++) {
		poll_event_add(&events[i]);
	}

	cb.events     = events;
	cb.num_events = num_events;

	k_sem_init(&cb.sem, 0, 1);
	sys_dlist_append(&poll_list, &cb.next);
	irq_unlock(key);

	err = k_sem_take(&cb.sem, timeout);
	key = irq_lock();
	sys_dlist_remove(&cb.next);

	poll_event_pending(events, num_events);

	for (i = 0; i < num_events; i++) {
		poll_event_remove(&events[i]);
	}

end:
	irq_unlock(key);

	return err;
}
