/**
 * @file    os_config.h
 * @brief   Compile-time configuration of the Mini RTOS kernel.
 *
 * Every option can be overridden from the build system without touching this
 * file, e.g. `make DEFINES=-DOS_CFG_MAX_TASKS=32`.
 */
#ifndef OS_CONFIG_H
#define OS_CONFIG_H

/** Maximum number of tasks alive at the same time (size of the static TCB pool). */
#ifndef OS_CFG_MAX_TASKS
#define OS_CFG_MAX_TASKS 16U
#endif

/** Maximum number of message queues alive at the same time. */
#ifndef OS_CFG_MAX_QUEUES
#define OS_CFG_MAX_QUEUES 8U
#endif

/** Maximum number of memory pools alive at the same time. */
#ifndef OS_CFG_MAX_MEMPOOLS
#define OS_CFG_MAX_MEMPOOLS 4U
#endif

/** Size of the task name buffer, including the terminating NUL. */
#ifndef OS_CFG_TASK_NAME_LEN
#define OS_CFG_TASK_NAME_LEN 16U
#endif

/**
 * Upper bound of a single idle sleep. Bounds the latency of events raised
 * outside the scheduler (interrupts, signal handlers) while the CPU idles.
 */
#ifndef OS_CFG_IDLE_MAX_SLEEP_MS
#define OS_CFG_IDLE_MAX_SLEEP_MS 10U
#endif

/** Assertion used for internal invariants only, never for argument checks. */
#ifndef OS_ASSERT
#include <assert.h>
#define OS_ASSERT(expr) assert(expr)
#endif

#endif /* OS_CONFIG_H */
