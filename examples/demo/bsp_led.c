/**
 * @file    bsp_led.c
 * @brief   Simulated status LED: a GPIO output modelled as a boolean.
 */
#include "bsp_led.h"

static bool s_led_on;

void bsp_led_init(void)
{
    s_led_on = false;
}

void bsp_led_toggle(void)
{
    s_led_on = !s_led_on;
}

bool bsp_led_is_on(void)
{
    return s_led_on;
}
