/**
 * @file    os_tick.h
 * @brief   Wrap-safe tick comparisons (kernel private).
 *
 * os_tick_t is a free-running 32-bit counter that wraps every ~49.7 days.
 * Comparing two ticks with `<` breaks at the wrap; comparing the signed
 * difference is correct as long as both ticks are less than 2^31 ms apart.
 */
#ifndef OS_TICK_H
#define OS_TICK_H

#include <stdbool.h>
#include <stdint.h>

#include "os_types.h"

/** @return true if @p now is at or past @p deadline. */
static inline bool os_tick_reached(os_tick_t now, os_tick_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

/** @return true if @p a is strictly later than @p b. */
static inline bool os_tick_after(os_tick_t a, os_tick_t b)
{
    return (int32_t)(a - b) > 0;
}

#endif /* OS_TICK_H */
