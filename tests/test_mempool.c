/**
 * @file    test_mempool.c
 * @brief   Memory pool tests: geometry, O(1) alloc/free, misuse detection,
 *          zero-copy hand-over through a queue.
 */
#include <stdint.h>
#include <string.h>

#include "test_framework.h"
#include "test_helpers.h"
#include "test_suites.h"

#define BLOCK_REQUEST 10U
#define BLOCK_COUNT   4U

static OS_MEMPOOL_DECLARE_STORAGE(s_storage, BLOCK_REQUEST, BLOCK_COUNT);

static void setup(void)
{
    (void)os_scheduler_init();
    memset(s_storage, 0, sizeof(s_storage));
}

static os_mempool_handle_t create_pool(void)
{
    os_mempool_handle_t pool = NULL;

    (void)os_mempool_create(s_storage, sizeof(s_storage), BLOCK_REQUEST, &pool);
    return pool;
}

static void test_create_rejects_invalid_args(void)
{
    os_mempool_handle_t pool     = NULL;
    uint8_t            *bytes    = (uint8_t *)(void *)s_storage;
    const size_t        one_block = OS_MEMPOOL_BLOCK_SIZE(BLOCK_REQUEST);

    setup();

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_create(NULL, 64U, 8U, &pool));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_create(s_storage, sizeof(s_storage), 0U, &pool));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_create(s_storage, sizeof(s_storage), 8U, NULL));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG,
                   os_mempool_create(bytes + 1, sizeof(s_storage) - 1U, 8U, &pool));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG,
                   os_mempool_create(s_storage, one_block - 1U, BLOCK_REQUEST, &pool));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG,
                   os_mempool_create(s_storage, sizeof(s_storage), SIZE_MAX, &pool));
    TEST_ASSERT(pool == NULL);
}

static void test_geometry_is_rounded_and_derived(void)
{
    os_mempool_info_t info;

    setup();
    os_mempool_handle_t pool = create_pool();

    TEST_ASSERT_EQ(OS_OK, os_mempool_get_info(pool, &info));
    TEST_ASSERT_EQ(OS_MEMPOOL_BLOCK_SIZE(BLOCK_REQUEST), info.block_size);
    TEST_ASSERT_EQ(0, info.block_size % OS_MEMPOOL_ALIGN);
    TEST_ASSERT(info.block_size >= BLOCK_REQUEST);
    TEST_ASSERT_EQ(BLOCK_COUNT, info.block_count);
    TEST_ASSERT_EQ(BLOCK_COUNT, info.free_count);
    TEST_ASSERT_EQ(BLOCK_COUNT, info.min_free_count);
}

static void test_alloc_until_exhausted_then_reuse(void)
{
    void             *blocks[BLOCK_COUNT];
    const uintptr_t   first = (uintptr_t)s_storage;
    const uintptr_t   end   = first + sizeof(s_storage);
    os_mempool_info_t info;

    setup();
    os_mempool_handle_t pool = create_pool();

    for (size_t i = 0U; i < BLOCK_COUNT; ++i) {
        blocks[i] = os_mempool_alloc(pool);
        TEST_ASSERT(blocks[i] != NULL);
        TEST_ASSERT(((uintptr_t)blocks[i] >= first) && ((uintptr_t)blocks[i] < end));
        TEST_ASSERT_EQ(0, (uintptr_t)blocks[i] % OS_MEMPOOL_ALIGN);
        for (size_t j = 0U; j < i; ++j) {
            TEST_ASSERT(blocks[i] != blocks[j]);
        }
    }
    TEST_ASSERT(os_mempool_alloc(pool) == NULL);

    /* LIFO free list: the block freed last is handed out first (cache friendly). */
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, blocks[2]));
    TEST_ASSERT(os_mempool_alloc(pool) == blocks[2]);

    TEST_ASSERT_EQ(OS_OK, os_mempool_get_info(pool, &info));
    TEST_ASSERT_EQ(0, info.free_count);
    TEST_ASSERT_EQ(0, info.min_free_count);
}

static void test_blocks_do_not_overlap(void)
{
    setup();
    os_mempool_handle_t pool = create_pool();

    uint8_t *a = os_mempool_alloc(pool);
    uint8_t *b = os_mempool_alloc(pool);
    TEST_ASSERT((a != NULL) && (b != NULL));

    memset(a, 0xAA, BLOCK_REQUEST);
    memset(b, 0x55, BLOCK_REQUEST);
    for (size_t i = 0U; i < BLOCK_REQUEST; ++i) {
        TEST_ASSERT_EQ(0xAA, a[i]);
    }
}

static void test_free_rejects_foreign_pointers(void)
{
    uint32_t outside = 0U;

    setup();
    os_mempool_handle_t pool  = create_pool();
    uint8_t            *block = os_mempool_alloc(pool);

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_free(pool, NULL));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_free(pool, &outside));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_free(pool, block + 1));
    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_free(NULL, block));
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, block));
}

static void test_double_free_is_detected(void)
{
    os_mempool_info_t info;

    setup();
    os_mempool_handle_t pool  = create_pool();
    void               *block = os_mempool_alloc(pool);

    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, block));
    TEST_ASSERT_EQ(OS_ERR_INVALID_STATE, os_mempool_free(pool, block));

    TEST_ASSERT_EQ(OS_OK, os_mempool_get_info(pool, &info));
    TEST_ASSERT_EQ(BLOCK_COUNT, info.free_count); /* Free list not corrupted. */
}

static void test_user_data_looking_like_free_marker_is_accepted(void)
{
    setup();
    os_mempool_handle_t pool = create_pool();
    void               *a    = os_mempool_alloc(pool);
    void               *b    = os_mempool_alloc(pool);

    /* Copy b's free-list header (link + marker) into a, which is still allocated. */
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, b));
    memcpy(a, b, OS_MEMPOOL_MIN_BLOCK_SIZE);

    /* The marker matches, but a is not in the free list: this is a valid free. */
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, a));
}

static void test_watermark_tracks_peak_usage(void)
{
    os_mempool_info_t info;

    setup();
    os_mempool_handle_t pool = create_pool();

    void *a = os_mempool_alloc(pool);
    void *b = os_mempool_alloc(pool);
    void *c = os_mempool_alloc(pool);
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, a));
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, b));
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, c));

    TEST_ASSERT_EQ(OS_OK, os_mempool_get_info(pool, &info));
    TEST_ASSERT_EQ(BLOCK_COUNT, info.free_count);
    TEST_ASSERT_EQ(BLOCK_COUNT - 3U, info.min_free_count);
}

static void test_delete_requires_all_blocks_returned(void)
{
    setup();
    os_mempool_handle_t pool  = create_pool();
    void               *block = os_mempool_alloc(pool);

    TEST_ASSERT_EQ(OS_ERR_INVALID_STATE, os_mempool_delete(pool));
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(pool, block));
    TEST_ASSERT_EQ(OS_OK, os_mempool_delete(pool));

    TEST_ASSERT_EQ(OS_ERR_INVALID_ARG, os_mempool_delete(pool));
    TEST_ASSERT(os_mempool_alloc(pool) == NULL);
}

static void test_control_block_pool_exhaustion(void)
{
    os_mempool_handle_t pools[OS_CFG_MAX_MEMPOOLS];
    os_mempool_handle_t extra = NULL;

    setup();

    for (size_t i = 0U; i < OS_CFG_MAX_MEMPOOLS; ++i) {
        pools[i] = create_pool();
        TEST_ASSERT(pools[i] != NULL);
    }
    TEST_ASSERT_EQ(OS_ERR_NO_MEMORY,
                   os_mempool_create(s_storage, sizeof(s_storage), BLOCK_REQUEST, &extra));

    TEST_ASSERT_EQ(OS_OK, os_mempool_delete(pools[1]));
    TEST_ASSERT_EQ(OS_OK, os_mempool_create(s_storage, sizeof(s_storage), BLOCK_REQUEST, &extra));
}

static void test_zero_copy_hand_over_through_queue(void)
{
    typedef struct {
        uint32_t seq;
        uint8_t  payload[8];
    } frame_t;

    static OS_MEMPOOL_DECLARE_STORAGE(frame_storage, sizeof(frame_t), 2U);
    frame_t            *queue_storage[2];
    os_mempool_handle_t frames = NULL;
    os_queue_handle_t   queue  = NULL;
    frame_t            *received = NULL;

    setup();
    TEST_ASSERT_EQ(OS_OK,
                   os_mempool_create(frame_storage, sizeof(frame_storage), sizeof(frame_t), &frames));
    TEST_ASSERT_EQ(OS_OK, os_queue_create(queue_storage, sizeof(frame_t *), 2U, &queue));

    /* Producer: fill a block, send only its address. */
    frame_t *sent = os_mempool_alloc(frames);
    TEST_ASSERT(sent != NULL);
    sent->seq = 7U;
    memset(sent->payload, 0x5A, sizeof(sent->payload));
    TEST_ASSERT_EQ(OS_OK, os_queue_send(queue, &sent));

    /* Consumer: gets the very same block, then releases it. */
    TEST_ASSERT_EQ(OS_OK, os_queue_receive(queue, &received));
    TEST_ASSERT(received == sent);
    TEST_ASSERT_EQ(7, received->seq);
    TEST_ASSERT_EQ(0x5A, received->payload[7]);
    TEST_ASSERT_EQ(OS_OK, os_mempool_free(frames, received));
    TEST_ASSERT_EQ(OS_OK, os_mempool_delete(frames));
}

static void test_critical_sections_are_balanced(void)
{
    setup();
    os_mempool_handle_t pool  = create_pool();
    void               *block = os_mempool_alloc(pool);

    (void)os_mempool_free(pool, block);
    (void)os_mempool_free(pool, block);
    (void)os_mempool_delete(pool);

    TEST_ASSERT_EQ(0, os_port_sim_critical_nesting());
}

void test_mempool_suite(void)
{
    test_framework_begin_suite("mempool");
    RUN_TEST(test_create_rejects_invalid_args);
    RUN_TEST(test_geometry_is_rounded_and_derived);
    RUN_TEST(test_alloc_until_exhausted_then_reuse);
    RUN_TEST(test_blocks_do_not_overlap);
    RUN_TEST(test_free_rejects_foreign_pointers);
    RUN_TEST(test_double_free_is_detected);
    RUN_TEST(test_user_data_looking_like_free_marker_is_accepted);
    RUN_TEST(test_watermark_tracks_peak_usage);
    RUN_TEST(test_delete_requires_all_blocks_returned);
    RUN_TEST(test_control_block_pool_exhaustion);
    RUN_TEST(test_zero_copy_hand_over_through_queue);
    RUN_TEST(test_critical_sections_are_balanced);
}
