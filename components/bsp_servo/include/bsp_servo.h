/**
 * @file bsp_servo.h
 * @brief Servo PWM driver for the two-axis labyrinth table.
 */

#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Static configuration for one hobby servo. */
typedef struct {
    gpio_num_t gpio;                 /**< PWM output pin. */
    ledc_channel_t channel;          /**< LEDC PWM channel. */
    ledc_timer_t timer;              /**< LEDC timer. */
    ledc_mode_t speed_mode;          /**< LEDC speed mode. */
    uint32_t min_pulse_us;           /**< Pulse width for -100 command. */
    uint32_t center_pulse_us;        /**< Pulse width for 0 command. */
    uint32_t max_pulse_us;           /**< Pulse width for +100 command. */
} bsp_servo_channel_config_t;

/** Driver instance for one servo channel. */
typedef struct {
    bsp_servo_channel_config_t config;
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
 * @brief Initialize the shared LEDC timer used for 50 Hz servo control.
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
 * @brief Command a servo with a normalized position.
 *
 * @param servo Initialized servo instance.
 * @param percent Desired position in the range -100 to 100.
 * @return ESP_OK on success.
 */
esp_err_t bsp_servo_write_percent(const bsp_servo_t *servo, int percent);

#ifdef __cplusplus
}
#endif
