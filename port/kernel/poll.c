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

static struct k_spinlock lock = {};

static inline void add_event(sys_dlist_t *events, struct k_poll_event *event,
			     struct z_poller *poller)
{
	sys_dlist_append(events, &event->_node);
}

/* must be called with interrupts locked */
static inline bool is_condition_met(struct k_poll_event *event, uint32_t *state)
{
	switch (event->type) {
	case K_POLL_TYPE_SEM_AVAILABLE:
		if (k_sem_count_get(event->sem) > 0U) {
			*state = K_POLL_STATE_SEM_AVAILABLE;
			return true;
		}
		break;
	case K_POLL_TYPE_DATA_AVAILABLE:
		if (!k_queue_is_empty(event->queue)) {
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
	case K_POLL_TYPE_MSGQ_DATA_AVAILABLE:
		if (event->msgq->used_msgs > 0) {
			*state = K_POLL_STATE_MSGQ_DATA_AVAILABLE;
			return true;
		}
		break;
	case K_POLL_TYPE_IGNORE:
		break;
	default:
		__ASSERT(false, "invalid event type (0x%x)\n", event->type);
		break;
	}

	return false;
}

/* must be called with interrupts locked */
static inline void register_event(struct k_poll_event *event,
				 struct z_poller *poller)
{
	switch (event->type) {
	case K_POLL_TYPE_SEM_AVAILABLE:
		__ASSERT(event->sem != NULL, "invalid semaphore\n");
		add_event(&event->sem->poll_events, event, poller);
		break;
	case K_POLL_TYPE_DATA_AVAILABLE:
		__ASSERT(event->queue != NULL, "invalid queue\n");
		add_event(&event->queue->poll_events, event, poller);
		break;
	case K_POLL_TYPE_SIGNAL:
		__ASSERT(event->signal != NULL, "invalid poll signal\n");
		add_event(&event->signal->poll_events, event, poller);
		break;
	case K_POLL_TYPE_MSGQ_DATA_AVAILABLE:
		__ASSERT(event->msgq != NULL, "invalid message queue\n");
		add_event(&event->msgq->poll_events, event, poller);
		break;
	case K_POLL_TYPE_IGNORE:
		/* nothing to do */
		break;
	default:
		__ASSERT(false, "invalid event type\n");
		break;
	}

	event->poller = poller;
}

/* must be called with interrupts locked */
static inline void clear_event_registration(struct k_poll_event *event)
{
	bool remove_event = false;

	event->poller = NULL;

	switch (event->type) {
	case K_POLL_TYPE_SEM_AVAILABLE:
		__ASSERT(event->sem != NULL, "invalid semaphore\n");
		remove_event = true;
		break;
	case K_POLL_TYPE_DATA_AVAILABLE:
		__ASSERT(event->queue != NULL, "invalid queue\n");
		remove_event = true;
		break;
	case K_POLL_TYPE_SIGNAL:
		__ASSERT(event->signal != NULL, "invalid poll signal\n");
		remove_event = true;
		break;
	case K_POLL_TYPE_MSGQ_DATA_AVAILABLE:
		__ASSERT(event->msgq != NULL, "invalid message queue\n");
		remove_event = true;
		break;
	case K_POLL_TYPE_IGNORE:
		/* nothing to do */
		break;
	default:
		__ASSERT(false, "invalid event type\n");
		break;
	}
	if (remove_event && sys_dnode_is_linked(&event->_node)) {
		sys_dlist_remove(&event->_node);
	}
}

/* must be called with interrupts locked */
static inline void clear_event_registrations(struct k_poll_event *events,
					      int num_events,
					      k_spinlock_key_t key)
{
	while (num_events--) {
		clear_event_registration(&events[num_events]);
		k_spin_unlock(&lock, key);
		key = k_spin_lock(&lock);
	}
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
	} else if (state == K_POLL_STATE_SEM_AVAILABLE &&
           event->type == K_POLL_TYPE_SEM_AVAILABLE) {
        return true;
    } else {
		return false;
	}
}

static inline void set_event_ready(struct k_poll_event *event, uint32_t state)
{
	event->poller = NULL;
	event->state |= state;

    switch (event->type) {
		case K_POLL_TYPE_DATA_AVAILABLE:
			break;
		case K_POLL_TYPE_SEM_AVAILABLE:
			if (k_sem_count_get(event->sem) > 0) {
				k_sem_reset(event->sem);
			}
			break;
		case K_POLL_TYPE_SIGNAL:
			if (event->signal->signaled != 0) {
				event->signal->signaled = 0;
			}
			break;
	}
}

static inline int register_events(struct k_poll_event *events,
				  int num_events,
				  struct z_poller *poller,
				  bool just_check)
{
	int events_registered = 0;

	for (int ii = 0; ii < num_events; ii++) {
		k_spinlock_key_t key;
		uint32_t state;

		key = k_spin_lock(&lock);
		if (is_condition_met(&events[ii], &state)) {
			set_event_ready(&events[ii], state);
			poller->is_polling = false;
		} else if (!just_check && poller->is_polling) {
			register_event(&events[ii], poller);
			events_registered += 1;
		} else {
			/* Event is not one of those identified in is_condition_met()
			 * catching non-polling events, or is marked for just check,
			 * or not marked for polling. No action needed.
			 */
			;
		}
		k_spin_unlock(&lock, key);
	}

	return events_registered;
}

static int signal_poller(struct k_poll_event *event, uint32_t state)
{
	struct z_poller *poller = event->poller;

    if (state == K_POLL_STATE_CANCELLED ||
        event_match(event, state)) {
        set_event_ready(event, state);
        k_sem_give(&poller->sem);
    }

    return 0;
}

void k_poll_event_init(struct k_poll_event *event, uint32_t type,
		       int mode, void *obj)
{
    event->type = type;
    event->state = K_POLL_STATE_NOT_READY;
    event->obj = obj;
}

int k_poll(struct k_poll_event *events, int num_events,
		  k_timeout_t timeout)
{
	int events_registered;
	k_spinlock_key_t key;
	struct z_poller poller;

    k_sem_init(&poller.sem, 0, 1);
	poller.is_polling = true;

	__ASSERT(events != NULL, "NULL events\n");
	__ASSERT(num_events >= 0, "<0 events\n");

	events_registered = register_events(events, num_events, &poller,
					    K_TIMEOUT_EQ(timeout, K_NO_WAIT));

	key = k_spin_lock(&lock);

	/*
	 * If we're not polling anymore, it means that at least one event
	 * condition is met, either when looping through the events here or
	 * because one of the events registered has had its state changed.
	 */
	if (!poller.is_polling) {
		clear_event_registrations(events, events_registered, key);
		k_spin_unlock(&lock, key);

		return 0;
	}

	poller.is_polling = false;

	if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		k_spin_unlock(&lock, key);

		return -EAGAIN;
	}

	int ret = k_sem_take(&poller.sem, timeout);

	/*
	 * Clear all event registrations. If events happen while we're in this
	 * loop, and we already had one that triggered, that's OK: they will
	 * end up in the list of events that are ready; if we timed out, and
	 * events happen while we're in this loop, that is OK as well since
	 * we've already know the return code (-EAGAIN), and even if they are
	 * added to the list of events that occurred, the user has to check the
	 * return code first, which invalidates the whole list of event states.
	 */
	key = k_spin_lock(&lock);
	clear_event_registrations(events, events_registered, key);
	k_spin_unlock(&lock, key);

	return ret;
}

/* must be called with interrupts locked */
static int signal_poll_event(struct k_poll_event *event, uint32_t state)
{
	struct z_poller *poller = event->poller;
	int retcode = 0;

	if (poller != NULL) {
		retcode = signal_poller(event, state);
		poller->is_polling = false;

		if (retcode < 0) {
			return retcode;
		}
	}

	return retcode;
}

void z_handle_obj_poll_events(sys_dlist_t *events, uint32_t state)
{
	struct k_poll_event *poll_event;
	k_spinlock_key_t key = k_spin_lock(&lock);

	poll_event = (struct k_poll_event *)sys_dlist_get(events);
	if (poll_event != NULL) {
		(void) signal_poll_event(poll_event, state);
	}

	k_spin_unlock(&lock, key);
}

int k_poll_signal_raise(struct k_poll_signal *sig, int result)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	struct k_poll_event *poll_event;

	sig->result = result;
	sig->signaled = 1U;

	poll_event = (struct k_poll_event *)sys_dlist_get(&sig->poll_events);
	if (poll_event == NULL) {
		k_spin_unlock(&lock, key);

		return 0;
	}

	int rc = signal_poll_event(poll_event, K_POLL_STATE_SIGNALED);

	k_spin_unlock(&lock, key);
	return rc;
}