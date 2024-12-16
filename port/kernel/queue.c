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

void k_queue_init(struct k_queue *queue)
{
	sys_sflist_init(&queue->data_q);
	queue->lock = (struct k_spinlock) {};
	sys_dlist_init(&queue->poll_events);
}

static void handle_poll_events(struct k_queue *queue, uint32_t state)
{
	z_handle_obj_poll_events(&queue->poll_events, state);
}

void k_queue_cancel_wait(struct k_queue *queue)
{
	handle_poll_events(queue, K_POLL_STATE_CANCELLED);
}

void k_queue_insert(struct k_queue *queue, void *prev, void *data)
{
	k_spinlock_key_t key = k_spin_lock(&queue->lock);
	sys_sflist_insert(&queue->data_q, prev, data);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
}

void k_queue_append(struct k_queue *queue, void *data)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&queue->lock);
	sys_sflist_append(&queue->data_q, data);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
}

void k_queue_prepend(struct k_queue *queue, void *data)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&queue->lock);
	sys_sflist_prepend(&queue->data_q, data);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
}

static void *k_queue_poll(struct k_queue *queue, k_timeout_t timeout)
{
	struct k_poll_event event;
	k_spinlock_key_t key;
	void *data = NULL;
	int err;

	k_poll_event_init(&event, K_POLL_TYPE_DATA_AVAILABLE,
			  K_POLL_MODE_NOTIFY_ONLY, queue);

	event.state = K_POLL_STATE_NOT_READY;
	err = k_poll(&event, 1, timeout);
	if (err)
		return NULL;

	key = k_spin_lock(&queue->lock);
	data = sys_sflist_get(&queue->data_q);
	k_spin_unlock(&queue->lock, key);

	return data;
}

void *k_queue_get(struct k_queue *queue, k_timeout_t timeout)
{
	k_spinlock_key_t key;
	void *data;

	if (sys_sflist_is_empty(&queue->data_q)) {
		if (K_TIMEOUT_EQ(timeout, K_NO_WAIT))
			return NULL;
	} else {
		key = k_spin_lock(&queue->lock);
		data = sys_sflist_get(&queue->data_q);
		k_spin_unlock(&queue->lock, key);
		return data;
	}

	return k_queue_poll(queue, timeout);
}

int k_queue_append_list(struct k_queue *queue, void *head, void *tail)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&queue->lock);
	sys_sflist_append_list(&queue->data_q, head, tail);
	k_spin_unlock(&queue->lock, key);

	handle_poll_events(queue, K_POLL_STATE_DATA_AVAILABLE);
	return 0;
}

int k_queue_is_empty(struct k_queue *queue)
{
	return sys_sflist_is_empty(&queue->data_q);
}

void *k_queue_peek_head(struct k_queue *queue)
{
	return sys_sflist_peek_head(&queue->data_q);
}