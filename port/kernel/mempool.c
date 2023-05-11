/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <sys/util.h>

void k_free(void *ptr)
{
	free(ptr);
}

void *k_aligned_alloc(size_t align, size_t size)
{
	return memalign(align, size);
}

void *k_malloc(size_t size)
{
	return memalign(8, size);
}
