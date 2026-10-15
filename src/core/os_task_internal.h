/**
 * @file    os_task_internal.h
 * @brief   Task control block definition (kernel private).
 *
 * Only kernel sources include this header. Applications see struct os_task
 * as an incomplete type through os_task_handle_t (opaque pointer).
 */
#ifndef OS_TASK_INTERNAL_H
#define OS_TASK_INTERNAL_H

#include "os_config.h"
#include "os_list.h"
#include "os_task.h"

/** Task Control Block (TCB). */
struct os_task {
    os_list_node_t  node;         /**< Links the TCB into the ready or delayed list. */
    os_task_fn_t    fn;
    void           *arg;
    char            name[OS_CFG_TASK_NAME_LEN];
    os_priority_t   priority;
    os_tick_t       period_ms;    /**< 0 for event-driven tasks. */
    os_tick_t       next_release; /**< Absolute tick of the next periodic activation. */
    os_task_state_t state;
    bool            in_use;         /**< Slot allocated from the pool. */
    bool            notified;       /**< Activation requested by os_task_notify(). */
    bool            delete_pending; /**< Deleted itself; reclaimed when the callback returns. */
    uint32_t        run_count;
    uint32_t        overrun_count;
    os_tick_t       max_exec_ms;
};

/** Reset the task pool. Called by os_scheduler_init(). */
void os_task_module_init(void);

/** Detach a TCB from the scheduler and return it to the pool. */
void os_task_free(struct os_task *task);

/** @return true if @p task designates a live task. */
bool os_task_handle_is_valid(os_task_handle_t task);

#endif /* OS_TASK_INTERNAL_H */
