/**
 * @file    os_port_sim.h
 * @brief   Extra controls of the simulation port, used by unit tests.
 *
 * The simulation port has a virtual clock: time only moves when a test
 * advances it or when the scheduler idles, which makes every test
 * deterministic and instantaneous.
 */
#ifndef OS_PORT_SIM_H
#define OS_PORT_SIM_H

#include "os_port.h"

/** Set the virtual clock, e.g. just before the 32-bit wrap-around. */
void os_port_sim_set_tick(os_tick_t tick);

/** Move the virtual clock forward. */
void os_port_sim_advance(os_tick_t delta_ms);

/** @return Current critical section nesting depth (0 when balanced). */
unsigned int os_port_sim_critical_nesting(void);

#endif /* OS_PORT_SIM_H */
