/**
 * @file    test_helpers.h
 * @brief   Task callbacks and drivers shared by the kernel test suites.
 */
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stddef.h>

#include "os.h"
#include "os_port_sim.h"

#define TEST_RUN_LOG_CAPACITY 32U

/** Records the tick of every activation of a task (arg of test_log_task). */
typedef struct {
    os_tick_t times[TEST_RUN_LOG_CAPACITY];
    size_t    count;
} test_run_log_t;

/** Task callback appending the current tick to the test_run_log_t in @p arg. */
void test_log_task(void *arg);

/** Task callback appending the first character of the string in @p arg to the trace. */
void test_trace_task(void *arg);
void test_trace_reset(void);
const char *test_trace_get(void);

/** Create a task with no start delay. @return NULL on failure. */
os_task_handle_t test_spawn(const char *name, os_task_fn_t fn, void *arg, os_priority_t priority,
                            os_tick_t period_ms);

/** Run the scheduler on the virtual clock, 1 ms at a time, until @p end_tick is reached. */
void test_run_until(os_tick_t end_tick);

#endif /* TEST_HELPERS_H */
