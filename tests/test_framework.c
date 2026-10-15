/**
 * @file    test_framework.c
 * @brief   Minimal dependency-free unit test framework.
 */
#include "test_framework.h"

#include <stdio.h>

static bool         s_current_failed;
static unsigned int s_tests_run;
static unsigned int s_tests_failed;
static unsigned int s_checks;

void test_framework_begin_suite(const char *name)
{
    (void)printf("\n[%s]\n", name);
}

void test_framework_run(const char *name, test_fn_t fn)
{
    s_current_failed = false;
    s_tests_run++;

    fn();

    if (s_current_failed) {
        s_tests_failed++;
        (void)printf("  FAIL  %s\n", name);
    } else {
        (void)printf("  pass  %s\n", name);
    }
}

bool test_framework_check(bool condition, const char *expr, const char *file, int line)
{
    s_checks++;
    if (!condition) {
        s_current_failed = true;
        (void)printf("        %s:%d: assertion failed: %s\n", file, line, expr);
    }
    return condition;
}

bool test_framework_check_eq(long long expected, long long actual, const char *expected_expr,
                             const char *actual_expr, const char *file, int line)
{
    s_checks++;
    if (expected != actual) {
        s_current_failed = true;
        (void)printf("        %s:%d: expected %s == %s (%lld), got %lld\n", file, line,
                     actual_expr, expected_expr, expected, actual);
        return false;
    }
    return true;
}

unsigned int test_framework_report(void)
{
    (void)printf("\n%u tests, %u checks, %u failed\n", s_tests_run, s_checks, s_tests_failed);
    return s_tests_failed;
}
