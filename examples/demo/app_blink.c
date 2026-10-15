/**
 * @file    app_blink.c
 * @brief   Periodic task toggling the status LED.
 */
#include "app_blink.h"

#include "app_log.h"
#include "bsp_led.h"

void app_blink_task(void *arg)
{
    (void)arg;

    bsp_led_toggle();
    app_log("BLINK", "LED %s", bsp_led_is_on() ? "ON" : "OFF");
}
