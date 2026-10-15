/**
 * @file    os_block_pool.c
 * @brief   Fixed-size block allocator with an in-place free list.
 *
 * Double-free detection without extra memory: every free block carries a
 * per-pool marker next to its link. Freeing a block that does not carry the
 * marker is accepted in O(1). Only if the marker is present (a real double
 * free, or user data that happens to match) is the free list walked to tell
 * the two cases apart.
 *
 * The free-list node is read and written with memcpy() so the allocator never
 * accesses the caller's objects through an incompatible type (strict aliasing).
 */
#include "os_block_pool.h"

#include <stdalign.h>
#include <string.h>

#include "os_config.h"

#define BLOCK_POOL_MARKER_SEED ((uintptr_t)0xF4EEB10CUL)

typedef struct {
    void     *next;
    uintptr_t marker;
} block_free_node_t;

_Static_assert(sizeof(block_free_node_t) <= OS_BLOCK_POOL_MIN_BLOCK_SIZE,
               "free-list node must fit in the minimum block");

static block_free_node_t block_read_node(const void *block)
{
    block_free_node_t node;

    memcpy(&node, block, sizeof(node));
    return node;
}

static void block_write_node(void *block, void *next, uintptr_t marker)
{
    const block_free_node_t node = { next, marker };

    memcpy(block, &node, sizeof(node));
}

static bool block_is_free(const os_block_pool_t *pool, const void *block)
{
    if (block_read_node(block).marker != pool->marker) {
        return false; /* Fast path: allocated blocks normally lack the marker. */
    }

    for (const void *it = pool->free_head; it != NULL; it = block_read_node(it).next) {
        if (it == block) {
            return true;
        }
    }
    return false;
}

void os_block_pool_init(os_block_pool_t *pool, void *storage, size_t block_size,
                        size_t block_count)
{
    OS_ASSERT((pool != NULL) && (storage != NULL) && (block_count > 0U));
    OS_ASSERT(block_size >= OS_BLOCK_POOL_MIN_BLOCK_SIZE);
    OS_ASSERT((block_size % alignof(block_free_node_t)) == 0U);
    OS_ASSERT(((uintptr_t)storage % alignof(block_free_node_t)) == 0U);

    pool->base           = (uint8_t *)storage;
    pool->block_size     = block_size;
    pool->block_count    = block_count;
    pool->free_head      = NULL;
    pool->free_count     = block_count;
    pool->min_free_count = block_count;
    pool->marker         = (uintptr_t)pool ^ BLOCK_POOL_MARKER_SEED;

    /* Push in reverse so blocks are handed out in ascending address order. */
    for (size_t i = block_count; i > 0U; --i) {
        void *block = pool->base + ((i - 1U) * block_size);

        block_write_node(block, pool->free_head, pool->marker);
        pool->free_head = block;
    }
}

void *os_block_pool_alloc(os_block_pool_t *pool)
{
    void *block = pool->free_head;

    if (block == NULL) {
        return NULL;
    }

    pool->free_head = block_read_node(block).next;
    pool->free_count--;
    if (pool->free_count < pool->min_free_count) {
        pool->min_free_count = pool->free_count;
    }

    /* Erase the marker so freeing this block later takes the fast path. */
    memset(block, 0, sizeof(block_free_node_t));
    return block;
}

os_status_t os_block_pool_free(os_block_pool_t *pool, void *block)
{
    if ((block == NULL) || !os_block_pool_owns(pool, block)) {
        return OS_ERR_INVALID_ARG;
    }
    if (block_is_free(pool, block)) {
        return OS_ERR_INVALID_STATE;
    }

    block_write_node(block, pool->free_head, pool->marker);
    pool->free_head = block;
    pool->free_count++;
    return OS_OK;
}

bool os_block_pool_owns(const os_block_pool_t *pool, const void *block)
{
    const uintptr_t addr  = (uintptr_t)block;
    const uintptr_t first = (uintptr_t)pool->base;
    const uintptr_t end   = first + (pool->block_count * pool->block_size);

    if ((pool->base == NULL) || (addr < first) || (addr >= end)) {
        return false;
    }
    return ((addr - first) % pool->block_size) == 0U;
}
