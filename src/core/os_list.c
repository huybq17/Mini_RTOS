/**
 * @file    os_list.c
 * @brief   Intrusive circular doubly-linked list implementation.
 */
#include "os_list.h"

#include "os_config.h"

static void list_link_between(os_list_t *list, os_list_node_t *node, os_list_node_t *prev,
                              os_list_node_t *next)
{
    node->prev  = prev;
    node->next  = next;
    node->owner = list;
    prev->next  = node;
    next->prev  = node;
    list->size++;
}

void os_list_init(os_list_t *list)
{
    OS_ASSERT(list != NULL);

    list->sentinel.prev  = &list->sentinel;
    list->sentinel.next  = &list->sentinel;
    list->sentinel.owner = list;
    list->size           = 0U;
}

void os_list_node_init(os_list_node_t *node)
{
    OS_ASSERT(node != NULL);

    node->prev  = NULL;
    node->next  = NULL;
    node->owner = NULL;
}

bool os_list_is_empty(const os_list_t *list)
{
    return list->size == 0U;
}

size_t os_list_size(const os_list_t *list)
{
    return list->size;
}

bool os_list_node_is_linked(const os_list_node_t *node)
{
    return node->owner != NULL;
}

os_list_node_t *os_list_front(const os_list_t *list)
{
    return os_list_is_empty(list) ? NULL : list->sentinel.next;
}

void os_list_push_back(os_list_t *list, os_list_node_t *node)
{
    OS_ASSERT((list != NULL) && (node != NULL));
    OS_ASSERT(!os_list_node_is_linked(node));

    list_link_between(list, node, list->sentinel.prev, &list->sentinel);
}

void os_list_insert_sorted(os_list_t *list, os_list_node_t *node, os_list_less_fn_t less)
{
    OS_ASSERT((list != NULL) && (node != NULL) && (less != NULL));
    OS_ASSERT(!os_list_node_is_linked(node));

    os_list_node_t *pos = list->sentinel.next;

    /* Skip every element that does not go after node: equal keys stay FIFO. */
    while ((pos != &list->sentinel) && !less(node, pos)) {
        pos = pos->next;
    }

    list_link_between(list, node, pos->prev, pos);
}

void os_list_remove(os_list_node_t *node)
{
    OS_ASSERT(node != NULL);

    os_list_t *list = node->owner;
    if (list == NULL) {
        return;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
    list->size--;
    os_list_node_init(node);
}

os_list_node_t *os_list_pop_front(os_list_t *list)
{
    os_list_node_t *node = os_list_front(list);

    if (node != NULL) {
        os_list_remove(node);
    }
    return node;
}
