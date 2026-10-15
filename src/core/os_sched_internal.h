/**
 * @file    os_sched_internal.h
 * @brief   Scheduler services used by other kernel modules (kernel private).
 *
 * Callers must hold the critical section.
 */
#ifndef OS_SCHED_INTERNAL_H
#define OS_SCHED_INTERNAL_H

#include "os_task_internal.h"

/** Move a task to the ready list, sorted by priority. */
void os_sched_make_ready(struct os_task *task);

/**
 * Park a task until its next activation: ready list if a notification is
 * pending, delayed list if periodic, otherwise no list (waits for a notify).
 */
void os_sched_enqueue(struct os_task *task);

/** Remove a task from whatever scheduler list holds it. */
void os_sched_detach(struct os_task *task);

/** @return Task being executed, or NULL outside a task callback. */
struct os_task *os_sched_current(void);

#endif /* OS_SCHED_INTERNAL_H */
