#include "bsp_servo.h"

#include "bsp_board.h"
#include "esp_check.h"

#define SERVO_TIMER_RESOLUTION_HZ 1000000U
#define SERVO_PWM_PERIOD_US 20000U
#define MAX_PERCENT_TENTHS 150
#define MIN_PERCENT_TENTHS -150
#define SERVO_PULSE_DEADBAND_US 4U

static mcpwm_timer_handle_t s_servo_timer;

static int clamp_tenths_percent(int percent_tenths)
{
    if (percent_tenths > 1000) {
        return 1000;
    }
    if (percent_tenths < -1000) {
        return -1000;
    }
    return percent_tenths;
}

static uint32_t tenths_percent_to_pulse_us(const bsp_servo_channel_config_t *config, int percent_tenths)
{
    percent_tenths = clamp_tenths_percent(percent_tenths);
    if (percent_tenths >= 0) {
        const uint32_t span = config->max_pulse_us - config->center_pulse_us;
        return config->center_pulse_us + ((uint32_t)percent_tenths * span) / 1000U;
    }

    const uint32_t span = config->center_pulse_us - config->min_pulse_us;
    return config->center_pulse_us - ((uint32_t)(-percent_tenths) * span) / 1000U;
}

bsp_servo_channel_config_t bsp_servo_x_default_config(void)
{
    return (bsp_servo_channel_config_t) {
        .gpio = BSP_SERVO_X_GPIO,
        .min_pulse_us = 500,
        .center_pulse_us = 1500,
        .max_pulse_us = 2500,
    };
}

bsp_servo_channel_config_t bsp_servo_y_default_config(void)
{
    return (bsp_servo_channel_config_t) {
        .gpio = BSP_SERVO_Y_GPIO,
        .min_pulse_us = 500,
        .center_pulse_us = 1500,
        .max_pulse_us = 2500,
    };
}

esp_err_t bsp_servo_timer_init(void)
{
    if (s_servo_timer != NULL) {
        return ESP_OK;
    }

    const mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMER_RESOLUTION_HZ,
        .period_ticks = SERVO_PWM_PERIOD_US,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };

    ESP_RETURN_ON_ERROR(mcpwm_new_timer(&timer_config, &s_servo_timer),
                        "bsp_servo", "mcpwm timer");
    ESP_RETURN_ON_ERROR(mcpwm_timer_enable(s_servo_timer),
                        "bsp_servo", "mcpwm timer enable");
    ESP_RETURN_ON_ERROR(mcpwm_timer_start_stop(s_servo_timer, MCPWM_TIMER_START_NO_STOP),
                        "bsp_servo", "mcpwm timer start");

    return ESP_OK;
}

esp_err_t bsp_servo_init(bsp_servo_t *servo, const bsp_servo_channel_config_t *config)
{
    if (servo == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_servo_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *servo = (bsp_servo_t) {
        .config = *config,
    };

    const mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_operator(&operator_config, &servo->operator),
                        "bsp_servo", "mcpwm operator");
    ESP_RETURN_ON_ERROR(mcpwm_operator_connect_timer(servo->operator, s_servo_timer),
                        "bsp_servo", "mcpwm connect timer");

    const mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_comparator(servo->operator, &comparator_config, &servo->comparator),
                        "bsp_servo", "mcpwm comparator");

    const mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = config->gpio,
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_generator(servo->operator, &generator_config, &servo->generator),
                        "bsp_servo", "mcpwm generator");

    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(servo->comparator, config->center_pulse_us),
                        "bsp_servo", "mcpwm center compare");
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_timer_event(
                            servo->generator,
                            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                          MCPWM_TIMER_EVENT_EMPTY,
                                                          MCPWM_GEN_ACTION_HIGH)),
                        "bsp_servo", "mcpwm generator high");
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_compare_event(
                            servo->generator,
                            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                            servo->comparator,
                                                            MCPWM_GEN_ACTION_LOW)),
                        "bsp_servo", "mcpwm generator low");

    servo->last_pulse_us = config->center_pulse_us;
    servo->command_initialized = true;
    servo->pulse_enabled = true;
    return ESP_OK;
}

esp_err_t bsp_servo_set_pulse_enabled(bsp_servo_t *servo, bool enabled)
{
    if (servo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (servo->pulse_enabled == enabled) {
        return ESP_OK;
    }

    const int force_level = enabled ? -1 : 0;
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_force_level(servo->generator, force_level, true),
                        "bsp_servo", "set force level");
    servo->pulse_enabled = enabled;

    return ESP_OK;
}

esp_err_t bsp_servo_write_tenths_percent(bsp_servo_t *servo, int percent_tenths)
{
    if (servo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (percent_tenths > MAX_PERCENT_TENTHS) {
        percent_tenths = MAX_PERCENT_TENTHS;
    } else if (percent_tenths < MIN_PERCENT_TENTHS) {
        percent_tenths = MIN_PERCENT_TENTHS;
    }

    const uint32_t pulse_us = tenths_percent_to_pulse_us(&servo->config, percent_tenths);
    ESP_RETURN_ON_ERROR(bsp_servo_set_pulse_enabled(servo, true),
                        "bsp_servo", "enable pulse");

    if (servo->command_initialized) {
        const uint32_t delta_us = (pulse_us > servo->last_pulse_us)
                                      ? (pulse_us - servo->last_pulse_us)
                                      : (servo->last_pulse_us - pulse_us);
        if (delta_us < SERVO_PULSE_DEADBAND_US) {
            return ESP_OK;
        }
    }

    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(servo->comparator, pulse_us),
                        "bsp_servo", "set compare");

    servo->last_pulse_us = pulse_us;
    servo->command_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_servo_write_percent(bsp_servo_t *servo, int percent)
{
    return bsp_servo_write_tenths_percent(servo, percent * 10);
}
