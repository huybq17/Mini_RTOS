/**
 * @file    os_port.h
 * @brief   Hardware abstraction required by the kernel.
 *
 * The kernel core is platform independent. Each target provides one
 * implementation of this interface:
 *   - src/port/host : Linux / macOS / Windows process (real time).
 *   - src/port/sim  : virtual clock driven by unit tests (deterministic).
 * A microcontroller port would use SysTick and disable/enable interrupts.
 */
#ifndef OS_PORT_H
#define OS_PORT_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialise the time base. Tick 0 is the moment of this call. */
void os_port_init(void);

/** @return Monotonic time in milliseconds. */
os_tick_t os_port_get_tick(void);

/** @brief Idle the CPU for at most @p max_sleep_ms (sleep, WFI, ...). */
void os_port_idle(os_tick_t max_sleep_ms);

/** @brief Enter a critical section. Calls may be nested. */
void os_port_enter_critical(void);

/** @brief Leave a critical section entered with os_port_enter_critical(). */
void os_port_exit_critical(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_PORT_H */
