/**
 * @file    app_messages.h
 * @brief   Message types exchanged between demo tasks.
 *
 * Frames live in a memory pool and only their address travels through the
 * queue (zero-copy). Ownership moves with the pointer: the sender must not
 * touch a frame after sending it, and the receiver must free it.
 */
#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include <stdint.h>

#include "os.h"

#define SENSOR_FRAME_SAMPLES 16U

/** Burst of sensor samples, sent by the sensor task to the logger task. */
typedef struct {
    uint32_t  seq;          /**< Frame sequence number. */
    os_tick_t timestamp_ms; /**< Tick at which the burst was captured. */
    int16_t   samples_centi[SENSOR_FRAME_SAMPLES]; /**< Hundredths of a degree Celsius. */
} sensor_frame_t;

#endif /* APP_MESSAGES_H */
