/**
 * @file    test_list.c
 * @brief   White-box tests of the intrusive list (kernel private module).
 */
#include "os_list.h"
#include "test_framework.h"
#include "test_suites.h"

typedef struct {
    int            key;
    char           tag;
    os_list_node_t node;
} item_t;

static bool item_less(const os_list_node_t *lhs, const os_list_node_t *rhs)
{
    return OS_CONTAINER_OF_CONST(lhs, item_t, node)->key <
           OS_CONTAINER_OF_CONST(rhs, item_t, node)->key;
}

static char item_tag(os_list_node_t *node)
{
    return OS_CONTAINER_OF(node, item_t, node)->tag;
}

static void test_new_list_is_empty(void)
{
    os_list_t list;

    os_list_init(&list);

    TEST_ASSERT(os_list_is_empty(&list));
    TEST_ASSERT_EQ(0, os_list_size(&list));
    TEST_ASSERT(os_list_front(&list) == NULL);
    TEST_ASSERT(os_list_pop_front(&list) == NULL);
}

static void test_push_back_then_pop_front_is_fifo(void)
{
    os_list_t list;
    item_t    a = { 0, 'a', { NULL, NULL, NULL } };
    item_t    b = { 0, 'b', { NULL, NULL, NULL } };
    item_t    c = { 0, 'c', { NULL, NULL, NULL } };

    os_list_init(&list);
    os_list_push_back(&list, &a.node);
    os_list_push_back(&list, &b.node);
    os_list_push_back(&list, &c.node);

    TEST_ASSERT_EQ(3, os_list_size(&list));
    TEST_ASSERT_EQ('a', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT_EQ('b', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT_EQ('c', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT(os_list_is_empty(&list));
}

static void test_remove_detaches_node(void)
{
    os_list_t list;
    item_t    a = { 0, 'a', { NULL, NULL, NULL } };
    item_t    b = { 0, 'b', { NULL, NULL, NULL } };
    item_t    c = { 0, 'c', { NULL, NULL, NULL } };

    os_list_init(&list);
    os_list_push_back(&list, &a.node);
    os_list_push_back(&list, &b.node);
    os_list_push_back(&list, &c.node);

    os_list_remove(&b.node);

    TEST_ASSERT(!os_list_node_is_linked(&b.node));
    TEST_ASSERT_EQ(2, os_list_size(&list));
    TEST_ASSERT_EQ('a', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT_EQ('c', item_tag(os_list_pop_front(&list)));

    os_list_remove(&b.node); /* Removing a detached node is a no-op. */
    TEST_ASSERT(os_list_is_empty(&list));
}

static void test_insert_sorted_is_stable(void)
{
    os_list_t list;
    item_t    first_three  = { 3, 'x', { NULL, NULL, NULL } };
    item_t    one          = { 1, 'a', { NULL, NULL, NULL } };
    item_t    second_three = { 3, 'y', { NULL, NULL, NULL } };
    item_t    two          = { 2, 'b', { NULL, NULL, NULL } };

    os_list_init(&list);
    os_list_insert_sorted(&list, &first_three.node, item_less);
    os_list_insert_sorted(&list, &one.node, item_less);
    os_list_insert_sorted(&list, &second_three.node, item_less);
    os_list_insert_sorted(&list, &two.node, item_less);

    TEST_ASSERT_EQ('a', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT_EQ('b', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT_EQ('x', item_tag(os_list_pop_front(&list)));
    TEST_ASSERT_EQ('y', item_tag(os_list_pop_front(&list)));
}

void test_list_suite(void)
{
    test_framework_begin_suite("list");
    RUN_TEST(test_new_list_is_empty);
    RUN_TEST(test_push_back_then_pop_front_is_fifo);
    RUN_TEST(test_remove_detaches_node);
    RUN_TEST(test_insert_sorted_is_stable);
}
