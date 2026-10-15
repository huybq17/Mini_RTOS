/**
 * @file    os_queue.c
 * @brief   Copy-by-value ring-buffer message queue.
 */
#include "os_queue.h"

#include <string.h>

#include "os_block_pool.h"
#include "os_config.h"
#include "os_port.h"
#include "os_queue_internal.h"
#include "os_task_internal.h"

struct os_queue {
    uint8_t         *storage;
    size_t           item_size;
    size_t           capacity;
    size_t           head;     /**< Index of the oldest item. */
    size_t           count;
    os_task_handle_t receiver; /**< Notified on every successful send, may be NULL. */
    bool             in_use;
};

static struct os_queue s_queue_storage[OS_CFG_MAX_QUEUES];
static os_block_pool_t s_queue_pool;

static bool queue_is_valid(const struct os_queue *queue)
{
    if ((queue == NULL) || !os_block_pool_owns(&s_queue_pool, queue)) {
        return false;
    }
    return queue->in_use;
}

static uint8_t *queue_slot(const struct os_queue *queue, size_t index)
{
    return queue->storage + (index * queue->item_size);
}

void os_queue_module_init(void)
{
    memset(s_queue_storage, 0, sizeof(s_queue_storage));
    os_block_pool_init(&s_queue_pool, s_queue_storage, sizeof(struct os_queue),
                       OS_CFG_MAX_QUEUES);
}

void os_queue_on_task_deleted(os_task_handle_t task)
{
    for (size_t i = 0U; i < OS_CFG_MAX_QUEUES; ++i) {
        if (s_queue_storage[i].in_use && (s_queue_storage[i].receiver == task)) {
            s_queue_storage[i].receiver = NULL;
        }
    }
}

os_status_t os_queue_create(void *storage, size_t item_size, size_t capacity,
                            os_queue_handle_t *out_handle)
{
    if ((storage == NULL) || (item_size == 0U) || (capacity == 0U) || (out_handle == NULL)) {
        return OS_ERR_INVALID_ARG;
    }
    if (capacity > (SIZE_MAX / item_size)) {
        return OS_ERR_INVALID_ARG; /* Storage size would overflow size_t. */
    }

    os_status_t status = OS_ERR_NO_MEMORY;

    os_port_enter_critical();

    struct os_queue *queue = os_block_pool_alloc(&s_queue_pool);
    if (queue != NULL) {
        memset(queue, 0, sizeof(*queue));
        queue->storage   = (uint8_t *)storage;
        queue->item_size = item_size;
        queue->capacity  = capacity;
        queue->in_use    = true;
        *out_handle      = queue;
        status           = OS_OK;
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_queue_delete(os_queue_handle_t queue)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    if (queue_is_valid(queue)) {
        memset(queue, 0, sizeof(*queue));
        status = os_block_pool_free(&s_queue_pool, queue);
    } else {
        status = OS_ERR_INVALID_ARG;
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_queue_send(os_queue_handle_t queue, const void *item)
{
    if (item == NULL) {
        return OS_ERR_INVALID_ARG;
    }

    os_status_t      status   = OS_OK;
    os_task_handle_t receiver = NULL;

    os_port_enter_critical();

    if (!queue_is_valid(queue)) {
        status = OS_ERR_INVALID_ARG;
    } else if (queue->count == queue->capacity) {
        status = OS_ERR_QUEUE_FULL;
    } else {
        const size_t tail = (queue->head + queue->count) % queue->capacity;

        memcpy(queue_slot(queue, tail), item, queue->item_size);
        queue->count++;
        receiver = queue->receiver;
    }

    os_port_exit_critical();

    if (receiver != NULL) {
        (void)os_task_notify(receiver);
    }
    return status;
}

os_status_t os_queue_receive(os_queue_handle_t queue, void *out_item)
{
    if (out_item == NULL) {
        return OS_ERR_INVALID_ARG;
    }

    os_status_t status = OS_OK;

    os_port_enter_critical();

    if (!queue_is_valid(queue)) {
        status = OS_ERR_INVALID_ARG;
    } else if (queue->count == 0U) {
        status = OS_ERR_QUEUE_EMPTY;
    } else {
        memcpy(out_item, queue_slot(queue, queue->head), queue->item_size);
        queue->head = (queue->head + 1U) % queue->capacity;
        queue->count--;
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_queue_peek(os_queue_handle_t queue, void *out_item)
{
    if (out_item == NULL) {
        return OS_ERR_INVALID_ARG;
    }

    os_status_t status = OS_OK;

    os_port_enter_critical();

    if (!queue_is_valid(queue)) {
        status = OS_ERR_INVALID_ARG;
    } else if (queue->count == 0U) {
        status = OS_ERR_QUEUE_EMPTY;
    } else {
        memcpy(out_item, queue_slot(queue, queue->head), queue->item_size);
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_queue_set_receiver(os_queue_handle_t queue, os_task_handle_t task)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    if (!queue_is_valid(queue) || ((task != NULL) && !os_task_handle_is_valid(task))) {
        status = OS_ERR_INVALID_ARG;
    } else {
        queue->receiver = task;
    }

    os_port_exit_critical();
    return status;
}

size_t os_queue_count(os_queue_handle_t queue)
{
    size_t count = 0U;

    os_port_enter_critical();
    if (queue_is_valid(queue)) {
        count = queue->count;
    }
    os_port_exit_critical();

    return count;
}

size_t os_queue_space(os_queue_handle_t queue)
{
    size_t space = 0U;

    os_port_enter_critical();
    if (queue_is_valid(queue)) {
        space = queue->capacity - queue->count;
    }
    os_port_exit_critical();

    return space;
}
