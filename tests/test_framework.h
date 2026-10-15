/**
 * @file    test_framework.h
 * @brief   Minimal dependency-free unit test framework.
 *
 * An assertion failure prints its location and returns from the test
 * function, so later assertions of the same test are skipped.
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>

typedef void (*test_fn_t)(void);

void test_framework_begin_suite(const char *name);
void test_framework_run(const char *name, test_fn_t fn);
bool test_framework_check(bool condition, const char *expr, const char *file, int line);
bool test_framework_check_eq(long long expected, long long actual, const char *expected_expr,
                             const char *actual_expr, const char *file, int line);

/** Print the summary. @return Number of failed tests. */
unsigned int test_framework_report(void);

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!test_framework_check((cond), #cond, __FILE__, __LINE__)) {     \
            return;                                                         \
        }                                                                   \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual)                                                 \
    do {                                                                                 \
        if (!test_framework_check_eq((long long)(expected), (long long)(actual),         \
                                     #expected, #actual, __FILE__, __LINE__)) {          \
            return;                                                                      \
        }                                                                                \
    } while (0)

#define RUN_TEST(fn) test_framework_run(#fn, fn)

#endif /* TEST_FRAMEWORK_H */
