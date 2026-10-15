/**
 * @file    bsp_led.h
 * @brief   Simulated status LED (board support package).
 */
#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdbool.h>

void bsp_led_init(void);
void bsp_led_toggle(void);
bool bsp_led_is_on(void);

#endif /* BSP_LED_H */
