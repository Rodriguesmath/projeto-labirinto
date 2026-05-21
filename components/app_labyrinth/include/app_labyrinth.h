/**
 * @file app_labyrinth.h
 * @brief Application orchestration for phase 1 of the joystick labyrinth table.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize hardware, create queues, and start the three phase-1 tasks.
 *
 * Task layout:
 * - Joystick task: reads and filters joystick X/Y.
 * - Servo task: converts joystick commands to PWM commands.
 * - Status task: reports debug data through the serial log.
 *
 * @return ESP_OK when all tasks are created.
 */
esp_err_t app_labyrinth_start(void);

#ifdef __cplusplus
}
#endif
