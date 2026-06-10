#include "bsp_joystick.h"

#include <math.h>
#include <stdlib.h>

#include "bsp_board.h"
#include "esp_check.h"

#define ADC_SAMPLE_COUNT 9U
#define ADC_TRIM_COUNT 2U

static int clamp_percent_tenths(int value)
{
    if (value > 1000) {
        return 1000;
    }
    if (value < -1000) {
        return -1000;
    }
    return value;
}

static int process_axis(const bsp_joystick_config_t *config, int raw, int center_raw)
{
    if (abs(raw - center_raw) <= config->deadzone_raw) {
        return 0;
    }

    float normalized = 0.0f;
    if (raw < center_raw) {
        const int usable_low = center_raw - config->deadzone_raw - config->min_raw;
        normalized = (float)(center_raw - config->deadzone_raw - raw) / (float)usable_low;
        return clamp_percent_tenths((int)(-1000.0f * normalized));
    }

    const int usable_high = config->max_raw - (center_raw + config->deadzone_raw);
    normalized = (float)(raw - (center_raw + config->deadzone_raw)) / (float)usable_high;
    return clamp_percent_tenths((int)(1000.0f * normalized));
}

static float filter_axis(const bsp_joystick_config_t *config, float previous, int raw)
{
    const float delta = fabsf((float)raw - previous);
    const float alpha = (delta > (float)config->fast_threshold_raw) ? config->alpha_fast : config->alpha_slow;
    return (alpha * (float)raw) + ((1.0f - alpha) * previous);
}

static void sort_samples(int *samples, size_t count)
{
    for (size_t i = 1; i < count; i++) {
        const int value = samples[i];
        size_t j = i;
        while (j > 0 && samples[j - 1] > value) {
            samples[j] = samples[j - 1];
            j--;
        }
        samples[j] = value;
    }
}

static esp_err_t read_filtered_raw(adc_oneshot_unit_handle_t handle, adc_channel_t channel, int *raw_out)
{
    int samples[ADC_SAMPLE_COUNT] = {0};

    for (size_t i = 0; i < ADC_SAMPLE_COUNT; i++) {
        ESP_RETURN_ON_ERROR(adc_oneshot_read(handle, channel, &samples[i]), "bsp_joystick", "read filtered");
    }

    sort_samples(samples, ADC_SAMPLE_COUNT);

    int total = 0;
    for (size_t i = ADC_TRIM_COUNT; i < ADC_SAMPLE_COUNT - ADC_TRIM_COUNT; i++) {
        total += samples[i];
    }

    *raw_out = total / (int)(ADC_SAMPLE_COUNT - (2U * ADC_TRIM_COUNT));
    return ESP_OK;
}

bsp_joystick_config_t bsp_joystick_default_config(void)
{
    return (bsp_joystick_config_t) {
        .x_channel = BSP_JOYSTICK_X_CHANNEL,
        .y_channel = BSP_JOYSTICK_Y_CHANNEL,
        .min_raw = 0,
        .max_raw = 4095,
        .x_center_raw = BSP_JOYSTICK_X_CENTER_RAW,
        .y_center_raw = BSP_JOYSTICK_Y_CENTER_RAW,
        .deadzone_raw = 100,
        .alpha_slow = 0.03f,
        .alpha_fast = 0.25f,
        .fast_threshold_raw = 250,
    };
}

esp_err_t bsp_joystick_init(bsp_joystick_t *joystick, const bsp_joystick_config_t *config)
{
    if (joystick == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *joystick = (bsp_joystick_t) {
        .config = *config,
    };

    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = 0,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &joystick->adc_handle), "bsp_joystick", "adc unit");

    const adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(joystick->adc_handle, config->x_channel, &channel_config),
                        "bsp_joystick", "adc x channel");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(joystick->adc_handle, config->y_channel, &channel_config),
                        "bsp_joystick", "adc y channel");

    return ESP_OK;
}

esp_err_t bsp_joystick_read(bsp_joystick_t *joystick, bsp_joystick_sample_t *sample)
{
    if (joystick == NULL || sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int x_raw = 0;
    int y_raw = 0;
    ESP_RETURN_ON_ERROR(read_filtered_raw(joystick->adc_handle, joystick->config.x_channel, &x_raw),
                        "bsp_joystick", "read x");
    ESP_RETURN_ON_ERROR(read_filtered_raw(joystick->adc_handle, joystick->config.y_channel, &y_raw),
                        "bsp_joystick", "read y");

    if (!joystick->filter_initialized) {
        joystick->x_filtered = (float)x_raw;
        joystick->y_filtered = (float)y_raw;
        joystick->filter_initialized = true;
    } else {
        joystick->x_filtered = filter_axis(&joystick->config, joystick->x_filtered, x_raw);
        joystick->y_filtered = filter_axis(&joystick->config, joystick->y_filtered, y_raw);
    }

    const int x_percent_tenths = process_axis(&joystick->config,
                                              (int)joystick->x_filtered,
                                              joystick->config.x_center_raw);
    const int y_percent_tenths = process_axis(&joystick->config,
                                              (int)joystick->y_filtered,
                                              joystick->config.y_center_raw);

    *sample = (bsp_joystick_sample_t) {
        .x_raw = x_raw,
        .y_raw = y_raw,
        .x_percent = x_percent_tenths / 10,
        .y_percent = y_percent_tenths / 10,
        .x_percent_tenths = x_percent_tenths,
        .y_percent_tenths = y_percent_tenths,
    };

    return ESP_OK;
}
