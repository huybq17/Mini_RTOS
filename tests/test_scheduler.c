/**
 * @file    test_scheduler.c
 * @brief   Scheduling behaviour on the virtual clock: timing, priorities,
 *          suspend/resume/delete from inside tasks, notifications, overruns.
 */
#include <stdint.h>
#include <string.h>

#include "test_framework.h"
#include "test_helpers.h"
#include "test_suites.h"

static char s_tag_a[] = "A";
static char s_tag_b[] = "B";
static char s_tag_c[] = "C";
static char s_tag_high[] = "H";
static char s_tag_mid[]  = "M";
static char s_tag_low[]  = "L";

static uint32_t s_runs;

static void setup(void)
{
    (void)os_scheduler_init();
    test_trace_reset();
    s_runs = 0U;
}

static void self_delete_on_third_run_task(void *arg)
{
    (void)arg;
    s_runs++;
    if (s_runs == 3U) {
        (void)os_task_delete(NULL);
    }
}

static void self_suspend_task(void *arg)
{
    (void)arg;
    s_runs++;
    (void)os_task_suspend(NULL);
}

static void notify_self_once_task(void *arg)
{
    (void)arg;
    s_runs++;
    if (s_runs == 1U) {
        (void)os_task_notify(NULL);
    }
}

static void long_job_task(void *arg)
{
    (void)arg;
    s_runs++;
    os_port_sim_advance(250U); /* Simulate 250 ms of work in a 100 ms period. */
}

static void stop_after_five_runs_task(void *arg)
{
    (void)arg;
    s_runs++;
    if (s_runs == 5U) {
        os_scheduler_stop();
    }
}

static void test_idle_scheduler_runs_nothing(void)
{
    setup();

    TEST_ASSERT(!os_scheduler_run_once());
}

static void test_periodic_task_follows_its_period(void)
{
    test_run_log_t log = { { 0U }, 0U };

    setup();
    TEST_ASSERT(test_spawn("p", test_log_task, &log, 1U, 100U) != NULL);

    test_run_until(1000U);

    TEST_ASSERT_EQ(11, log.count);
    for (size_t i = 0U; i < log.count; ++i) {
        TEST_ASSERT_EQ(i * 100U, log.times[i]);
    }
}

static void test_start_delay_offsets_first_release(void)
{
    test_run_log_t         log    = { { 0U }, 0U };
    const os_task_config_t config = { "p", test_log_task, &log, 1U, 100U, 50U };
    os_task_handle_t       handle = NULL;

    setup();
    TEST_ASSERT_EQ(OS_OK, os_task_create(&config, &handle));

    test_run_until(250U);

    TEST_ASSERT_EQ(3, log.count);
    TEST_ASSERT_EQ(50, log.times[0]);
    TEST_ASSERT_EQ(150, log.times[1]);
    TEST_ASSERT_EQ(250, log.times[2]);
}

static void test_highest_priority_runs_first(void)
{
    setup();
    TEST_ASSERT(test_spawn("low", test_trace_task, s_tag_low, 1U, 100U) != NULL);
    TEST_ASSERT(test_spawn("high", test_trace_task, s_tag_high, 9U, 100U) != NULL);
    TEST_ASSERT(test_spawn("mid", test_trace_task, s_tag_mid, 5U, 100U) != NULL);

    test_run_until(0U);

    TEST_ASSERT(strcmp(test_trace_get(), "HML") == 0);
}

static void test_equal_priorities_run_in_fifo_order(void)
{
    setup();
    TEST_ASSERT(test_spawn("a", test_trace_task, s_tag_a, 2U, 100U) != NULL);
    TEST_ASSERT(test_spawn("b", test_trace_task, s_tag_b, 2U, 100U) != NULL);
    TEST_ASSERT(test_spawn("c", test_trace_task, s_tag_c, 2U, 100U) != NULL);

    test_run_until(100U);

    TEST_ASSERT(strcmp(test_trace_get(), "ABCABC") == 0);
}

static void test_suspended_task_does_not_run(void)
{
    test_run_log_t log = { { 0U }, 0U };
    os_task_info_t info;

    setup();
    os_task_handle_t task = test_spawn("p", test_log_task, &log, 1U, 100U);

    test_run_until(200U);
    TEST_ASSERT_EQ(3, log.count);

    TEST_ASSERT_EQ(OS_OK, os_task_suspend(task));
    test_run_until(500U);
    TEST_ASSERT_EQ(3, log.count);

    /* Resume at 500: runs immediately, then keeps its 100 ms period. */
    TEST_ASSERT_EQ(OS_OK, os_task_resume(task));
    test_run_until(600U);
    TEST_ASSERT_EQ(5, log.count);
    TEST_ASSERT_EQ(500, log.times[3]);
    TEST_ASSERT_EQ(600, log.times[4]);

    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT_EQ(0, info.overrun_count); /* Suspension is not an overrun. */
}

static void test_task_can_delete_itself(void)
{
    os_task_info_t info;

    setup();
    os_task_handle_t task = test_spawn("p", self_delete_on_third_run_task, NULL, 1U, 100U);

    test_run_until(1000U);

    TEST_ASSERT_EQ(3, s_runs);
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_get_info(task, &info));
}

static void test_task_can_suspend_itself(void)
{
    os_task_info_t info;

    setup();
    os_task_handle_t task = test_spawn("p", self_suspend_task, NULL, 1U, 100U);

    test_run_until(500U);
    TEST_ASSERT_EQ(1, s_runs);
    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT_EQ(OS_TASK_STATE_SUSPENDED, info.state);

    TEST_ASSERT_EQ(OS_OK, os_task_resume(task));
    TEST_ASSERT(os_scheduler_run_once());
    TEST_ASSERT_EQ(2, s_runs);
}

static void test_event_task_runs_only_when_notified(void)
{
    test_run_log_t log = { { 0U }, 0U };

    setup();
    os_task_handle_t task = test_spawn("e", test_log_task, &log, 1U, OS_TASK_PERIOD_EVENT);

    test_run_until(500U);
    TEST_ASSERT_EQ(0, log.count);

    /* Two notifies before the task runs are coalesced into one activation. */
    TEST_ASSERT_EQ(OS_OK, os_task_notify(task));
    TEST_ASSERT_EQ(OS_OK, os_task_notify(task));
    TEST_ASSERT(os_scheduler_run_once());
    TEST_ASSERT(!os_scheduler_run_once());
    TEST_ASSERT_EQ(1, log.count);
    TEST_ASSERT_EQ(500, log.times[0]);
}

static void test_notify_while_running_requeues_task(void)
{
    setup();
    os_task_handle_t task = test_spawn("e", notify_self_once_task, NULL, 1U, OS_TASK_PERIOD_EVENT);

    TEST_ASSERT_EQ(OS_OK, os_task_notify(task));
    test_run_until(10U);

    TEST_ASSERT_EQ(2, s_runs);
}

static void test_notify_of_suspended_task_is_kept_until_resume(void)
{
    test_run_log_t log = { { 0U }, 0U };

    setup();
    os_task_handle_t task = test_spawn("e", test_log_task, &log, 1U, OS_TASK_PERIOD_EVENT);

    TEST_ASSERT_EQ(OS_OK, os_task_suspend(task));
    TEST_ASSERT_EQ(OS_OK, os_task_notify(task));
    TEST_ASSERT(!os_scheduler_run_once());

    TEST_ASSERT_EQ(OS_OK, os_task_resume(task));
    TEST_ASSERT(os_scheduler_run_once());
    TEST_ASSERT_EQ(1, log.count);
}

static void test_overrun_is_detected_and_period_realigned(void)
{
    os_task_info_t info;

    setup();
    os_task_handle_t task = test_spawn("slow", long_job_task, NULL, 1U, 100U);

    /* Runs from 0 to 250: the releases at 100 and 200 are skipped. */
    TEST_ASSERT(os_scheduler_run_once());
    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT_EQ(2, info.overrun_count);
    TEST_ASSERT_EQ(250, info.max_exec_ms);

    /* Next release stays on the 100 ms grid: 300, not 350. */
    os_port_sim_set_tick(299U);
    TEST_ASSERT(!os_scheduler_run_once());
    os_port_sim_set_tick(300U);
    TEST_ASSERT(os_scheduler_run_once());
    TEST_ASSERT_EQ(2, s_runs);
}

static void test_periodic_timing_survives_tick_wraparound(void)
{
    test_run_log_t  log   = { { 0U }, 0U };
    const os_tick_t start = UINT32_MAX - 149U;

    setup();
    os_port_sim_set_tick(start);
    TEST_ASSERT(test_spawn("p", test_log_task, &log, 1U, 100U) != NULL);

    test_run_until(150U); /* Crosses 0xFFFFFFFF -> 0. */

    TEST_ASSERT_EQ(4, log.count);
    TEST_ASSERT_EQ(start, log.times[0]);
    TEST_ASSERT_EQ(start + 100U, log.times[1]);
    TEST_ASSERT_EQ(50, log.times[2]);
    TEST_ASSERT_EQ(150, log.times[3]);
}

static void test_start_returns_after_stop(void)
{
    setup();
    TEST_ASSERT(test_spawn("p", stop_after_five_runs_task, NULL, 1U, 10U) != NULL);

    os_scheduler_start(); /* The simulated idle jumps straight to each release. */

    TEST_ASSERT_EQ(5, s_runs);
    TEST_ASSERT_EQ(40, os_scheduler_get_tick());
}

static void test_critical_sections_are_balanced(void)
{
    test_run_log_t log = { { 0U }, 0U };

    setup();
    os_task_handle_t periodic = test_spawn("p", test_log_task, &log, 2U, 10U);
    os_task_handle_t event    = test_spawn("e", test_log_task, &log, 1U, OS_TASK_PERIOD_EVENT);

    (void)os_task_notify(event);
    test_run_until(50U);
    (void)os_task_suspend(periodic);
    (void)os_task_resume(periodic);
    (void)os_task_delete(event);
    test_run_until(100U);

    TEST_ASSERT_EQ(0, os_port_sim_critical_nesting());
}

void test_scheduler_suite(void)
{
    test_framework_begin_suite("scheduler");
    RUN_TEST(test_idle_scheduler_runs_nothing);
    RUN_TEST(test_periodic_task_follows_its_period);
    RUN_TEST(test_start_delay_offsets_first_release);
    RUN_TEST(test_highest_priority_runs_first);
    RUN_TEST(test_equal_priorities_run_in_fifo_order);
    RUN_TEST(test_suspended_task_does_not_run);
    RUN_TEST(test_task_can_delete_itself);
    RUN_TEST(test_task_can_suspend_itself);
    RUN_TEST(test_event_task_runs_only_when_notified);
    RUN_TEST(test_notify_while_running_requeues_task);
    RUN_TEST(test_notify_of_suspended_task_is_kept_until_resume);
    RUN_TEST(test_overrun_is_detected_and_period_realigned);
    RUN_TEST(test_periodic_timing_survives_tick_wraparound);
    RUN_TEST(test_start_returns_after_stop);
    RUN_TEST(test_critical_sections_are_balanced);
}
