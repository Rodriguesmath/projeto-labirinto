/**
 * @file bsp_victory_sensor.h
 * @brief ESP-IDF GPIO driver for the optical reflection sensor and victory LED.
 *
 * This board support package component abstracts the optical reflection sensor
 * used to detect the presence of a metallic ball at the labyrinth exit, and
 * the LED that signals the victory condition.
 *
 * Detection logic:
 * - The sensor outputs a low level when a highly reflective surface (metallic
 *   or chrome ball) is present in front of it.
 * - The sensor outputs a high level when no reflective surface is detected.
 *
 * All GPIO pin assignments are centralised in bsp_board.h.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Static configuration for the victory detection subsystem. */
typedef struct {
    gpio_num_t sensor_gpio;   /**< GPIO connected to the optical sensor output. */
    gpio_num_t led_gpio;      /**< GPIO connected to the victory indicator LED. */
} bsp_victory_sensor_config_t;

/** Victory sensor driver instance.  Allocate it in the caller scope. */
typedef struct {
    bsp_victory_sensor_config_t config; /**< Copy of the configuration used at init. */
} bsp_victory_sensor_t;

/**
 * @brief Fill a victory sensor configuration with board defaults.
 *
 * The returned structure references the GPIO constants defined in bsp_board.h,
 * keeping pin assignments centralised in a single place.
 *
 * @return Default configuration for the current board.
 */
bsp_victory_sensor_config_t bsp_victory_sensor_default_config(void);

/**
 * @brief Initialize the optical sensor input and the victory LED output.
 *
 * Configures the sensor GPIO as a digital input with an internal pull-up
 * resistor and the LED GPIO as a push-pull output starting in the off state.
 *
 * @param sensor Driver instance to initialize.
 * @param config Driver configuration.
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
esp_err_t bsp_victory_sensor_init(bsp_victory_sensor_t *sensor,
                                  const bsp_victory_sensor_config_t *config);

/**
 * @brief Read the optical sensor and report whether a ball is detected.
 *
 * A low GPIO level produced by the sensor is interpreted as a highly
 * reflective surface (metallic ball) being present.  The result is returned
 * as a plain boolean so the caller is shielded from the active-low polarity.
 *
 * @param sensor      Initialized driver instance.
 * @param detected    Set to true when a ball is detected, false otherwise.
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
esp_err_t bsp_victory_sensor_read(bsp_victory_sensor_t *sensor, bool *detected);

/**
 * @brief Turn the victory indicator LED on or off.
 *
 * @param sensor  Initialized driver instance.
 * @param on      true to light the LED, false to turn it off.
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
esp_err_t bsp_victory_sensor_set_led(bsp_victory_sensor_t *sensor, bool on);

#ifdef __cplusplus
}
#endif
