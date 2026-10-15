/**
 * @file    os_mempool.c
 * @brief   Public fixed-size memory pools built on os_block_pool.
 *
 * The pool control blocks themselves come from an internal block pool, so
 * creating and deleting pools is O(1) as well.
 */
#include "os_mempool.h"

#include <string.h>

#include "os_block_pool.h"
#include "os_config.h"
#include "os_mempool_internal.h"
#include "os_port.h"

_Static_assert(OS_MEMPOOL_MIN_BLOCK_SIZE >= OS_BLOCK_POOL_MIN_BLOCK_SIZE,
               "public minimum block must hold the free-list node");

struct os_mempool {
    os_block_pool_t blocks;
    bool            in_use;
};

static struct os_mempool s_mempool_storage[OS_CFG_MAX_MEMPOOLS];
static os_block_pool_t   s_mempool_pool;

static bool mempool_is_valid(const struct os_mempool *pool)
{
    if ((pool == NULL) || !os_block_pool_owns(&s_mempool_pool, pool)) {
        return false;
    }
    return pool->in_use;
}

void os_mempool_module_init(void)
{
    memset(s_mempool_storage, 0, sizeof(s_mempool_storage));
    os_block_pool_init(&s_mempool_pool, s_mempool_storage, sizeof(struct os_mempool),
                       OS_CFG_MAX_MEMPOOLS);
}

os_status_t os_mempool_create(void *storage, size_t storage_size, size_t block_size,
                              os_mempool_handle_t *out_handle)
{
    if ((storage == NULL) || (block_size == 0U) || (out_handle == NULL)) {
        return OS_ERR_INVALID_ARG;
    }
    if (((uintptr_t)storage % OS_MEMPOOL_ALIGN) != 0U) {
        return OS_ERR_INVALID_ARG;
    }
    if (block_size > (SIZE_MAX - OS_MEMPOOL_ALIGN)) {
        return OS_ERR_INVALID_ARG; /* Rounding up would overflow. */
    }

    const size_t rounded_size = OS_MEMPOOL_BLOCK_SIZE(block_size);
    const size_t block_count  = storage_size / rounded_size;
    if (block_count == 0U) {
        return OS_ERR_INVALID_ARG;
    }

    os_status_t status = OS_ERR_NO_MEMORY;

    os_port_enter_critical();

    struct os_mempool *pool = os_block_pool_alloc(&s_mempool_pool);
    if (pool != NULL) {
        memset(pool, 0, sizeof(*pool));
        os_block_pool_init(&pool->blocks, storage, rounded_size, block_count);
        pool->in_use = true;
        *out_handle  = pool;
        status       = OS_OK;
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_mempool_delete(os_mempool_handle_t pool)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    if (!mempool_is_valid(pool)) {
        status = OS_ERR_INVALID_ARG;
    } else if (pool->blocks.free_count != pool->blocks.block_count) {
        status = OS_ERR_INVALID_STATE; /* Blocks still owned by someone. */
    } else {
        memset(pool, 0, sizeof(*pool));
        status = os_block_pool_free(&s_mempool_pool, pool);
    }

    os_port_exit_critical();
    return status;
}

void *os_mempool_alloc(os_mempool_handle_t pool)
{
    void *block = NULL;

    os_port_enter_critical();
    if (mempool_is_valid(pool)) {
        block = os_block_pool_alloc(&pool->blocks);
    }
    os_port_exit_critical();

    return block;
}

os_status_t os_mempool_free(os_mempool_handle_t pool, void *block)
{
    os_status_t status = OS_ERR_INVALID_ARG;

    os_port_enter_critical();
    if (mempool_is_valid(pool)) {
        status = os_block_pool_free(&pool->blocks, block);
    }
    os_port_exit_critical();

    return status;
}

os_status_t os_mempool_get_info(os_mempool_handle_t pool, os_mempool_info_t *out_info)
{
    if (out_info == NULL) {
        return OS_ERR_INVALID_ARG;
    }

    os_status_t status = OS_OK;

    os_port_enter_critical();

    if (mempool_is_valid(pool)) {
        out_info->block_size     = pool->blocks.block_size;
        out_info->block_count    = pool->blocks.block_count;
        out_info->free_count     = pool->blocks.free_count;
        out_info->min_free_count = pool->blocks.min_free_count;
    } else {
        status = OS_ERR_INVALID_ARG;
    }

    os_port_exit_critical();
    return status;
}
