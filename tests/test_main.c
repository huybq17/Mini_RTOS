/**
 * @file    test_main.c
 * @brief   Unit test runner.
 */
#include <stdlib.h>

#include "test_framework.h"
#include "test_suites.h"

int main(void)
{
    test_list_suite();
    test_task_suite();
    test_scheduler_suite();
    test_queue_suite();
    test_mempool_suite();

    return (test_framework_report() == 0U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
