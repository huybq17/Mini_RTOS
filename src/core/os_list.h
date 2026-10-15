/**
 * @file    os_list.h
 * @brief   Intrusive circular doubly-linked list (kernel private).
 *
 * The node is embedded in the object it links (e.g. the task control block),
 * so linking never allocates and removal is O(1). The owning object is
 * recovered with OS_CONTAINER_OF().
 */
#ifndef OS_LIST_H
#define OS_LIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct os_list os_list_t;

typedef struct os_list_node {
    struct os_list_node *prev;
    struct os_list_node *next;
    os_list_t           *owner; /**< List the node is linked into, NULL when detached. */
} os_list_node_t;

struct os_list {
    os_list_node_t sentinel; /**< Dummy head: removes every empty-list special case. */
    size_t         size;
};

/** Strict ordering used by os_list_insert_sorted(): true if lhs goes before rhs. */
typedef bool (*os_list_less_fn_t)(const os_list_node_t *lhs, const os_list_node_t *rhs);

/** Recover a pointer to the structure that embeds @p ptr as field @p member. */
#define OS_CONTAINER_OF(ptr, type, member) \
    ((type *)(void *)((char *)(ptr) - offsetof(type, member)))

/** Const-correct variant of OS_CONTAINER_OF(). */
#define OS_CONTAINER_OF_CONST(ptr, type, member) \
    ((const type *)(const void *)((const char *)(ptr) - offsetof(type, member)))

void os_list_init(os_list_t *list);
void os_list_node_init(os_list_node_t *node);

bool   os_list_is_empty(const os_list_t *list);
size_t os_list_size(const os_list_t *list);
bool   os_list_node_is_linked(const os_list_node_t *node);

/** @return First node, or NULL if the list is empty. */
os_list_node_t *os_list_front(const os_list_t *list);

void os_list_push_back(os_list_t *list, os_list_node_t *node);

/** Insert keeping the list sorted; equal elements keep insertion (FIFO) order. O(n). */
void os_list_insert_sorted(os_list_t *list, os_list_node_t *node, os_list_less_fn_t less);

/** Unlink @p node from whatever list holds it. No-op on a detached node. O(1). */
void os_list_remove(os_list_node_t *node);

/** @return Removed first node, or NULL if the list is empty. */
os_list_node_t *os_list_pop_front(os_list_t *list);

#endif /* OS_LIST_H */
