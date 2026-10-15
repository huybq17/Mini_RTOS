/**
 * @file    os_queue_internal.h
 * @brief   Queue services used by other kernel modules (kernel private).
 */
#ifndef OS_QUEUE_INTERNAL_H
#define OS_QUEUE_INTERNAL_H

#include "os_queue.h"

/** Reset the queue pool. Called by os_scheduler_init(). */
void os_queue_module_init(void);

/** Unbind @p task from every queue, so a deleted task is never notified. */
void os_queue_on_task_deleted(os_task_handle_t task);

#endif /* OS_QUEUE_INTERNAL_H */
