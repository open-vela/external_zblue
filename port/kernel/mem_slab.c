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
#include <zephyr/init.h>
#include <zephyr/sys/check.h>

static struct k_spinlock lock;

/**
 * @brief Initialize kernel memory slab subsystem.
 *
 * Perform any initialization of memory slabs that wasn't done at build time.
 * Currently this just involves creating the list of free blocks for each slab.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p slab contains invalid configuration and/or values.
 */
static int create_free_list(struct k_mem_slab *slab)
{
	char *p;

    /* blocks must be word aligned */
	CHECKIF(((slab->info.block_size | (uintptr_t)slab->buffer) &
				(sizeof(void *) - 1)) != 0U) {
		return -EINVAL;
	}

	slab->free_list = NULL;
	p = slab->buffer + slab->info.block_size * (slab->info.num_blocks - 1);

	while (p >= slab->buffer) {
		*(char **)p = slab->free_list;
		slab->free_list = p;
		p -= slab->info.block_size;
	}
	return 0;
}

/**
 * @brief Complete initialization of statically defined memory slabs.
 *
 * Perform any initialization that wasn't done at build time.
 *
 * @return N/A
 */
static int init_mem_slab_obj_core_list(void)
{
	int rc = 0;

	STRUCT_SECTION_FOREACH(k_mem_slab, slab) {
		rc = create_free_list(slab);
		if (rc < 0) {
			goto out;
		}
	}

out:
	return rc;
}

SYS_INIT(init_mem_slab_obj_core_list, PRE_KERNEL_1,
	 CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);

int k_mem_slab_alloc(struct k_mem_slab *slab, void **mem, k_timeout_t timeout)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	int result;

	if (slab->free_list != NULL) {
		/* take a free block */
		*mem = slab->free_list;
		slab->free_list = *(char **)(slab->free_list);
		slab->info.num_used++;
		result = 0;
	} else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		/* don't wait for a free block to become available */
		*mem = NULL;
		result = -ENOMEM;
	} else {
		result = z_sched_wait(&lock, key, &slab->wait_q, timeout, NULL);
		if (result) {
			return result;
		}

		__ASSERT_NO_MSG(slab->free_list != NULL);

		key = k_spin_lock(&lock);

		*mem = slab->free_list;
		slab->free_list = *(char **)(slab->free_list);
		slab->info.num_used++;

		k_spin_unlock(&lock, key);
	}

	k_spin_unlock(&lock, key);

	return result;
}

void k_mem_slab_free(struct k_mem_slab *slab, void *mem)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	*(char **) mem = slab->free_list;
	slab->free_list = (char *) mem;
	slab->info.num_used--;

	(void)z_sched_wake(&slab->wait_q, 0, NULL);

	k_spin_unlock(&lock, key);
}
