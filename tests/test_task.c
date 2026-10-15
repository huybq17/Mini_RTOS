/**
 * @file    test_task.c
 * @brief   Task API tests: argument checks, pool management, state changes.
 */
#include <string.h>

#include "test_framework.h"
#include "test_helpers.h"
#include "test_suites.h"

static void noop_task(void *arg)
{
    (void)arg;
}

static void setup(void)
{
    (void)os_scheduler_init();
}

static void test_create_rejects_invalid_args(void)
{
    os_task_handle_t       handle = NULL;
    const os_task_config_t no_fn  = { "t", NULL, NULL, 1U, 100U, 0U };
    const os_task_config_t valid  = { "t", noop_task, NULL, 1U, 100U, 0U };

    setup();

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_create(NULL, &handle));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_create(&no_fn, &handle));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_create(&valid, NULL));
    TEST_ASSERT(handle == NULL);
}

static void test_pool_exhaustion_and_slot_reuse(void)
{
    os_task_handle_t handles[OS_CFG_MAX_TASKS];
    os_task_handle_t extra = NULL;

    setup();

    for (size_t i = 0U; i < OS_CFG_MAX_TASKS; ++i) {
        handles[i] = test_spawn("t", noop_task, NULL, 1U, 100U);
        TEST_ASSERT(handles[i] != NULL);
    }

    const os_task_config_t config = { "extra", noop_task, NULL, 1U, 100U, 0U };
    TEST_ASSERT_EQ(OS_ERR_NO_MEMORY, os_task_create(&config, &extra));

    TEST_ASSERT_EQ(OS_OK, os_task_delete(handles[3]));
    TEST_ASSERT_EQ(OS_OK, os_task_create(&config, &extra));
}

static void test_get_info_reports_configuration(void)
{
    os_task_info_t info;

    setup();
    os_task_handle_t task = test_spawn("sensor", noop_task, NULL, 7U, 250U);

    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT(strcmp(info.name, "sensor") == 0);
    TEST_ASSERT_EQ(OS_TASK_STATE_WAITING, info.state);
    TEST_ASSERT_EQ(7, info.priority);
    TEST_ASSERT_EQ(250, info.period_ms);
    TEST_ASSERT_EQ(0, info.run_count);
    TEST_ASSERT_EQ(0, info.overrun_count);
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_get_info(task, NULL));
}

static void test_long_name_is_truncated(void)
{
    os_task_info_t info;

    setup();
    os_task_handle_t task =
        test_spawn("a_task_name_that_is_far_too_long", noop_task, NULL, 1U, 100U);

    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT_EQ(OS_CFG_TASK_NAME_LEN - 1U, strlen(info.name));
}

static void test_deleted_handle_is_rejected(void)
{
    setup();
    os_task_handle_t task = test_spawn("t", noop_task, NULL, 1U, 100U);

    TEST_ASSERT_EQ(OS_OK, os_task_delete(task));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_delete(task));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_suspend(task));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_resume(task));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_notify(task));
}

static void test_foreign_pointer_is_rejected(void)
{
    int dummy = 0;

    setup();

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_suspend((os_task_handle_t)(void *)&dummy));
}

static void test_null_handle_outside_task_is_rejected(void)
{
    setup();

    TEST_ASSERT(os_task_get_current() == NULL);
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_delete(NULL));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_task_suspend(NULL));
}

static void test_suspend_resume_state_machine(void)
{
    os_task_info_t info;

    setup();
    os_task_handle_t task = test_spawn("t", noop_task, NULL, 1U, 100U);

    TEST_ASSERT_EQ(OS_ERR_INVALID_STATE, os_task_resume(task));

    TEST_ASSERT_EQ(OS_OK, os_task_suspend(task));
    TEST_ASSERT_EQ(OS_OK, os_task_suspend(task)); /* Idempotent. */
    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT_EQ(OS_TASK_STATE_SUSPENDED, info.state);

    TEST_ASSERT_EQ(OS_OK, os_task_resume(task));
    TEST_ASSERT_EQ(OS_OK, os_task_get_info(task, &info));
    TEST_ASSERT_EQ(OS_TASK_STATE_WAITING, info.state);
}

void test_task_suite(void)
{
    test_framework_begin_suite("task");
    RUN_TEST(test_create_rejects_invalid_args);
    RUN_TEST(test_pool_exhaustion_and_slot_reuse);
    RUN_TEST(test_get_info_reports_configuration);
    RUN_TEST(test_long_name_is_truncated);
    RUN_TEST(test_deleted_handle_is_rejected);
    RUN_TEST(test_foreign_pointer_is_rejected);
    RUN_TEST(test_null_handle_outside_task_is_rejected);
    RUN_TEST(test_suspend_resume_state_machine);
}
