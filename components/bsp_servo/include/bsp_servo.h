/**
 * @file bsp_servo.h
 * @brief Servo PWM driver for the two-axis labyrinth table.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Static configuration for one hobby servo. */
typedef struct {
    gpio_num_t gpio;                 /**< PWM output pin. */
    uint32_t min_pulse_us;           /**< Pulse width for -100 command. */
    uint32_t center_pulse_us;        /**< Pulse width for 0 command. */
    uint32_t max_pulse_us;           /**< Pulse width for +100 command. */
} bsp_servo_channel_config_t;

/** Driver instance for one servo channel. */
typedef struct {
    bsp_servo_channel_config_t config;
    mcpwm_oper_handle_t operator;
    mcpwm_cmpr_handle_t comparator;
    mcpwm_gen_handle_t generator;
    bool command_initialized;
    bool pulse_enabled;
    uint32_t last_pulse_us;
} bsp_servo_t;

/**
 * @brief Fill default X-axis servo configuration.
 *
 * @return Default X-axis servo configuration.
 */
bsp_servo_channel_config_t bsp_servo_x_default_config(void);

/**
 * @brief Fill default Y-axis servo configuration.
 *
 * @return Default Y-axis servo configuration.
 */
bsp_servo_channel_config_t bsp_servo_y_default_config(void);

/**
 * @brief Initialize the shared MCPWM timer used for 50 Hz servo control.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_servo_timer_init(void);

/**
 * @brief Initialize one servo PWM channel.
 *
 * @param servo Driver instance to initialize.
 * @param config Servo channel configuration.
 * @return ESP_OK on success.
 */
esp_err_t bsp_servo_init(bsp_servo_t *servo, const bsp_servo_channel_config_t *config);

/**
 * @brief Enable or disable the servo pulse train.
 *
 * Disabling holds the PWM pin low so the servo stops actively hunting while
 * the table is idle. Enabling releases the MCPWM generator back to normal PWM.
 *
 * @param servo Initialized servo instance.
 * @param enabled true to emit pulses, false to hold output low.
 * @return ESP_OK on success.
 */
esp_err_t bsp_servo_set_pulse_enabled(bsp_servo_t *servo, bool enabled);

/**
 * @brief Command a servo with a normalized position in tenths of percent.
 *
 * @param servo Initialized servo instance.
 * @param percent_tenths Desired position in the range -1000 to 1000.
 * @return ESP_OK on success.
 */
esp_err_t bsp_servo_write_tenths_percent(bsp_servo_t *servo, int percent_tenths);

/**
 * @brief Command a servo with a normalized position.
 *
 * @param servo Initialized servo instance.
 * @param percent Desired position in the range -100 to 100.
 * @return ESP_OK on success.
 */
esp_err_t bsp_servo_write_percent(bsp_servo_t *servo, int percent);

#ifdef __cplusplus
}
#endif
