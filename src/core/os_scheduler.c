/**
 * @file    os_scheduler.c
 * @brief   Cooperative priority scheduler (super-loop).
 *
 * Two intrusive lists drive the scheduling:
 *   - ready list   : sorted by priority (highest first), FIFO among equals.
 *   - delayed list : periodic tasks sorted by next release tick.
 * A running task belongs to no list; event-driven tasks waiting for a
 * notification belong to no list either.
 */
#include "os_scheduler.h"

#include "os_mempool_internal.h"
#include "os_port.h"
#include "os_queue_internal.h"
#include "os_sched_internal.h"
#include "os_task_internal.h"
#include "os_tick.h"

static os_list_t       s_ready_list;
static os_list_t       s_delayed_list;
static struct os_task *s_current;
static volatile bool   s_running; /* May be cleared from a signal handler or an ISR. */

static struct os_task *sched_task_of(os_list_node_t *node)
{
    return OS_CONTAINER_OF(node, struct os_task, node);
}

static bool sched_ready_less(const os_list_node_t *lhs, const os_list_node_t *rhs)
{
    const struct os_task *a = OS_CONTAINER_OF_CONST(lhs, struct os_task, node);
    const struct os_task *b = OS_CONTAINER_OF_CONST(rhs, struct os_task, node);

    return a->priority > b->priority;
}

static bool sched_delayed_less(const os_list_node_t *lhs, const os_list_node_t *rhs)
{
    const struct os_task *a = OS_CONTAINER_OF_CONST(lhs, struct os_task, node);
    const struct os_task *b = OS_CONTAINER_OF_CONST(rhs, struct os_task, node);

    return os_tick_after(b->next_release, a->next_release);
}

/* Move every periodic task whose release time has come to the ready list. */
static void sched_release_due_tasks(os_tick_t now)
{
    os_list_node_t *node = os_list_front(&s_delayed_list);

    while (node != NULL) {
        struct os_task *task = sched_task_of(node);

        if (!os_tick_reached(now, task->next_release)) {
            break; /* Sorted list: every following task is due later. */
        }
        os_sched_make_ready(task);
        node = os_list_front(&s_delayed_list);
    }
}

/*
 * Compute the next release of a periodic task that just completed at @p now.
 * The release grid is anchored on the first release, so the period does not
 * drift with execution time. Releases already in the past when the task
 * completes were skipped: they are counted as overruns.
 */
static void sched_advance_release(struct os_task *task, os_tick_t now)
{
    task->next_release += task->period_ms;

    if (os_tick_after(now, task->next_release)) {
        const os_tick_t missed = ((now - task->next_release - 1U) / task->period_ms) + 1U;

        task->overrun_count += missed;
        task->next_release += missed * task->period_ms;
    }
}

/* Run one task to completion. Called with the critical section held. */
static void sched_dispatch(struct os_task *task)
{
    const os_tick_t start = os_port_get_tick();
    const bool periodic_release =
        (task->period_ms != 0U) && os_tick_reached(start, task->next_release);

    task->state    = OS_TASK_STATE_RUNNING;
    task->notified = false;
    s_current      = task;

    /* The callback may call any kernel API, so it runs outside the critical section. */
    os_port_exit_critical();
    task->fn(task->arg);
    os_port_enter_critical();

    s_current = NULL;

    const os_tick_t exec_ms = os_port_get_tick() - start;
    if (exec_ms > task->max_exec_ms) {
        task->max_exec_ms = exec_ms;
    }
    task->run_count++;

    if (task->delete_pending) {
        os_task_free(task);
    } else if (task->state == OS_TASK_STATE_SUSPENDED) {
        /* Suspended itself: stays out of every list until resumed. */
    } else {
        if (periodic_release) {
            sched_advance_release(task, start + exec_ms);
        }
        os_sched_enqueue(task);
    }
}

/* How long the CPU may sleep before the next activation. */
static os_tick_t sched_idle_budget(void)
{
    os_tick_t budget = OS_CFG_IDLE_MAX_SLEEP_MS;

    os_port_enter_critical();

    if (!os_list_is_empty(&s_ready_list)) {
        budget = 0U;
    } else {
        os_list_node_t *node = os_list_front(&s_delayed_list);

        if (node != NULL) {
            const os_tick_t now     = os_port_get_tick();
            const os_tick_t release = sched_task_of(node)->next_release;

            if (os_tick_reached(now, release)) {
                budget = 0U;
            } else if ((release - now) < budget) {
                budget = release - now;
            } else {
                /* Next release is further away than the maximum sleep. */
            }
        }
    }

    os_port_exit_critical();
    return budget;
}

void os_sched_make_ready(struct os_task *task)
{
    os_list_remove(&task->node);
    task->state = OS_TASK_STATE_READY;
    os_list_insert_sorted(&s_ready_list, &task->node, sched_ready_less);
}

void os_sched_enqueue(struct os_task *task)
{
    if (task->notified) {
        os_sched_make_ready(task);
        return;
    }

    os_list_remove(&task->node);
    task->state = OS_TASK_STATE_WAITING;
    if (task->period_ms != 0U) {
        os_list_insert_sorted(&s_delayed_list, &task->node, sched_delayed_less);
    }
}

void os_sched_detach(struct os_task *task)
{
    os_list_remove(&task->node);
}

struct os_task *os_sched_current(void)
{
    return s_current;
}

os_status_t os_scheduler_init(void)
{
    if (s_current != NULL) {
        return OS_ERR_INVALID_STATE;
    }

    os_port_init();
    os_list_init(&s_ready_list);
    os_list_init(&s_delayed_list);
    s_running = false;
    os_task_module_init();
    os_queue_module_init();
    os_mempool_module_init();

    return OS_OK;
}

void os_scheduler_start(void)
{
    s_running = true;

    while (s_running) {
        if (!os_scheduler_run_once()) {
            const os_tick_t budget = sched_idle_budget();

            if (budget > 0U) {
                os_port_idle(budget);
            }
        }
    }
}

void os_scheduler_stop(void)
{
    s_running = false;
}

bool os_scheduler_run_once(void)
{
    /* Not re-entrant: a task must return instead of running the scheduler. */
    if (s_current != NULL) {
        return false;
    }

    bool ran = false;

    os_port_enter_critical();

    sched_release_due_tasks(os_port_get_tick());

    os_list_node_t *node = os_list_pop_front(&s_ready_list);
    if (node != NULL) {
        sched_dispatch(sched_task_of(node));
        ran = true;
    }

    os_port_exit_critical();
    return ran;
}

os_tick_t os_scheduler_get_tick(void)
{
    return os_port_get_tick();
}
