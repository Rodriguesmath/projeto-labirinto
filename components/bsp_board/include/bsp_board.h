/**
 * @file bsp_board.h
 * @brief Central pinout and electrical configuration for the labyrinth table.
 *
 * This board support package keeps ESP-IDF and board-specific constants out of
 * the application layer. Change this file when the wiring changes.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ADC channel connected to joystick X axis. ESP32 ADC1 channel 4 is GPIO32. */
#define BSP_JOYSTICK_X_CHANNEL ADC_CHANNEL_4

/** ADC channel connected to joystick Y axis. ESP32 ADC1 channel 5 is GPIO33. */
#define BSP_JOYSTICK_Y_CHANNEL ADC_CHANNEL_5

/** GPIO that drives the X-axis servo PWM signal. */
#define BSP_SERVO_X_GPIO GPIO_NUM_18

/** GPIO that drives the Y-axis servo PWM signal. */
#define BSP_SERVO_Y_GPIO GPIO_NUM_19

/** GPIO used by the "system ready" status LED. */
#define BSP_STATUS_LED_GPIO GPIO_NUM_2

/** LEDC timer used by both servo PWM channels. */
#define BSP_SERVO_LEDC_TIMER LEDC_TIMER_0

/** LEDC speed mode used by the servo PWM channels. */
#define BSP_SERVO_LEDC_MODE LEDC_LOW_SPEED_MODE

/** LEDC channel assigned to the X-axis servo. */
#define BSP_SERVO_X_LEDC_CHANNEL LEDC_CHANNEL_0

/** LEDC channel assigned to the Y-axis servo. */
#define BSP_SERVO_Y_LEDC_CHANNEL LEDC_CHANNEL_1

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
