/**
 * @file    test_helpers.c
 * @brief   Task callbacks and drivers shared by the kernel test suites.
 */
#include "test_helpers.h"

#include <stdint.h>

#define TRACE_CAPACITY 64U

static char   s_trace[TRACE_CAPACITY];
static size_t s_trace_len;

void test_log_task(void *arg)
{
    test_run_log_t *log = (test_run_log_t *)arg;

    if (log->count < TEST_RUN_LOG_CAPACITY) {
        log->times[log->count] = os_scheduler_get_tick();
    }
    log->count++;
}

void test_trace_task(void *arg)
{
    const char *tag = (const char *)arg;

    if (s_trace_len < (TRACE_CAPACITY - 1U)) {
        s_trace[s_trace_len] = tag[0];
        s_trace_len++;
        s_trace[s_trace_len] = '\0';
    }
}

void test_trace_reset(void)
{
    s_trace_len = 0U;
    s_trace[0]  = '\0';
}

const char *test_trace_get(void)
{
    return s_trace;
}

os_task_handle_t test_spawn(const char *name, os_task_fn_t fn, void *arg, os_priority_t priority,
                            os_tick_t period_ms)
{
    const os_task_config_t config = {
        .name           = name,
        .fn             = fn,
        .arg            = arg,
        .priority       = priority,
        .period_ms      = period_ms,
        .start_delay_ms = 0U,
    };
    os_task_handle_t handle = NULL;

    if (os_task_create(&config, &handle) != OS_OK) {
        return NULL;
    }
    return handle;
}

void test_run_until(os_tick_t end_tick)
{
    for (;;) {
        while (os_scheduler_run_once()) {
        }
        if ((int32_t)(os_scheduler_get_tick() - end_tick) >= 0) {
            break;
        }
        os_port_sim_advance(1U);
    }
}
