/**
 * @file    os_queue.h
 * @brief   Fixed-size message queue for inter-task communication.
 *
 * Items are copied by value into a ring buffer supplied by the caller, so the
 * kernel never allocates memory. A queue can be bound to a receiver task that
 * is notified on every successful send, which turns the receiver into a
 * purely event-driven consumer.
 */
#ifndef OS_QUEUE_H
#define OS_QUEUE_H

#include "os_task.h"
#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to a queue. */
typedef struct os_queue *os_queue_handle_t;

/** FreeRTOS-style alias. */
typedef os_queue_handle_t QueueHandle_t;

/** Number of bytes of storage required by a queue. */
#define OS_QUEUE_STORAGE_SIZE(item_size, capacity) ((size_t)(item_size) * (size_t)(capacity))

/**
 * @brief  Create a queue on top of caller-provided storage.
 * @param  storage     Buffer of at least OS_QUEUE_STORAGE_SIZE(item_size, capacity)
 *                     bytes that outlives the queue.
 * @param  item_size   Size of one item in bytes.
 * @param  capacity    Maximum number of items.
 * @param  out_handle  Receives the handle on success.
 * @return OS_OK, OS_ERR_INVALID_ARG or OS_ERR_NO_MEMORY.
 */
os_status_t os_queue_create(void *storage, size_t item_size, size_t capacity,
                            os_queue_handle_t *out_handle);

/** @brief Delete a queue. Pending items are discarded; the storage is not touched. */
os_status_t os_queue_delete(os_queue_handle_t queue);

/**
 * @brief  Copy @p item to the back of the queue and notify the receiver task.
 * @return OS_OK, OS_ERR_INVALID_ARG or OS_ERR_QUEUE_FULL (never blocks).
 */
os_status_t os_queue_send(os_queue_handle_t queue, const void *item);

/**
 * @brief  Copy the front item into @p out_item and remove it.
 * @return OS_OK, OS_ERR_INVALID_ARG or OS_ERR_QUEUE_EMPTY (never blocks).
 */
os_status_t os_queue_receive(os_queue_handle_t queue, void *out_item);

/** @brief Copy the front item into @p out_item without removing it. */
os_status_t os_queue_peek(os_queue_handle_t queue, void *out_item);

/**
 * @brief  Bind a task that is notified on every successful send.
 *         Pass NULL to unbind. The binding is dropped if the task is deleted.
 */
os_status_t os_queue_set_receiver(os_queue_handle_t queue, os_task_handle_t task);

/** @return Number of items currently stored, 0 for an invalid handle. */
size_t os_queue_count(os_queue_handle_t queue);

/** @return Number of free slots, 0 for an invalid handle. */
size_t os_queue_space(os_queue_handle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* OS_QUEUE_H */
