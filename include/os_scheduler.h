/**
 * @file    os_scheduler.h
 * @brief   Cooperative (non-preemptive) priority scheduler.
 *
 * The scheduler is a super-loop: on every iteration it releases the periodic
 * tasks whose activation time has come, then runs the highest-priority ready
 * task to completion. Tasks of equal priority run in FIFO order. When nothing
 * is ready, the CPU idles until the next activation.
 */
#ifndef OS_SCHEDULER_H
#define OS_SCHEDULER_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialise the port layer and reset all kernel objects.
 *         Must be called before any other kernel API.
 * @return OS_ERR_INVALID_STATE if called from inside a task.
 */
os_status_t os_scheduler_init(void);

/** @brief Run the scheduling loop until os_scheduler_stop() is called. */
void os_scheduler_start(void);

/** @brief Ask os_scheduler_start() to return after the current task. */
void os_scheduler_stop(void);

/**
 * @brief  Perform a single scheduling step: release due tasks, run at most one.
 *
 * Useful to embed the kernel inside an existing main loop, and in tests.
 * @return true if a task was executed.
 */
bool os_scheduler_run_once(void);

/** @return Current system tick in milliseconds. */
os_tick_t os_scheduler_get_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_SCHEDULER_H */
