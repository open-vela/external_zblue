/****************************************************************************
 * apps/external/zblue/port/subsys/net/buf.c
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

#define LOG_MODULE_NAME net_buf
#define LOG_LEVEL CONFIG_NET_BUF_LOG_LEVEL

#include <logging/log.h>

#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/byteorder.h>

#include <net/buf.h>

extern struct net_buf_pool adv_buf_pool;
extern struct net_buf_pool *net_buf_pool_list[];
extern int net_buf_pool_list_size;

static uint8_t *fixed_data_alloc(struct net_buf *buf, size_t *size,
		k_timeout_t timeout)
{
	struct net_buf_pool *pool = net_buf_pool_get(buf->pool_id);
	const struct net_buf_pool_fixed *fixed = pool->alloc->alloc_data;

	*size = MIN(fixed->data_size, *size);

	return fixed->data_pool + fixed->data_size * net_buf_id(buf);
}

static void fixed_data_unref(struct net_buf *buf, uint8_t *data)
{
}

const struct net_buf_data_cb net_buf_fixed_cb = {
	.alloc = fixed_data_alloc,
	.unref = fixed_data_unref,
};

struct net_buf_pool *net_buf_pool_get(int id)
{
	return net_buf_pool_list[id];
}

static int pool_id(struct net_buf_pool *pool)
{
	int index;

	for (index = 0; index < net_buf_pool_list_size; index++) {
		if (net_buf_pool_list[index] == pool)
			return index;
	}

	return -1;
}

int net_buf_id(struct net_buf *buf)
{
	struct net_buf_pool *pool = net_buf_pool_get(buf->pool_id);

	return buf - pool->__bufs;
}

static inline struct net_buf *pool_get_uninit(struct net_buf_pool *pool,
		uint16_t uninit_count)
{
	struct net_buf *buf;

	buf = &pool->__bufs[pool->buf_count - uninit_count];

	buf->pool_id = pool_id(pool);

	return buf;
}

void net_buf_reset(struct net_buf *buf)
{
	net_buf_simple_reset(&buf->b);
}

static uint8_t *data_alloc(struct net_buf *buf, size_t *size, k_timeout_t timeout)
{
	struct net_buf_pool *pool = net_buf_pool_get(buf->pool_id);

	return pool->alloc->cb->alloc(buf, size, timeout);
}

static uint8_t *data_ref(struct net_buf *buf, uint8_t *data)
{
	uint8_t *ref_count;

	ref_count = data - 1;
	(*ref_count)++;

	return data;
}

static void data_unref(struct net_buf *buf, uint8_t *data)
{
	struct net_buf_pool *pool = net_buf_pool_get(buf->pool_id);

	if (buf->flags & NET_BUF_EXTERNAL_DATA) {
		return;
	}

	pool->alloc->cb->unref(buf, data);
}

struct net_buf *net_buf_alloc_len(struct net_buf_pool *pool, size_t size,
		k_timeout_t timeout)
{
	uint64_t end = z_timeout_end_calc(timeout);
	struct net_buf *buf;
	unsigned int key;

	__ASSERT_NO_MSG(pool);

	key = irq_lock();

	if (pool->uninit_count) {
		uint16_t uninit_count;

		if (pool->uninit_count < pool->buf_count) {
			buf = k_lifo_get(&pool->free, K_NO_WAIT);
			if (buf) {
				irq_unlock(key);
				goto success;
			}
		}

		uninit_count = pool->uninit_count--;
		irq_unlock(key);

		buf = pool_get_uninit(pool, uninit_count);
		goto success;
	}

	irq_unlock(key);

	buf = k_lifo_get(&pool->free, timeout);
	if (!buf) {
		return NULL;
	}

success:
	if (size) {
		if (!K_TIMEOUT_EQ(timeout, K_NO_WAIT) &&
				!K_TIMEOUT_EQ(timeout, K_FOREVER)) {
			int64_t remaining = end - z_tick_get();

			if (remaining <= 0) {
				timeout = K_NO_WAIT;
			} else {
				timeout = Z_TIMEOUT_TICKS(remaining);
			}
		}

		buf->__buf = data_alloc(buf, &size, timeout);
		if (!buf->__buf) {
			net_buf_destroy(buf);
			return NULL;
		}

	} else {
		buf->__buf = NULL;
	}

	buf->ref   = 1U;
	buf->flags = 0U;
	buf->frags = NULL;
	buf->size  = size;
	net_buf_reset(buf);

	return buf;
}

struct net_buf *net_buf_alloc_fixed(struct net_buf_pool *pool,
		k_timeout_t timeout)
{
	const struct net_buf_pool_fixed *fixed = pool->alloc->alloc_data;

	return net_buf_alloc_len(pool, fixed->data_size, timeout);
}

struct net_buf *net_buf_alloc_with_data(struct net_buf_pool *pool,
		void *data, size_t size,
		k_timeout_t timeout)
{
	struct net_buf *buf;

	buf = net_buf_alloc_len(pool, 0, timeout);
	if (!buf) {
		return NULL;
	}

	net_buf_simple_init_with_data(&buf->b, data, size);
	buf->flags = NET_BUF_EXTERNAL_DATA;

	return buf;
}

struct net_buf *net_buf_get(struct k_fifo *fifo, k_timeout_t timeout)
{
	struct net_buf *buf, *frag;

	buf = k_fifo_get(fifo, timeout);
	if (!buf)
		return NULL;

	for (frag = buf; (frag->flags & NET_BUF_FRAGS); frag = frag->frags) {
		frag->frags = k_fifo_get(fifo, K_NO_WAIT);
		frag->flags &= ~NET_BUF_FRAGS;
	}

	frag->frags = NULL;

	return buf;
}

void net_buf_simple_init_with_data(struct net_buf_simple *buf,
		void *data, size_t size)
{
	buf->__buf = data;
	buf->data  = data;
	buf->size  = size;
	buf->len   = size;
}

void net_buf_simple_reserve(struct net_buf_simple *buf, size_t reserve)
{
	buf->data = buf->__buf + reserve;
}

void net_buf_slist_put(sys_slist_t *list, struct net_buf *buf)
{
	struct net_buf *tail;
	unsigned int key;

	for (tail = buf; tail->frags; tail = tail->frags)
		tail->flags |= NET_BUF_FRAGS;

	key = irq_lock();
	sys_slist_append_list(list, &buf->node, &tail->node);
	irq_unlock(key);
}

struct net_buf *net_buf_slist_get(sys_slist_t *list)
{
	struct net_buf *buf, *frag;
	unsigned int key;

	key = irq_lock();
	buf = (void *)sys_slist_get(list);
	irq_unlock(key);

	if (!buf)
		return NULL;

	/* Get any fragments belonging to this buffer */
	for (frag = buf; (frag->flags & NET_BUF_FRAGS); frag = frag->frags) {
		key = irq_lock();
		frag->frags = (void *)sys_slist_get(list);
		irq_unlock(key);

		frag->flags &= ~NET_BUF_FRAGS;
	}

	frag->frags = NULL;

	return buf;
}

void net_buf_put(struct k_fifo *fifo, struct net_buf *buf)
{
	struct net_buf *tail;

	for (tail = buf; tail->frags; tail = tail->frags)
		tail->flags |= NET_BUF_FRAGS;

	k_fifo_put_list(fifo, buf, tail);
}

void net_buf_unref(struct net_buf *buf)
{

	while (buf) {
		struct net_buf *frags = buf->frags;
		struct net_buf_pool *pool;

		if (--buf->ref > 0)
			return;

		if (buf->__buf) {
			data_unref(buf, buf->__buf);
			buf->__buf = NULL;
		}

		buf->data = NULL;
		buf->frags = NULL;

		pool = net_buf_pool_get(buf->pool_id);

		if (pool->destroy) {
			pool->destroy(buf);
		} else {
			net_buf_destroy(buf);
		}

		buf = frags;
	}
}

struct net_buf *net_buf_ref(struct net_buf *buf)
{
#ifdef CONFIG_BT_MESH
	struct net_buf_pool *pool;
	unsigned int key;
#endif

#ifdef CONFIG_BT_MESH
	pool = net_buf_pool_get(buf->pool_id);

	if (pool == &adv_buf_pool) {
		key = irq_lock();
	}
#endif

	buf->ref++;

#ifdef CONFIG_BT_MESH
	if (pool == &adv_buf_pool) {
		irq_unlock(key);
	}
#endif

	return buf;
}

struct net_buf *net_buf_clone(struct net_buf *buf, k_timeout_t timeout)
{
	int64_t end = z_timeout_end_calc(timeout);
	struct net_buf_pool *pool;
	struct net_buf *clone;

	__ASSERT_NO_MSG(buf);

	pool = net_buf_pool_get(buf->pool_id);

	clone = net_buf_alloc_len(pool, 0, timeout);
	if (!clone) {
		return NULL;
	}

	if (pool->alloc->cb->ref && !(buf->flags & NET_BUF_EXTERNAL_DATA)) {
		clone->__buf = data_ref(buf, buf->__buf);
		clone->data = buf->data;
		clone->len = buf->len;
		clone->size = buf->size;
	} else {
		size_t size = buf->size;

		if (!K_TIMEOUT_EQ(timeout, K_NO_WAIT) &&
				!K_TIMEOUT_EQ(timeout, K_FOREVER)) {
			int64_t remaining = end - z_tick_get();

			if (remaining <= 0) {
				timeout = K_NO_WAIT;
			} else {
				timeout = Z_TIMEOUT_TICKS(remaining);
			}
		}

		clone->__buf = data_alloc(clone, &size, timeout);
		if (!clone->__buf || size < buf->size) {
			net_buf_destroy(clone);
			return NULL;
		}

		clone->size = size;
		clone->data = clone->__buf + net_buf_headroom(buf);
		net_buf_add_mem(clone, buf->data, buf->len);
	}

	return clone;
}

struct net_buf *net_buf_frag_last(struct net_buf *buf)
{
	while (buf->frags) {
		buf = buf->frags;
	}

	return buf;
}

void net_buf_frag_insert(struct net_buf *parent, struct net_buf *frag)
{
	if (parent->frags)
		net_buf_frag_last(frag)->frags = parent->frags;

	parent->frags = frag;
}

struct net_buf *net_buf_frag_add(struct net_buf *head, struct net_buf *frag)
{
	if (!head) {
		return net_buf_ref(frag);
	}

	net_buf_frag_insert(net_buf_frag_last(head), frag);

	return head;
}

struct net_buf *net_buf_frag_del(struct net_buf *parent, struct net_buf *frag)
{
	struct net_buf *next_frag;

	if (parent)
		parent->frags = frag->frags;

	next_frag = frag->frags;

	frag->frags = NULL;

	net_buf_unref(frag);

	return next_frag;
}

size_t net_buf_linearize(void *dst, size_t dst_len, struct net_buf *src,
		size_t offset, size_t len)
{
	struct net_buf *frag;
	size_t to_copy;
	size_t copied;

	len = MIN(len, dst_len);

	frag = src;

	while (frag && offset >= frag->len) {
		offset -= frag->len;
		frag = frag->frags;
	}

	copied = 0;
	while (frag && len > 0) {
		to_copy = MIN(len, frag->len - offset);
		memcpy((uint8_t *)dst + copied, frag->data + offset, to_copy);

		copied += to_copy;

		len -= to_copy;
		frag = frag->frags;

		offset = 0;
	}

	return copied;
}

size_t net_buf_append_bytes(struct net_buf *buf, size_t len,
		const void *value, k_timeout_t timeout,
		net_buf_allocator_cb allocate_cb, void *user_data)
{
	struct net_buf *frag = net_buf_frag_last(buf);
	size_t added_len = 0;
	const uint8_t *value8 = value;

	do {
		uint16_t count = MIN(len, net_buf_tailroom(frag));

		net_buf_add_mem(frag, value8, count);
		len -= count;
		added_len += count;
		value8 += count;

		if (len == 0) {
			return added_len;
		}

		frag = allocate_cb(timeout, user_data);
		if (!frag) {
			return added_len;
		}

		net_buf_frag_add(buf, frag);
	} while (1);

	return 0;
}

void net_buf_simple_clone(const struct net_buf_simple *original,
		struct net_buf_simple *clone)
{
	memcpy(clone, original, sizeof(struct net_buf_simple));
}

void *net_buf_simple_add(struct net_buf_simple *buf, size_t len)
{
	uint8_t *tail = net_buf_simple_tail(buf);

	buf->len += len;
	return tail;
}

void *net_buf_simple_add_mem(struct net_buf_simple *buf, const void *mem,
		size_t len)
{
	return memcpy(net_buf_simple_add(buf, len), mem, len);
}

uint8_t *net_buf_simple_add_u8(struct net_buf_simple *buf, uint8_t val)
{
	uint8_t *u8;

	u8 = net_buf_simple_add(buf, 1);
	*u8 = val;

	return u8;
}

void net_buf_simple_add_le16(struct net_buf_simple *buf, uint16_t val)
{
	sys_put_le16(val, net_buf_simple_add(buf, sizeof(val)));
}

void net_buf_simple_add_be16(struct net_buf_simple *buf, uint16_t val)
{
	sys_put_be16(val, net_buf_simple_add(buf, sizeof(val)));
}

void net_buf_simple_add_le24(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_le24(val, net_buf_simple_add(buf, 3));
}

void net_buf_simple_add_be24(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_be24(val, net_buf_simple_add(buf, 3));
}

void net_buf_simple_add_le32(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_le32(val, net_buf_simple_add(buf, sizeof(val)));
}

void net_buf_simple_add_be32(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_be32(val, net_buf_simple_add(buf, sizeof(val)));
}

void net_buf_simple_add_le48(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_le48(val, net_buf_simple_add(buf, 6));
}

void net_buf_simple_add_be48(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_be48(val, net_buf_simple_add(buf, 6));
}

void net_buf_simple_add_le64(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_le64(val, net_buf_simple_add(buf, sizeof(val)));
}

void net_buf_simple_add_be64(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_be64(val, net_buf_simple_add(buf, sizeof(val)));
}

void *net_buf_simple_push(struct net_buf_simple *buf, size_t len)
{
	buf->data -= len;
	buf->len += len;
	return buf->data;
}

void net_buf_simple_push_le16(struct net_buf_simple *buf, uint16_t val)
{
	sys_put_le16(val, net_buf_simple_push(buf, sizeof(val)));
}

void net_buf_simple_push_be16(struct net_buf_simple *buf, uint16_t val)
{
	sys_put_be16(val, net_buf_simple_push(buf, sizeof(val)));
}

void net_buf_simple_push_u8(struct net_buf_simple *buf, uint8_t val)
{
	uint8_t *data = net_buf_simple_push(buf, 1);

	*data = val;
}

void net_buf_simple_push_le24(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_le24(val, net_buf_simple_push(buf, 3));
}

void net_buf_simple_push_be24(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_be24(val, net_buf_simple_push(buf, 3));
}

void net_buf_simple_push_le32(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_le32(val, net_buf_simple_push(buf, sizeof(val)));
}

void net_buf_simple_push_be32(struct net_buf_simple *buf, uint32_t val)
{
	sys_put_be32(val, net_buf_simple_push(buf, sizeof(val)));
}

void net_buf_simple_push_le48(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_le48(val, net_buf_simple_push(buf, 6));
}

void net_buf_simple_push_be48(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_be48(val, net_buf_simple_push(buf, 6));
}

void net_buf_simple_push_le64(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_le64(val, net_buf_simple_push(buf, sizeof(val)));
}

void net_buf_simple_push_be64(struct net_buf_simple *buf, uint64_t val)
{
	sys_put_be64(val, net_buf_simple_push(buf, sizeof(val)));
}

void *net_buf_simple_pull(struct net_buf_simple *buf, size_t len)
{
	buf->len -= len;
	return buf->data += len;
}

void *net_buf_simple_pull_mem(struct net_buf_simple *buf, size_t len)
{
	void *data = buf->data;

	buf->len -= len;
	buf->data += len;

	return data;
}

uint8_t net_buf_simple_pull_u8(struct net_buf_simple *buf)
{
	uint8_t val;

	val = buf->data[0];
	net_buf_simple_pull(buf, 1);

	return val;
}

uint16_t net_buf_simple_pull_le16(struct net_buf_simple *buf)
{
	uint16_t val;

	val = UNALIGNED_GET((uint16_t *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_le16_to_cpu(val);
}

uint16_t net_buf_simple_pull_be16(struct net_buf_simple *buf)
{
	uint16_t val;

	val = UNALIGNED_GET((uint16_t *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_be16_to_cpu(val);
}

uint32_t net_buf_simple_pull_le24(struct net_buf_simple *buf)
{
	struct uint24 {
		uint32_t u24:24;
	} __packed val;

	val = UNALIGNED_GET((struct uint24 *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_le24_to_cpu(val.u24);
}

uint32_t net_buf_simple_pull_be24(struct net_buf_simple *buf)
{
	struct uint24 {
		uint32_t u24:24;
	} __packed val;

	val = UNALIGNED_GET((struct uint24 *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_be24_to_cpu(val.u24);
}

uint32_t net_buf_simple_pull_le32(struct net_buf_simple *buf)
{
	uint32_t val;

	val = UNALIGNED_GET((uint32_t *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_le32_to_cpu(val);
}

uint32_t net_buf_simple_pull_be32(struct net_buf_simple *buf)
{
	uint32_t val;

	val = UNALIGNED_GET((uint32_t *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_be32_to_cpu(val);
}

uint64_t net_buf_simple_pull_le48(struct net_buf_simple *buf)
{
	struct uint48 {
		uint64_t u48:48;
	} __packed val;

	val = UNALIGNED_GET((struct uint48 *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_le48_to_cpu(val.u48);
}

uint64_t net_buf_simple_pull_be48(struct net_buf_simple *buf)
{
	struct uint48 {
		uint64_t u48:48;
	} __packed val;

	val = UNALIGNED_GET((struct uint48 *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_be48_to_cpu(val.u48);
}

uint64_t net_buf_simple_pull_le64(struct net_buf_simple *buf)
{
	uint64_t val;

	val = UNALIGNED_GET((uint64_t *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_le64_to_cpu(val);
}

uint64_t net_buf_simple_pull_be64(struct net_buf_simple *buf)
{
	uint64_t val;

	val = UNALIGNED_GET((uint64_t *)buf->data);
	net_buf_simple_pull(buf, sizeof(val));

	return sys_be64_to_cpu(val);
}

size_t net_buf_simple_headroom(struct net_buf_simple *buf)
{
	return buf->data - buf->__buf;
}

size_t net_buf_simple_tailroom(struct net_buf_simple *buf)
{
	return buf->size - net_buf_simple_headroom(buf) - buf->len;
}
