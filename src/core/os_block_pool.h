/**
 * @file    os_block_pool.h
 * @brief   Fixed-size block allocator with an in-place free list (kernel private).
 *
 * Free blocks are chained through their own memory: the first bytes of a free
 * block hold the pointer to the next free block, so the allocator needs no
 * bookkeeping memory and both alloc and free are O(1).
 *
 * Used for every kernel object pool (TCBs, queues, memory pools) and as the
 * engine of the public os_mempool API.
 */
#ifndef OS_BLOCK_POOL_H
#define OS_BLOCK_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "os_types.h"

/** Smallest block able to hold a free-list node (next pointer + marker). */
#define OS_BLOCK_POOL_MIN_BLOCK_SIZE (2U * sizeof(void *))

typedef struct {
    uint8_t  *base;           /**< First block. */
    size_t    block_size;
    size_t    block_count;
    void     *free_head;      /**< First free block, NULL when exhausted. */
    size_t    free_count;
    size_t    min_free_count; /**< Low watermark of free_count since init. */
    uintptr_t marker;         /**< Tag stored in every free block, per pool. */
} os_block_pool_t;

/**
 * Build the free list over @p storage.
 * @p block_size must be at least OS_BLOCK_POOL_MIN_BLOCK_SIZE and a multiple
 * of the pointer alignment; @p storage must be suitably aligned.
 */
void os_block_pool_init(os_block_pool_t *pool, void *storage, size_t block_size,
                        size_t block_count);

/** @return A block in O(1), or NULL if the pool is exhausted. */
void *os_block_pool_alloc(os_block_pool_t *pool);

/**
 * @brief  Return a block to the pool in O(1).
 * @return OS_ERR_INVALID_ARG if @p block does not start a block of this pool,
 *         OS_ERR_INVALID_STATE if it is already free (double free).
 */
os_status_t os_block_pool_free(os_block_pool_t *pool, void *block);

/** @return true if @p block is the start address of one of the pool's blocks. */
bool os_block_pool_owns(const os_block_pool_t *pool, const void *block);

#endif /* OS_BLOCK_POOL_H */
