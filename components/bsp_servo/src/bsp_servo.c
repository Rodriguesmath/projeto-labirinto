#include "bsp_servo.h"

#include "bsp_board.h"
#include "esp_check.h"

#define SERVO_PWM_FREQ_HZ 50
#define SERVO_LEDC_RESOLUTION LEDC_TIMER_16_BIT
#define SERVO_PWM_PERIOD_US 20000U
#define SERVO_MAX_DUTY ((1U << 16U) - 1U)
#define MAX_PERCENT 15
#define MIN_PERCENT -15

static int clamp_percent(int percent)
{
    if (percent > 100) {
        return 100;
    }
    if (percent < -100) {
        return -100;
    }
    return percent;
}

static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    return (pulse_us * SERVO_MAX_DUTY) / SERVO_PWM_PERIOD_US;
}

static uint32_t percent_to_pulse_us(const bsp_servo_channel_config_t *config, int percent)
{
    percent = clamp_percent(percent);
    if (percent >= 0) {
        const uint32_t span = config->max_pulse_us - config->center_pulse_us;
        return config->center_pulse_us + ((uint32_t)percent * span) / 100U;
    }

    const uint32_t span = config->center_pulse_us - config->min_pulse_us;
    return config->center_pulse_us - ((uint32_t)(-percent) * span) / 100U;
}

bsp_servo_channel_config_t bsp_servo_x_default_config(void)
{
    return (bsp_servo_channel_config_t) {
        .gpio = BSP_SERVO_X_GPIO,
        .channel = BSP_SERVO_X_LEDC_CHANNEL,
        .timer = BSP_SERVO_LEDC_TIMER,
        .speed_mode = BSP_SERVO_LEDC_MODE,
        .min_pulse_us = 500,
        .center_pulse_us = 1500,
        .max_pulse_us = 2500,
    };
}

bsp_servo_channel_config_t bsp_servo_y_default_config(void)
{
    return (bsp_servo_channel_config_t) {
        .gpio = BSP_SERVO_Y_GPIO,
        .channel = BSP_SERVO_Y_LEDC_CHANNEL,
        .timer = BSP_SERVO_LEDC_TIMER,
        .speed_mode = BSP_SERVO_LEDC_MODE,
        .min_pulse_us = 500,
        .center_pulse_us = 1500,
        .max_pulse_us = 2500,
    };
}

esp_err_t bsp_servo_timer_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = BSP_SERVO_LEDC_MODE,
        .timer_num = BSP_SERVO_LEDC_TIMER,
        .duty_resolution = SERVO_LEDC_RESOLUTION,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    return ledc_timer_config(&timer_config);
}

esp_err_t bsp_servo_init(bsp_servo_t *servo, const bsp_servo_channel_config_t *config)
{
    if (servo == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *servo = (bsp_servo_t) {
        .config = *config,
    };

    const ledc_channel_config_t channel_config = {
        .gpio_num = config->gpio,
        .speed_mode = config->speed_mode,
        .channel = config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->timer,
        .duty = pulse_to_duty(config->center_pulse_us),
        .hpoint = 0,
    };

    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), "bsp_servo", "ledc channel");
    return bsp_servo_write_percent(servo, 0);
}

esp_err_t bsp_servo_write_percent(const bsp_servo_t *servo, int percent)
{
    if (servo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (percent > MAX_PERCENT) {
        percent = MAX_PERCENT;
    } else if (percent < MIN_PERCENT) {
        percent = MIN_PERCENT;
    }

    const uint32_t pulse_us = percent_to_pulse_us(&servo->config, percent);
    const uint32_t duty = pulse_to_duty(pulse_us);

    ESP_RETURN_ON_ERROR(ledc_set_duty(servo->config.speed_mode, servo->config.channel, duty),
                        "bsp_servo", "set duty");
    ESP_RETURN_ON_ERROR(ledc_update_duty(servo->config.speed_mode, servo->config.channel),
                        "bsp_servo", "update duty");

    return ESP_OK;
}
