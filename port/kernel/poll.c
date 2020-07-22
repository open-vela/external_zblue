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
#include <spinlock.h>

struct event_cb {
	dq_entry_t next;
	struct k_poll_event *events;
	int num_events;
	struct k_sem sem;
};

static dq_queue_t event_cb_list;

static void event_callback(struct k_poll_event *event, uint8_t event_type);

void k_poll_event_init(struct k_poll_event *event,
		u32_t type, int mode, void *obj)
{
	event->poller = NULL;
	event->type   = type;
	event->state  = K_POLL_STATE_NOT_READY;
	event->mode   = mode;
	event->unused = 0;
	event->obj    = obj;
}

static inline void set_event_state(struct k_poll_event *event, u32_t state)
{
	event->poller = NULL;
	event->state |= state;
}

static int _signal_poll_event(struct k_poll_event *event, u32_t state)
{
	set_event_state(event, state);
	if (event->type == K_POLL_TYPE_DATA_AVAILABLE) {
		set_event_state(event, state);
		event_callback(event, K_POLL_TYPE_FIFO_DATA_AVAILABLE);
	}

	return 0;
}

void _handle_obj_poll_events(dq_queue_t *events, u32_t state)
{
	struct k_poll_event *poll_event;

	poll_event = (struct k_poll_event *)dq_remfirst(events);
	if (poll_event)
		(void)_signal_poll_event(poll_event, state);
}

static inline int is_condition_met(struct k_poll_event *event, u32_t *state)
{
	switch (event->type) {
		case K_POLL_TYPE_SEM_AVAILABLE:
			if (k_sem_count_get(event->sem) > 0) {
				*state = K_POLL_STATE_SEM_AVAILABLE;
				return true;
			}
			break;
		case K_POLL_TYPE_DATA_AVAILABLE:
			if (!sq_empty(&event->queue->data_q)) {
				*state = K_POLL_STATE_FIFO_DATA_AVAILABLE;
				return true;
			}
			break;
		case K_POLL_TYPE_SIGNAL:
			if (event->signal->signaled != 0U) {
				*state = K_POLL_STATE_SIGNALED;
				return true;
			}
			break;
		case K_POLL_TYPE_IGNORE:
			break;
		default:
			break;
	}

	return false;
}

static inline void clear_event_registration(struct k_poll_event *event)
{
	event->poller = NULL;

	switch (event->type) {
		case K_POLL_TYPE_DATA_AVAILABLE:
			__ASSERT(event->queue, "invalid queue\n");
			dq_rem(&event->_node, &event->queue->poll_events);
			break;
		case K_POLL_TYPE_SIGNAL:
			__ASSERT(event->signal, "invalid poll signal\n");
			dq_rem(&event->_node, &event->signal->poll_events);
			break;
			__ASSERT(0, "invalid event type\n");
			break;
	}
}

static inline void clear_event_registrations(struct k_poll_event *events,
		int last_registered, unsigned int key)
{
	for (; last_registered >= 0; last_registered--) {
		clear_event_registration(&events[last_registered]);
		irq_unlock(key);
		key = irq_lock();
	}
}

static inline void add_event(dq_queue_t *events, struct k_poll_event *event,
		struct _poller *poller)
{
	dq_addlast(&event->_node, events);
}

static inline int register_event(struct k_poll_event *event,
		struct _poller *poller)
{
	switch (event->type) {
		case K_POLL_TYPE_DATA_AVAILABLE:
			__ASSERT(event->queue, "invalid queue\n");
			add_event(&event->queue->poll_events, event, poller);
			break;
		case K_POLL_TYPE_SIGNAL:
			__ASSERT(event->signal, "invalid poll signal\n");
			add_event(&event->signal->poll_events, event, poller);
			break;
		default:
			__ASSERT(0, "invalid event type\n");
			break;
	}

	event->poller = poller;

	return 0;
}

static bool polling_events(struct k_poll_event *events, int num_events,
		s32_t timeout, int *last_registered)
{
	bool polling = true;
	unsigned int key;
	u32_t state;
	int i;

	for (i = 0; i < num_events; i++) {
		key = irq_lock();
		if (is_condition_met(&events[i], &state)) {
			set_event_state(&events[i], state);
			polling = false;
		} else if (timeout != K_NO_WAIT && polling) {
			register_event(&events[i], NULL);
			++(*last_registered);
		}
		irq_unlock(key);
	}


	return polling;
}

static void event_callback(struct k_poll_event *event, uint8_t event_type)
{
	unsigned int key;
	FAR dq_entry_t *e;
	struct event_cb *cb;
	int i;

	key = irq_lock();
	for (e = dq_peek(&event_cb_list); e; e = dq_next(e)) {
		cb = (struct event_cb *)e;
		for (i = 0; i < cb->num_events; i++) {
			if (cb->events[i].type == event_type &&
					&cb->events[i] == event) {
				k_sem_give(&cb->sem);
				break;
			}
		}
	}

	irq_unlock(key);
}

int k_poll(struct k_poll_event *events, int num_events, s32_t timeout)
{
	int last_registered = -1;
	bool polling = true;
	struct event_cb cb;
	int key;

	cb.events = events;
	cb.num_events = num_events;

	k_sem_init(&cb.sem, 0, 1);

	dq_addlast(&cb.next, &event_cb_list);

	polling = polling_events(events, num_events, timeout, &last_registered);
	if (polling == false)
		goto exit;

	k_sem_take(&cb.sem, timeout);

	last_registered = -1;
	polling_events(events, num_events, K_NO_WAIT, &last_registered);
exit:
	dq_rem(&cb.next, &event_cb_list);
	k_sem_delete(&cb.sem);

	key = irq_lock();
	clear_event_registrations(events, last_registered, key);
	irq_unlock(key);
	return 0;
}

int k_poll_signal_raise(struct k_poll_signal *signal, int result)
{
	return 0;
}
