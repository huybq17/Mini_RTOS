/**
 * @file    os_task.c
 * @brief   Task lifecycle: static TCB pool, create/delete/suspend/resume/notify.
 */
#include "os_task.h"

#include <string.h>

#include "os_block_pool.h"
#include "os_port.h"
#include "os_queue_internal.h"
#include "os_sched_internal.h"
#include "os_task_internal.h"
#include "os_tick.h"

/* All TCBs live here: no heap, bounded memory, O(1) allocation. */
static struct os_task  s_task_storage[OS_CFG_MAX_TASKS];
static os_block_pool_t s_task_pool;

/* Reject NULL, pointers outside the pool, misaligned pointers and free slots. */
static bool task_is_valid(const struct os_task *task)
{
    if ((task == NULL) || !os_block_pool_owns(&s_task_pool, task)) {
        return false;
    }
    return task->in_use && !task->delete_pending;
}

/* NULL designates the calling task, as in FreeRTOS. */
static struct os_task *task_resolve(os_task_handle_t task)
{
    struct os_task *tcb = (task != NULL) ? task : os_sched_current();

    return task_is_valid(tcb) ? tcb : NULL;
}

static struct os_task *task_alloc(void)
{
    struct os_task *tcb = os_block_pool_alloc(&s_task_pool);

    if (tcb != NULL) {
        memset(tcb, 0, sizeof(*tcb));
        os_list_node_init(&tcb->node);
        tcb->in_use = true;
    }
    return tcb;
}

static void task_copy_name(char *dst, const char *src)
{
    size_t len = 0U;

    if (src != NULL) {
        len = strlen(src);
        if (len >= OS_CFG_TASK_NAME_LEN) {
            len = OS_CFG_TASK_NAME_LEN - 1U;
        }
        memcpy(dst, src, len);
    }
    dst[len] = '\0';
}

void os_task_module_init(void)
{
    memset(s_task_storage, 0, sizeof(s_task_storage));
    os_block_pool_init(&s_task_pool, s_task_storage, sizeof(struct os_task), OS_CFG_MAX_TASKS);
}

void os_task_free(struct os_task *task)
{
    os_sched_detach(task);
    os_queue_on_task_deleted(task);
    memset(task, 0, sizeof(*task));

    const os_status_t status = os_block_pool_free(&s_task_pool, task);
    OS_ASSERT(status == OS_OK);
    (void)status;
}

bool os_task_handle_is_valid(os_task_handle_t task)
{
    return task_is_valid(task);
}

os_status_t os_task_create(const os_task_config_t *config, os_task_handle_t *out_handle)
{
    if ((config == NULL) || (config->fn == NULL) || (out_handle == NULL)) {
        return OS_ERR_INVALID_ARG;
    }

    os_port_enter_critical();

    struct os_task *tcb = task_alloc();
    if (tcb == NULL) {
        os_port_exit_critical();
        return OS_ERR_NO_MEMORY;
    }

    tcb->fn           = config->fn;
    tcb->arg          = config->arg;
    tcb->priority     = config->priority;
    tcb->period_ms    = config->period_ms;
    tcb->next_release = os_port_get_tick() + config->start_delay_ms;
    task_copy_name(tcb->name, config->name);
    os_sched_enqueue(tcb);

    os_port_exit_critical();

    *out_handle = tcb;
    return OS_OK;
}

os_status_t os_task_delete(os_task_handle_t task)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    struct os_task *tcb = task_resolve(task);
    if (tcb == NULL) {
        status = OS_ERR_INVALID_ARG;
    } else if (tcb == os_sched_current()) {
        /* Its callback is still on the stack: let the scheduler reclaim it. */
        tcb->delete_pending = true;
    } else {
        os_task_free(tcb);
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_task_suspend(os_task_handle_t task)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    struct os_task *tcb = task_resolve(task);
    if (tcb == NULL) {
        status = OS_ERR_INVALID_ARG;
    } else if (tcb->state != OS_TASK_STATE_SUSPENDED) {
        /* A running task is not in any list; the scheduler checks its state on return. */
        if (tcb != os_sched_current()) {
            os_sched_detach(tcb);
        }
        tcb->state = OS_TASK_STATE_SUSPENDED;
    } else {
        /* Already suspended: nothing to do. */
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_task_resume(os_task_handle_t task)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    struct os_task *tcb = task_resolve(task);
    if (tcb == NULL) {
        status = OS_ERR_INVALID_ARG;
    } else if (tcb->state != OS_TASK_STATE_SUSPENDED) {
        status = OS_ERR_INVALID_STATE;
    } else if (tcb == os_sched_current()) {
        /* Suspended and resumed within its own callback. */
        tcb->state = OS_TASK_STATE_RUNNING;
    } else {
        const os_tick_t now = os_port_get_tick();

        /* Drop the activations missed while suspended instead of counting overruns. */
        if ((tcb->period_ms != 0U) && os_tick_reached(now, tcb->next_release)) {
            tcb->next_release = now;
        }
        os_sched_enqueue(tcb);
    }

    os_port_exit_critical();
    return status;
}

os_status_t os_task_notify(os_task_handle_t task)
{
    os_status_t status = OS_OK;

    os_port_enter_critical();

    struct os_task *tcb = task_resolve(task);
    if (tcb == NULL) {
        status = OS_ERR_INVALID_ARG;
    } else {
        tcb->notified = true;
        /* READY: already queued. RUNNING: re-queued on return. SUSPENDED: kept for resume. */
        if (tcb->state == OS_TASK_STATE_WAITING) {
            os_sched_make_ready(tcb);
        }
    }

    os_port_exit_critical();
    return status;
}

os_task_handle_t os_task_get_current(void)
{
    return os_sched_current();
}

os_status_t os_task_get_info(os_task_handle_t task, os_task_info_t *out_info)
{
    if (out_info == NULL) {
        return OS_ERR_INVALID_ARG;
    }

    os_status_t status = OS_OK;

    os_port_enter_critical();

    const struct os_task *tcb = task_resolve(task);
    if (tcb == NULL) {
        status = OS_ERR_INVALID_ARG;
    } else {
        memcpy(out_info->name, tcb->name, sizeof(out_info->name));
        out_info->state         = tcb->state;
        out_info->priority      = tcb->priority;
        out_info->period_ms     = tcb->period_ms;
        out_info->run_count     = tcb->run_count;
        out_info->overrun_count = tcb->overrun_count;
        out_info->max_exec_ms   = tcb->max_exec_ms;
    }

    os_port_exit_critical();
    return status;
}
