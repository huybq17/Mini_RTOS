/**
 * @file    bsp_temp_sensor.c
 * @brief   Simulated temperature sensor producing a deterministic triangle wave.
 */
#include "bsp_temp_sensor.h"

#define TEMP_BASE_CENTI  2500
#define TEMP_STEP_CENTI  10
#define TEMP_WAVE_PERIOD 20U

static uint32_t s_sample_index;

void bsp_temp_sensor_init(void)
{
    s_sample_index = 0U;
}

int16_t bsp_temp_sensor_read_centi(void)
{
    const uint32_t phase = s_sample_index % TEMP_WAVE_PERIOD;
    const uint32_t half  = TEMP_WAVE_PERIOD / 2U;
    const int32_t  ramp  = (phase < half) ? (int32_t)phase : (int32_t)(TEMP_WAVE_PERIOD - phase);

    s_sample_index++;
    return (int16_t)(TEMP_BASE_CENTI + (ramp * TEMP_STEP_CENTI));
}
