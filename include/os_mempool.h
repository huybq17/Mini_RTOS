/**
 * @file    os_mempool.h
 * @brief   Fixed-size memory pools: deterministic O(1) alloc/free, no fragmentation.
 *
 * Typical use is zero-copy messaging, as in lwIP or CMSIS-RTOS osMemoryPool:
 * the producer allocates a block, fills it and sends only the pointer through
 * a queue; the consumer processes the block and frees it. Ownership moves
 * with the pointer: after sending, the producer must not touch the block.
 */
#ifndef OS_MEMPOOL_H
#define OS_MEMPOOL_H

#include <stdalign.h>
#include <stddef.h>

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to a memory pool. */
typedef struct os_mempool *os_mempool_handle_t;

/** Alignment of every block: suitable for any C object type. */
#define OS_MEMPOOL_ALIGN ((size_t)alignof(max_align_t))

/** Smallest block: a free block stores the free-list link in place. */
#define OS_MEMPOOL_MIN_BLOCK_SIZE (2U * sizeof(void *))

/** Actual size of a block able to hold @p size bytes. */
#define OS_MEMPOOL_BLOCK_SIZE(size)                                                       \
    ((((((size_t)(size) < OS_MEMPOOL_MIN_BLOCK_SIZE) ? OS_MEMPOOL_MIN_BLOCK_SIZE          \
                                                      : (size_t)(size)) +                 \
       OS_MEMPOOL_ALIGN - 1U) /                                                            \
      OS_MEMPOOL_ALIGN) *                                                                 \
     OS_MEMPOOL_ALIGN)

/** Bytes of storage needed for @p count blocks of @p size bytes. */
#define OS_MEMPOOL_STORAGE_SIZE(size, count) (OS_MEMPOOL_BLOCK_SIZE(size) * (size_t)(count))

/**
 * Declare a correctly aligned storage array, e.g.
 * `static OS_MEMPOOL_DECLARE_STORAGE(s_frames, sizeof(frame_t), 4U);`
 */
#define OS_MEMPOOL_DECLARE_STORAGE(name, size, count)                                \
    max_align_t name[(OS_MEMPOOL_STORAGE_SIZE(size, count) + sizeof(max_align_t) - 1U) / \
                     sizeof(max_align_t)]

/** Snapshot of a pool's geometry and usage. */
typedef struct {
    size_t block_size;     /**< Rounded block size in bytes. */
    size_t block_count;
    size_t free_count;
    size_t min_free_count; /**< Low watermark: size the pool from this value. */
} os_mempool_info_t;

/**
 * @brief  Create a pool that carves @p storage into equal blocks.
 * @param  storage       Buffer aligned to OS_MEMPOOL_ALIGN that outlives the pool
 *                       (use OS_MEMPOOL_DECLARE_STORAGE).
 * @param  storage_size  Size of @p storage in bytes; the block count is derived from it.
 * @param  block_size    Requested block size, rounded up with OS_MEMPOOL_BLOCK_SIZE().
 * @param  out_handle    Receives the handle on success.
 * @return OS_OK, OS_ERR_INVALID_ARG (NULL, misaligned or too small storage)
 *         or OS_ERR_NO_MEMORY (OS_CFG_MAX_MEMPOOLS reached).
 */
os_status_t os_mempool_create(void *storage, size_t storage_size, size_t block_size,
                              os_mempool_handle_t *out_handle);

/**
 * @brief  Delete a pool. All its blocks must have been freed.
 * @return OS_ERR_INVALID_STATE while blocks are still allocated.
 */
os_status_t os_mempool_delete(os_mempool_handle_t pool);

/** @return A block in O(1), or NULL if the pool is exhausted or the handle invalid. */
void *os_mempool_alloc(os_mempool_handle_t pool);

/**
 * @brief  Return a block to its pool in O(1).
 * @return OS_ERR_INVALID_ARG for NULL or a pointer that is not a block of this pool,
 *         OS_ERR_INVALID_STATE for a double free.
 */
os_status_t os_mempool_free(os_mempool_handle_t pool, void *block);

/** @brief Fill @p out_info with the pool's geometry and usage. */
os_status_t os_mempool_get_info(os_mempool_handle_t pool, os_mempool_info_t *out_info);

#ifdef __cplusplus
}
#endif

#endif /* OS_MEMPOOL_H */
