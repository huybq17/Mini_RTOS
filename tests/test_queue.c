/**
 * @file    test_queue.c
 * @brief   Message queue tests: ring buffer, limits, receiver notification.
 */
#include <stdint.h>

#include "test_framework.h"
#include "test_helpers.h"
#include "test_suites.h"

#define QUEUE_CAPACITY 3U

static uint32_t          s_storage[QUEUE_CAPACITY];
static os_queue_handle_t s_drain_queue;
static uint32_t          s_drained_sum;
static uint32_t          s_drain_runs;

static void setup(void)
{
    (void)os_scheduler_init();
    s_drain_queue = NULL;
    s_drained_sum = 0U;
    s_drain_runs  = 0U;
}

/* Event-driven consumer: empties the queue on every wake-up. */
static void drain_task(void *arg)
{
    uint32_t value = 0U;

    (void)arg;
    s_drain_runs++;
    while (os_queue_receive(s_drain_queue, &value) == OS_OK) {
        s_drained_sum += value;
    }
}

static os_queue_handle_t create_queue(void)
{
    os_queue_handle_t queue = NULL;

    (void)os_queue_create(s_storage, sizeof(uint32_t), QUEUE_CAPACITY, &queue);
    return queue;
}

static void test_create_rejects_invalid_args(void)
{
    os_queue_handle_t queue = NULL;

    setup();

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_queue_create(NULL, 4U, 3U, &queue));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_queue_create(s_storage, 0U, 3U, &queue));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_queue_create(s_storage, 4U, 0U, &queue));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_queue_create(s_storage, 4U, 3U, NULL));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_queue_create(s_storage, 4U, SIZE_MAX, &queue));
    TEST_ASSERT(queue == NULL);
}

static void test_fifo_order_across_ring_wraparound(void)
{
    uint32_t value = 0U;

    setup();
    os_queue_handle_t queue = create_queue();
    TEST_ASSERT(queue != NULL);

    const uint32_t one = 1U, two = 2U, three = 3U, four = 4U, five = 5U;
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &one));
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &two));
    TEST_ASSERT_EQ(OS_OK, os_queue_receive(queue, &value));
    TEST_ASSERT_EQ(1, value);

    /* The tail wraps to index 0 here. */
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &three));
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &four));
    TEST_ASSERT_EQ(OS_ERR_QUEUE_FULL, os_queue_send(queue, &five));

    TEST_ASSERT_EQ(OS_OK, os_queue_receive(queue, &value));
    TEST_ASSERT_EQ(2, value);
    TEST_ASSERT_EQ(OS_OK, os_queue_receive(queue, &value));
    TEST_ASSERT_EQ(3, value);
    TEST_ASSERT_EQ(OS_OK, os_queue_receive(queue, &value));
    TEST_ASSERT_EQ(4, value);
    TEST_ASSERT_EQ(OS_ERR_QUEUE_EMPTY, os_queue_receive(queue, &value));
}

static void test_peek_does_not_remove(void)
{
    uint32_t       value = 0U;
    const uint32_t item  = 42U;

    setup();
    os_queue_handle_t queue = create_queue();

    TEST_ASSERT_EQ(OS_ERR_QUEUE_EMPTY, os_queue_peek(queue, &value));
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &item));
    TEST_ASSERT_EQ(OS_OK, os_queue_peek(queue, &value));
    TEST_ASSERT_EQ(42, value);
    TEST_ASSERT_EQ(1, os_queue_count(queue));
}

static void test_count_and_space(void)
{
    const uint32_t item = 7U;

    setup();
    os_queue_handle_t queue = create_queue();

    TEST_ASSERT_EQ(0, os_queue_count(queue));
    TEST_ASSERT_EQ(QUEUE_CAPACITY, os_queue_space(queue));
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &item));
    TEST_ASSERT_EQ(1, os_queue_count(queue));
    TEST_ASSERT_EQ(QUEUE_CAPACITY - 1U, os_queue_space(queue));
    TEST_ASSERT_EQ(0, os_queue_count(NULL));
}

static void test_send_wakes_receiver_task(void)
{
    const uint32_t items[] = { 10U, 20U, 30U };

    setup();
    s_drain_queue = create_queue();
    os_task_handle_t consumer = test_spawn("drain", drain_task, NULL, 1U, OS_TASK_PERIOD_EVENT);
    TEST_ASSERT_EQ(OS_OK, os_queue_set_receiver(s_drain_queue, consumer));

    TEST_ASSERT(!os_scheduler_run_once()); /* Nothing sent yet: consumer sleeps. */

    for (size_t i = 0U; i < (sizeof(items) / sizeof(items[0])); ++i) {
        TEST_ASSERT_EQ(OS_OK, os_queue_send(s_drain_queue, &items[i]));
    }

    TEST_ASSERT(os_scheduler_run_once());
    TEST_ASSERT(!os_scheduler_run_once());
    TEST_ASSERT_EQ(1, s_drain_runs);
    TEST_ASSERT_EQ(60, s_drained_sum);
    TEST_ASSERT_EQ(0, os_queue_count(s_drain_queue));
}

static void test_receiver_binding_dropped_on_task_delete(void)
{
    const uint32_t item = 1U;

    setup();
    s_drain_queue = create_queue();
    os_task_handle_t consumer = test_spawn("drain", drain_task, NULL, 1U, OS_TASK_PERIOD_EVENT);
    TEST_ASSERT_EQ(OS_OK, os_queue_set_receiver(s_drain_queue, consumer));
    TEST_ASSERT_EQ(OS_OK, os_task_delete(consumer));

    /* The new task reuses the freed slot but must not inherit the binding. */
    TEST_ASSERT(test_spawn("other", drain_task, NULL, 1U, OS_TASK_PERIOD_EVENT) != NULL);
    TEST_ASSERT_EQ(OS_OK, os_queue_send(s_drain_queue, &item));
    TEST_ASSERT(!os_scheduler_run_once());
    TEST_ASSERT_EQ(0, s_drain_runs);
}

static void test_set_receiver_rejects_invalid_task(void)
{
    int dummy = 0;

    setup();
    os_queue_handle_t queue = create_queue();

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG,
                   os_queue_set_receiver(queue, (os_task_handle_t)(void *)&dummy));
    TEST_ASSERT_EQ(OS_OK, os_queue_set_receiver(queue, NULL));
}

static void test_pool_exhaustion_and_slot_reuse(void)
{
    os_queue_handle_t queues[OS_CFG_MAX_QUEUES];
    os_queue_handle_t extra = NULL;

    setup();

    for (size_t i = 0U; i < OS_CFG_MAX_QUEUES; ++i) {
        queues[i] = create_queue();
        TEST_ASSERT(queues[i] != NULL);
    }
    TEST_ASSERT_EQ(OS_ERR_NO_MEMORY, os_queue_create(s_storage, 4U, 1U, &extra));

    TEST_ASSERT_EQ(OS_OK, os_queue_delete(queues[0]));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_queue_delete(queues[0]));
    TEST_ASSERT_EQ(OS_OK, os_queue_create(s_storage, 4U, 1U, &extra));
}

void test_queue_suite(void)
{
    test_framework_begin_suite("queue");
    RUN_TEST(test_create_rejects_invalid_args);
    RUN_TEST(test_fifo_order_across_ring_wraparound);
    RUN_TEST(test_peek_does_not_remove);
    RUN_TEST(test_count_and_space);
    RUN_TEST(test_send_wakes_receiver_task);
    RUN_TEST(test_receiver_binding_dropped_on_task_delete);
    RUN_TEST(test_set_receiver_rejects_invalid_task);
    RUN_TEST(test_pool_exhaustion_and_slot_reuse);
}
