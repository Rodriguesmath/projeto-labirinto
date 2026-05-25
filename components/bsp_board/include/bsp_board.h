/**
 * @file bsp_board.h
 * @brief Central pinout and electrical configuration for the labyrinth table.
 *
 * This board support package keeps ESP-IDF and board-specific constants out of
 * the application layer. Change this file when the wiring changes.
 */

#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ADC channel connected to joystick X axis. */
#define BSP_JOYSTICK_X_CHANNEL ADC_CHANNEL_6

/** ADC channel connected to joystick Y axis. */
#define BSP_JOYSTICK_Y_CHANNEL ADC_CHANNEL_8

/** GPIO that drives the X-axis servo PWM signal. */
#define BSP_SERVO_X_GPIO GPIO_NUM_4

/** GPIO that drives the Y-axis servo PWM signal. */
#define BSP_SERVO_Y_GPIO GPIO_NUM_5

/**
 * @brief Initialize basic board services.
 *
 * The current first delivery does not require global board startup beyond
 * component-specific initialization, but this hook gives the application a
 * stable entry point for future board setup.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_board_init(void);

#ifdef __cplusplus
}
#endif
