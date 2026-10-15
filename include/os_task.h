/**
 * @file    os_task.h
 * @brief   Task management API: create, delete, suspend, resume, notify.
 *
 * A task is a run-to-completion callback. The scheduler calls it either
 * periodically (period_ms > 0) or whenever it is notified (period_ms == 0,
 * "event-driven" task). Tasks never block: they do their work and return.
 *
 * The task control block is hidden behind the opaque os_task_handle_t, so
 * applications cannot depend on, or corrupt, the kernel's internal state.
 */
#ifndef OS_TASK_H
#define OS_TASK_H

#include "os_config.h"
#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to a task. The structure is only defined inside the kernel. */
typedef struct os_task *os_task_handle_t;

/** FreeRTOS-style alias, for readers coming from FreeRTOS. */
typedef os_task_handle_t TaskHandle_t;

/** Task entry point. Must return; it is called again on the next activation. */
typedef void (*os_task_fn_t)(void *arg);

/** Pass as period_ms to create an event-driven task (runs only when notified). */
#define OS_TASK_PERIOD_EVENT 0U

/** Lifecycle state of a task. */
typedef enum {
    OS_TASK_STATE_READY,     /**< Activated, waiting for the CPU. */
    OS_TASK_STATE_RUNNING,   /**< Currently executing its callback. */
    OS_TASK_STATE_WAITING,   /**< Waiting for its next period or a notification. */
    OS_TASK_STATE_SUSPENDED  /**< Excluded from scheduling until resumed. */
} os_task_state_t;

/** Parameters of os_task_create(). */
typedef struct {
    const char   *name;           /**< Debug name, truncated to OS_CFG_TASK_NAME_LEN - 1. */
    os_task_fn_t  fn;             /**< Callback, must not be NULL. */
    void         *arg;            /**< Opaque context passed to fn. */
    os_priority_t priority;       /**< Higher value runs first when several tasks are ready. */
    os_tick_t     period_ms;      /**< Activation period, or OS_TASK_PERIOD_EVENT. */
    os_tick_t     start_delay_ms; /**< Offset of the first activation (periodic tasks only). */
} os_task_config_t;

/** Snapshot of a task's state and runtime statistics. */
typedef struct {
    char            name[OS_CFG_TASK_NAME_LEN];
    os_task_state_t state;
    os_priority_t   priority;
    os_tick_t       period_ms;
    uint32_t        run_count;     /**< Completed activations. */
    uint32_t        overrun_count; /**< Periodic activations skipped because the task ran late. */
    os_tick_t       max_exec_ms;   /**< Longest observed execution time. */
} os_task_info_t;

/**
 * @brief  Create a task from the static task pool.
 * @param  config      Task parameters, copied by the kernel.
 * @param  out_handle  Receives the handle on success.
 * @return OS_OK, OS_ERR_INVALID_ARG or OS_ERR_NO_MEMORY.
 */
os_status_t os_task_create(const os_task_config_t *config, os_task_handle_t *out_handle);

/**
 * @brief  Delete a task and return its slot to the pool.
 *
 * A task may delete itself (pass NULL from its callback); the slot is then
 * reclaimed as soon as the callback returns. The handle is invalid afterwards.
 */
os_status_t os_task_delete(os_task_handle_t task);

/**
 * @brief  Stop scheduling a task until os_task_resume(). Idempotent.
 *         Pass NULL from a callback to suspend the calling task.
 */
os_status_t os_task_suspend(os_task_handle_t task);

/**
 * @brief  Resume a suspended task. Periodic activations missed while
 *         suspended are dropped: the task runs once now and keeps its period.
 * @return OS_ERR_INVALID_STATE if the task is not suspended.
 */
os_status_t os_task_resume(os_task_handle_t task);

/**
 * @brief  Request one extra activation of a task (wakes event-driven tasks).
 *
 * Notifications are coalesced: several notifies before the task runs cause a
 * single activation. A notify on a suspended task is kept until it resumes.
 */
os_status_t os_task_notify(os_task_handle_t task);

/** @return Handle of the task being executed, or NULL outside a task callback. */
os_task_handle_t os_task_get_current(void);

/** @brief Fill @p out_info with a snapshot of the task. NULL means the calling task. */
os_status_t os_task_get_info(os_task_handle_t task, os_task_info_t *out_info);

#ifdef __cplusplus
}
#endif

#endif /* OS_TASK_H */
