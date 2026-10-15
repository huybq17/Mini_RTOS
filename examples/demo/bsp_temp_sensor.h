/**
 * @file    bsp_temp_sensor.h
 * @brief   Simulated temperature sensor (board support package).
 */
#ifndef BSP_TEMP_SENSOR_H
#define BSP_TEMP_SENSOR_H

#include <stdint.h>

void bsp_temp_sensor_init(void);

/** @return Temperature in hundredths of a degree Celsius (2531 = 25.31 C). */
int16_t bsp_temp_sensor_read_centi(void);

#endif /* BSP_TEMP_SENSOR_H */
