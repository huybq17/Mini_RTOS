/**
 * @file    os_types.h
 * @brief   Basic types shared by every Mini RTOS module.
 */
#ifndef OS_TYPES_H
#define OS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * System time in milliseconds since os_scheduler_init().
 * Wraps around after ~49.7 days; the kernel compares ticks wrap-safely.
 */
typedef uint32_t os_tick_t;

/** Task priority: a higher value means a more important task. */
typedef uint8_t os_priority_t;

/** Result code returned by every fallible kernel API. */
typedef enum {
    OS_OK = 0,            /**< Success. */
    OS_ERR_INVALID_ARG,   /**< NULL pointer, bad value or stale handle. */
    OS_ERR_NO_MEMORY,     /**< Static object pool exhausted. */
    OS_ERR_INVALID_STATE, /**< Operation not allowed in the object's current state. */
    OS_ERR_QUEUE_FULL,    /**< No free slot in the queue. */
    OS_ERR_QUEUE_EMPTY    /**< No item available in the queue. */
} os_status_t;

/**
 * @brief  Human-readable name of a status code, for logs.
 * @return Static string, never NULL.
 */
const char *os_status_to_string(os_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* OS_TYPES_H */
