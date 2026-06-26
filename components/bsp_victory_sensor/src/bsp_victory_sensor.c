#include "bsp_victory_sensor.h"

#include "bsp_board.h"
#include "esp_check.h"
#include "esp_err.h"

#define TAG "bsp_victory_sensor"

bsp_victory_sensor_config_t bsp_victory_sensor_default_config(void)
{
    return (bsp_victory_sensor_config_t){
        .sensor_gpio = BSP_SENSOR_VITORIA_GPIO,
        .led_gpio    = BSP_LED_VITORIA_GPIO,
    };
}

esp_err_t bsp_victory_sensor_init(bsp_victory_sensor_t *sensor,
                                  const bsp_victory_sensor_config_t *config)
{
    if (sensor == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sensor->config = *config;

    /* Configure the optical sensor GPIO as input with internal pull-up.
     * The sensor pulls the line low when a reflective surface is detected. */
    const gpio_config_t sensor_io_cfg = {
        .pin_bit_mask = (1ULL << (uint32_t)config->sensor_gpio),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sensor_io_cfg), TAG, "sensor gpio config");

    /* Configure the LED GPIO as push-pull output, starting in the off state. */
    const gpio_config_t led_io_cfg = {
        .pin_bit_mask = (1ULL << (uint32_t)config->led_gpio),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&led_io_cfg), TAG, "led gpio config");
    ESP_RETURN_ON_ERROR(gpio_set_level(config->led_gpio, 0), TAG, "led off at init");

    return ESP_OK;
}

esp_err_t bsp_victory_sensor_read(bsp_victory_sensor_t *sensor, bool *detected)
{
    if (sensor == NULL || detected == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* The sensor drives the line LOW when a highly reflective surface is in
     * range.  Invert the logic so the caller receives an intuitive bool. */
    const int level = gpio_get_level(sensor->config.sensor_gpio);
    *detected = (level == 0);

    return ESP_OK;
}

esp_err_t bsp_victory_sensor_set_led(bsp_victory_sensor_t *sensor, bool on)
{
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(sensor->config.led_gpio, on ? 1 : 0),
                        TAG, "led set level");

    return ESP_OK;
}
