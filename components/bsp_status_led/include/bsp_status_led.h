/**
 * @file bsp_status_led.h
 * @brief Board status LED abstraction.
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure the status LED GPIO.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_status_led_init(void);

/**
 * @brief Set whether the system-ready LED is lit.
 *
 * @param on True to turn the LED on, false to turn it off.
 * @return ESP_OK on success.
 */
esp_err_t bsp_status_led_set(bool on);

#ifdef __cplusplus
}
#endif
