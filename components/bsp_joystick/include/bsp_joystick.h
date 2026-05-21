/**
 * @file bsp_joystick.h
 * @brief ESP-IDF backed driver for the analog joystick.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Processed joystick sample used by the application layer. */
typedef struct {
    int x_raw;       /**< Raw ADC reading for X axis. */
    int y_raw;       /**< Raw ADC reading for Y axis. */
    int x_percent;   /**< X axis command after filtering, from -100 to 100. */
    int y_percent;   /**< Y axis command after filtering, from -100 to 100. */
} bsp_joystick_sample_t;

/** Static joystick driver configuration. */
typedef struct {
    adc_channel_t x_channel;     /**< ADC channel for X axis. */
    adc_channel_t y_channel;     /**< ADC channel for Y axis. */
    int min_raw;                 /**< Minimum expected raw ADC value. */
    int max_raw;                 /**< Maximum expected raw ADC value. */
    int center_raw;              /**< Resting raw ADC value. */
    int deadzone_raw;            /**< Center deadzone in raw ADC counts. */
    float alpha_slow;            /**< EMA factor used for small variations. */
    float alpha_fast;            /**< EMA factor used for abrupt variations. */
    int fast_threshold_raw;      /**< Raw delta that switches to alpha_fast. */
} bsp_joystick_config_t;

/** Joystick driver instance. Allocate it in the caller scope. */
typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    bsp_joystick_config_t config;
    bool filter_initialized;
    float x_filtered;
    float y_filtered;
} bsp_joystick_t;

/**
 * @brief Fill a joystick configuration with board defaults.
 *
 * @return Default configuration for the current board.
 */
bsp_joystick_config_t bsp_joystick_default_config(void);

/**
 * @brief Initialize ADC resources used by the joystick.
 *
 * @param joystick Driver instance to initialize.
 * @param config Driver configuration.
 * @return ESP_OK on success.
 */
esp_err_t bsp_joystick_init(bsp_joystick_t *joystick, const bsp_joystick_config_t *config);

/**
 * @brief Read, filter, and normalize both joystick axes.
 *
 * @param joystick Initialized joystick instance.
 * @param sample Output sample.
 * @return ESP_OK on success.
 */
esp_err_t bsp_joystick_read(bsp_joystick_t *joystick, bsp_joystick_sample_t *sample);

#ifdef __cplusplus
}
#endif
