#include "bsp_status_led.h"

#include "bsp_board.h"
#include "driver/gpio.h"

esp_err_t bsp_status_led_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BSP_STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    return bsp_status_led_set(false);
}

esp_err_t bsp_status_led_set(bool on)
{
    return gpio_set_level(BSP_STATUS_LED_GPIO, on ? 1 : 0);
}
