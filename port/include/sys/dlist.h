/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Doubly-linked list implementation
 *
 * Doubly-linked list implementation using inline macros/functions.
 * This API is not thread safe, and thus if a list is used across threads,
 * calls to functions must be protected with synchronization primitives.
 *
 * The lists are expected to be initialized such that both the head and tail
 * pointers point to the list itself.  Initializing the lists in such a fashion
 * simplifies the adding and removing of nodes to/from the list.
 */

#ifndef ZEPHYR_INCLUDE_SYS_DLIST_H_
#define ZEPHYR_INCLUDE_SYS_DLIST_H_

#define SYS_DLIST_STATIC_INIT(ptr_to_list) { (NULL), (NULL) }

struct _dnode {
	union {
		struct _dnode *head; /* ptr to head of list (sys_dlist_t) */
		struct _dnode *next; /* ptr to next node    (sys_dnode_t) */
	};
	union {
		struct _dnode *tail; /* ptr to tail of list (sys_dlist_t) */
		struct _dnode *prev; /* ptr to previous node (sys_dnode_t) */
	};
};

typedef struct _dnode sys_dlist_t;
typedef struct _dnode sys_dnode_t;

#endif /* ZEPHYR_INCLUDE_SYS_DLIST_H_ */
