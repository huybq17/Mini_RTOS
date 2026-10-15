/**
 * @file    os_port_sim.c
 * @brief   Simulation port with a virtual clock, for deterministic tests.
 */
#include "os_port_sim.h"

#include "os_config.h"

static os_tick_t    s_tick;
static unsigned int s_critical_nesting;

void os_port_init(void)
{
    s_tick             = 0U;
    s_critical_nesting = 0U;
}

os_tick_t os_port_get_tick(void)
{
    return s_tick;
}

void os_port_idle(os_tick_t max_sleep_ms)
{
    /* Sleeping in virtual time is just jumping the clock forward. */
    s_tick += max_sleep_ms;
}

void os_port_enter_critical(void)
{
    s_critical_nesting++;
}

void os_port_exit_critical(void)
{
    OS_ASSERT(s_critical_nesting > 0U);
    s_critical_nesting--;
}

void os_port_sim_set_tick(os_tick_t tick)
{
    s_tick = tick;
}

void os_port_sim_advance(os_tick_t delta_ms)
{
    s_tick += delta_ms;
}

unsigned int os_port_sim_critical_nesting(void)
{
    return s_critical_nesting;
}
