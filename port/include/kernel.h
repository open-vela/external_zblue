/*
 * Copyright (c) 2016, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief Public kernel APIs.
 */

#ifndef ZEPHYR_INCLUDE_KERNEL_H_
#define ZEPHYR_INCLUDE_KERNEL_H_

#include <kernel_includes.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K_PRIO_COOP(x) (CONFIG_NUM_COOP_PRIORITIES + (x))
#define K_PRIO_PREEMPT(x) (CONFIG_NUM_COOP_PRIORITIES + (x))

#ifdef CONFIG_POLL
#define _POLL_EVENT_OBJ_INIT(obj) \
	.poll_events = SYS_DLIST_STATIC_INIT(&obj.poll_events),
#define _POLL_EVENT dq_queue_t poll_events
#else
#define _POLL_EVENT_OBJ_INIT(obj)
#define _POLL_EVENT
#endif

struct k_thread;
struct k_mutex;
struct k_sem;
struct k_queue;
struct k_fifo;
struct k_lifo;
struct k_mem_slab;
struct k_poll_event;
struct k_poll_signal;

struct k_thread {
	pid_t pid;
};

typedef struct k_thread _thread_t;
typedef struct k_thread *k_tid_t;

__syscall         k_tid_t k_thread_create(struct k_thread *new_thread,
		k_thread_stack_t *stack, size_t stack_size,
		k_thread_entry_t entry, void *p1, void *p2, void *p3,
		int prio, uint32_t options, k_timeout_t delay);
__syscall int     k_thread_join(struct k_thread *thread, k_timeout_t timeout);
__syscall int32_t   k_sleep(k_timeout_t timeout);
__syscall int32_t   k_usleep(int32_t us);
__syscall void    k_yield(void);
__syscall k_tid_t k_current_get(void);
__syscall int     k_thread_priority_get(k_tid_t thread);
__syscall void    k_thread_priority_set(k_tid_t thread, int prio);
__syscall int     k_thread_name_set(k_tid_t thread_id, const char *value);
const char       *k_thread_name_get(k_tid_t thread_id);
__syscall int     k_thread_name_copy(k_tid_t thread_id, char *buf, size_t size);
const char       *k_thread_state_str(k_tid_t thread_id);

static inline int32_t k_msleep(int32_t ms)
{
	return k_sleep(Z_TIMEOUT_MS(ms));
}

static inline uint32_t k_cycle_get_32(void)
{
	return z_tick_get();
}

static inline uint64_t k_cyc_to_ns_floor64(uint64_t t)
{
	return t;
}

#define K_NO_WAIT     Z_TIMEOUT_NO_WAIT
#define K_NSEC(t)     Z_TIMEOUT_NS(t)
#define K_USEC(t)     Z_TIMEOUT_US(t)
#define K_CYC(t)      Z_TIMEOUT_CYC(t)
#define K_TICKS(t)    Z_TIMEOUT_TICKS(t)
#define K_MSEC(ms)    Z_TIMEOUT_MS(ms)
#define K_SECONDS(s)  K_MSEC((s) * MSEC_PER_SEC)
#define K_MINUTES(m)  K_SECONDS((m) * 60)
#define K_HOURS(h)    K_MINUTES((h) * 60)
#define K_FOREVER     Z_FOREVER

__syscall int64_t k_uptime_ticks(void);

static inline int64_t k_uptime_get(void)
{
	return k_ticks_to_ms_floor64(k_uptime_ticks());
}

static inline uint32_t k_uptime_get_32(void)
{
	return (uint32_t)k_uptime_get();
}

struct k_queue {
	sys_slist_t data_q;
	struct k_spinlock lock;
	union {
		_POLL_EVENT;
	};
};

#define Z_QUEUE_INITIALIZER(obj) \
	{ \
		.data_q = SYS_SLIST_STATIC_INIT(&obj.data_q), \
		.lock = { }, \
		{ \
			_POLL_EVENT_OBJ_INIT(obj) \
		}, \
	}

#define K_QUEUE_INITIALIZER __DEPRECATED_MACRO Z_QUEUE_INITIALIZER

__syscall void  k_queue_init(struct k_queue *queue);
__syscall void  k_queue_cancel_wait(struct k_queue *queue);
extern void     k_queue_append(struct k_queue *queue, void *data);
__syscall int32_t k_queue_alloc_append(struct k_queue *queue, void *data);
extern void     k_queue_prepend(struct k_queue *queue, void *data);
__syscall int32_t k_queue_alloc_prepend(struct k_queue *queue, void *data);
extern void     k_queue_insert(struct k_queue *queue, void *prev, void *data);
extern int      k_queue_append_list(struct k_queue *queue, void *head, void *tail);
extern int      k_queue_merge_slist(struct k_queue *queue, sys_slist_t *list);
__syscall void *k_queue_get(struct k_queue *queue, k_timeout_t timeout);
__syscall int   k_queue_is_empty(struct k_queue *queue);
__syscall void *k_queue_peek_head(struct k_queue *queue);
__syscall void *k_queue_peek_tail(struct k_queue *queue);

#define K_QUEUE_DEFINE(name) \
	Z_STRUCT_SECTION_ITERABLE(k_queue, name) = \
	Z_QUEUE_INITIALIZER(name)

	/** @} */

struct k_fifo {
	struct k_queue _queue;
};

/**
 * @cond INTERNAL_HIDDEN
 */
#define Z_FIFO_INITIALIZER(obj) \
{ \
	._queue = Z_QUEUE_INITIALIZER(obj._queue) \
}

#define K_FIFO_INITIALIZER __DEPRECATED_MACRO Z_FIFO_INITIALIZER

#define k_fifo_init(fifo) \
	k_queue_init(&(fifo)->_queue)

#define k_fifo_cancel_wait(fifo) \
	k_queue_cancel_wait(&(fifo)->_queue)

#define k_fifo_put(fifo, data) \
	k_queue_append(&(fifo)->_queue, data)

#define k_fifo_alloc_put(fifo, data) \
	k_queue_alloc_append(&(fifo)->_queue, data)

#define k_fifo_put_list(fifo, head, tail) \
	k_queue_append_list(&(fifo)->_queue, head, tail)

#define k_fifo_put_slist(fifo, list) \
	k_queue_merge_slist(&(fifo)->_queue, list)

#define k_fifo_get(fifo, timeout) \
	k_queue_get(&(fifo)->_queue, timeout)

#define k_fifo_is_empty(fifo) \
	k_queue_is_empty(&(fifo)->_queue)

#define k_fifo_peek_head(fifo) \
	k_queue_peek_head(&(fifo)->_queue)

#define k_fifo_peek_tail(fifo) \
	k_queue_peek_tail(&(fifo)->_queue)

#define K_FIFO_DEFINE(name) \
	Z_STRUCT_SECTION_ITERABLE(k_fifo, name) = \
Z_FIFO_INITIALIZER(name)

struct k_lifo {
	struct k_queue _queue;
};

#define Z_LIFO_INITIALIZER(obj) \
{ \
	._queue = Z_QUEUE_INITIALIZER(obj._queue) \
}

#define K_LIFO_INITIALIZER __DEPRECATED_MACRO Z_LIFO_INITIALIZER

#define k_lifo_init(lifo) \
	k_queue_init(&(lifo)->_queue)

#define k_lifo_put(lifo, data) \
	k_queue_prepend(&(lifo)->_queue, data)

#define k_lifo_alloc_put(lifo, data) \
	k_queue_alloc_prepend(&(lifo)->_queue, data)

#define k_lifo_get(lifo, timeout) \
	k_queue_get(&(lifo)->_queue, timeout)

#define K_LIFO_DEFINE(name) \
	Z_STRUCT_SECTION_ITERABLE(k_lifo, name) = \
Z_LIFO_INITIALIZER(name)

struct k_work;

typedef int (*_poller_cb_t)(struct k_poll_event *event, uint32_t state);
struct _poller {
	volatile bool is_polling;
	struct k_thread *thread;
	_poller_cb_t cb;
};

typedef void (*k_work_handler_t)(struct k_work *work);

struct k_work_q {
	struct k_thread thread;
};

enum {
	K_WORK_STATE_PENDING,
};

struct k_work {
	struct work_s nwork;
	k_work_handler_t handler;
	void *priv;
};

struct k_delayed_work {
	struct k_work work;
};

extern struct k_work_q k_sys_work_q;

#define Z_WORK_INITIALIZER(work_handler) \
{ \
	.handler = work_handler, \
}

#define K_WORK_INITIALIZER __DEPRECATED_MACRO Z_WORK_INITIALIZER

#define K_WORK_DEFINE(work, work_handler) \
	struct k_work work = Z_WORK_INITIALIZER(work_handler)

void k_work_init(struct k_work *work, k_work_handler_t handler);
void k_work_submit_to_queue(struct k_work_q *work_q, struct k_work *work);
void k_delayed_work_init(struct k_delayed_work *work, k_work_handler_t handler);
int k_delayed_work_submit_to_queue(struct k_work_q *work_q, struct k_delayed_work *work, k_timeout_t delay);
int k_delayed_work_cancel(struct k_delayed_work *work);
static inline void k_work_submit(struct k_work *work)
{
	k_work_submit_to_queue(&k_sys_work_q, work);
}
static inline int k_delayed_work_submit(struct k_delayed_work *work,
		k_timeout_t delay)
{
	return k_delayed_work_submit_to_queue(&k_sys_work_q, work, delay);
}

int32_t k_delayed_work_remaining_get(struct k_delayed_work *work);

struct k_mutex {
	pthread_mutex_t mutex;
};

#define Z_MUTEX_INITIALIZER(obj) \
{ \
	.mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, \
}

#define K_MUTEX_INITIALIZER __DEPRECATED_MACRO Z_MUTEX_INITIALIZER

#define K_MUTEX_DEFINE(name) \
	Z_STRUCT_SECTION_ITERABLE(k_mutex, name) = \
Z_MUTEX_INITIALIZER(name)

__syscall int k_mutex_init(struct k_mutex *mutex);
__syscall int k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout);
__syscall int k_mutex_unlock(struct k_mutex *mutex);

struct k_sem {
	sem_t sem;
	uint32_t limit;
	dq_queue_t poll_events;
};

#define Z_SEM_INITIALIZER(obj, initial_count, count_limit) \
{ \
	.sem = SEM_INITIALIZER(initial_count), \
	.limit = count_limit, \
}

#define K_SEM_INITIALIZER __DEPRECATED_MACRO Z_SEM_INITIALIZER

int k_sem_init(struct k_sem *sem, unsigned int initial_count, unsigned int limit);
int k_sem_take(struct k_sem *sem, k_timeout_t timeout);
void k_sem_give(struct k_sem *sem);
void k_sem_reset(struct k_sem *sem);
unsigned int k_sem_count_get(struct k_sem *sem);

#define K_SEM_DEFINE(name, initial_count, count_limit) \
	Z_STRUCT_SECTION_ITERABLE(k_sem, name) = \
Z_SEM_INITIALIZER(name, initial_count, count_limit); \
BUILD_ASSERT(((count_limit) != 0) && \
		((initial_count) <= (count_limit)));

struct k_mem_slab {
	uint32_t num_blocks;
	size_t block_size;
	char *buffer;
	char *free_list;
	uint32_t num_used;
};

#define Z_MEM_SLAB_INITIALIZER(obj, slab_buffer, slab_block_size, \
		slab_num_blocks) \
{ \
	.num_blocks = slab_num_blocks, \
	.block_size = slab_block_size, \
	.buffer = NULL, \
	.free_list = NULL, \
	.num_used = 0, \
}

#define K_MEM_SLAB_INITIALIZER __DEPRECATED_MACRO Z_MEM_SLAB_INITIALIZER

#define K_MEM_SLAB_DEFINE(name, slab_block_size, slab_num_blocks, slab_align) \
	char __noinit __aligned(WB_UP(slab_align)) \
_k_mem_slab_buf_##name[(slab_num_blocks) * WB_UP(slab_block_size)]; \
Z_STRUCT_SECTION_ITERABLE(k_mem_slab, name) = \
Z_MEM_SLAB_INITIALIZER(name, _k_mem_slab_buf_##name, \
		WB_UP(slab_block_size), slab_num_blocks)

extern int k_mem_slab_init(struct k_mem_slab *slab, void *buffer,
		size_t block_size, uint32_t num_blocks);
extern int k_mem_slab_alloc(struct k_mem_slab *slab, void **mem,
		k_timeout_t timeout);
extern void k_mem_slab_free(struct k_mem_slab *slab, void **mem);

static inline uint32_t k_mem_slab_num_free_get(struct k_mem_slab *slab)
{
	return slab->num_blocks - slab->num_used;
}

#define K_HEAP_DEFINE(name, bytes)				\
	char __aligned(sizeof(void *)) kheap_##name[bytes];	\
Z_STRUCT_SECTION_ITERABLE(k_heap, name) = {		\
	.heap = {					\
		.init_mem = kheap_##name,		\
		.init_bytes = (bytes),			\
	},						\
}

#define K_MEM_POOL_DEFINE(name, minsz, maxsz, nmax, align) \
	Z_MEM_POOL_DEFINE(name, minsz, maxsz, nmax, align)

#ifdef CONFIG_POLL
#define _INIT_OBJ_POLL_EVENT(obj) do { (obj)->poll_event = NULL; } while (false)
#else
#define _INIT_OBJ_POLL_EVENT(obj) do { } while (false)
#endif

/* private - types bit positions */
enum _poll_types_bits {
	/* can be used to ignore an event */
	_POLL_TYPE_IGNORE,

	/* to be signaled by k_poll_signal_raise() */
	_POLL_TYPE_SIGNAL,

	/* semaphore availability */
	_POLL_TYPE_SEM_AVAILABLE,

	/* queue/fifo/lifo data availability */
	_POLL_TYPE_DATA_AVAILABLE,

	_POLL_NUM_TYPES
};

#define Z_POLL_TYPE_BIT(type) (1 << ((type) - 1))

/* private - states bit positions */
enum _poll_states_bits {
	/* default state when creating event */
	_POLL_STATE_NOT_READY,

	/* signaled by k_poll_signal_raise() */
	_POLL_STATE_SIGNALED,

	/* semaphore is available */
	_POLL_STATE_SEM_AVAILABLE,

	/* data is available to read on queue/fifo/lifo */
	_POLL_STATE_DATA_AVAILABLE,

	/* queue/fifo/lifo wait was cancelled */
	_POLL_STATE_CANCELLED,

	_POLL_NUM_STATES
};

#define Z_POLL_STATE_BIT(state) (1 << ((state) - 1))

#define _POLL_EVENT_NUM_UNUSED_BITS \
	(32 - (0 \
				 + 8 /* tag */ \
				 + _POLL_NUM_TYPES \
				 + _POLL_NUM_STATES \
				 + 1 /* modes */ \
				))

/* end of polling API - PRIVATE */


/**
 * @defgroup poll_apis Async polling APIs
 * @ingroup kernel_apis
 * @{
 */

/* Public polling API */

/* public - values for k_poll_event.type bitfield */
#define K_POLL_TYPE_IGNORE 0
#define K_POLL_TYPE_SIGNAL Z_POLL_TYPE_BIT(_POLL_TYPE_SIGNAL)
#define K_POLL_TYPE_SEM_AVAILABLE Z_POLL_TYPE_BIT(_POLL_TYPE_SEM_AVAILABLE)
#define K_POLL_TYPE_DATA_AVAILABLE Z_POLL_TYPE_BIT(_POLL_TYPE_DATA_AVAILABLE)
#define K_POLL_TYPE_FIFO_DATA_AVAILABLE K_POLL_TYPE_DATA_AVAILABLE

/* public - polling modes */
enum k_poll_modes {
	/* polling thread does not take ownership of objects when available */
	K_POLL_MODE_NOTIFY_ONLY = 0,

	K_POLL_NUM_MODES
};

/* public - values for k_poll_event.state bitfield */
#define K_POLL_STATE_NOT_READY 0
#define K_POLL_STATE_SIGNALED Z_POLL_STATE_BIT(_POLL_STATE_SIGNALED)
#define K_POLL_STATE_SEM_AVAILABLE Z_POLL_STATE_BIT(_POLL_STATE_SEM_AVAILABLE)
#define K_POLL_STATE_DATA_AVAILABLE Z_POLL_STATE_BIT(_POLL_STATE_DATA_AVAILABLE)
#define K_POLL_STATE_FIFO_DATA_AVAILABLE K_POLL_STATE_DATA_AVAILABLE
#define K_POLL_STATE_CANCELLED Z_POLL_STATE_BIT(_POLL_STATE_CANCELLED)

/* public - poll signal object */
struct k_poll_signal {
	/** PRIVATE - DO NOT TOUCH */
	dq_queue_t poll_events;

	/**
	 * 1 if the event has been signaled, 0 otherwise. Stays set to 1 until
	 * user resets it to 0.
	 */
	unsigned int signaled;

	/** custom result value passed to k_poll_signal_raise() if needed */
	int result;
};

#define K_POLL_SIGNAL_INITIALIZER(obj) \
{ \
	.poll_events = SYS_DLIST_STATIC_INIT(&obj.poll_events), \
	.signaled = 0, \
	.result = 0, \
}
/**
 * @brief Poll Event
 *
 */
struct k_poll_event {
	/** PRIVATE - DO NOT TOUCH */
	dq_entry_t _node;

	/** PRIVATE - DO NOT TOUCH */
	struct _poller *poller;

	/** optional user-specified tag, opaque, untouched by the API */
	uint32_t tag:8;

	/** bitfield of event types (bitwise-ORed K_POLL_TYPE_xxx values) */
	uint32_t type:_POLL_NUM_TYPES;

	/** bitfield of event states (bitwise-ORed K_POLL_STATE_xxx values) */
	uint32_t state:_POLL_NUM_STATES;

	/** mode of operation, from enum k_poll_modes */
	uint32_t mode:1;

	/** unused bits in 32-bit word */
	uint32_t unused:_POLL_EVENT_NUM_UNUSED_BITS;

	/** per-type data */
	union {
		void *obj;
		struct k_poll_signal *signal;
		struct k_sem *sem;
		struct k_fifo *fifo;
		struct k_queue *queue;
	};
};

#define K_POLL_EVENT_INITIALIZER(event_type, event_mode, event_obj) \
{ \
	.poller = NULL, \
	.type = event_type, \
	.state = K_POLL_STATE_NOT_READY, \
	.mode = event_mode, \
	.unused = 0, \
	.obj = event_obj, \
}

#define K_POLL_EVENT_STATIC_INITIALIZER(event_type, event_mode, event_obj, \
		event_tag) \
{ \
	.tag = event_tag, \
	.type = event_type, \
	.state = K_POLL_STATE_NOT_READY, \
	.mode = event_mode, \
	.unused = 0, \
	.obj = event_obj, \
}

extern void    k_poll_event_init(struct k_poll_event *event, uint32_t type, int mode, void *obj);
__syscall int  k_poll(struct k_poll_event *events, int num_events, k_timeout_t timeout);
__syscall void k_poll_signal_init(struct k_poll_signal *signal);
__syscall void k_poll_signal_reset(struct k_poll_signal *signal);
__syscall void k_poll_signal_check(struct k_poll_signal *signal, unsigned int *signaled, int *result);
__syscall int  k_poll_signal_raise(struct k_poll_signal *signal, int result);

#define k_oops()  assert(false)
#define k_panic() assert(false)

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_KERNEL_H_ */
