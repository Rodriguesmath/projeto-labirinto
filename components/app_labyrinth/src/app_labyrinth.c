#include "app_labyrinth.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bsp_board.h"
#include "bsp_joystick.h"
#include "bsp_mpu6050.h"
#include "bsp_servo.h"
#include "esp_check.h"
#include "esp_log.h"
#include "rtos_port.h"

#define JOYSTICK_PERIOD_MS 20U
#define MPU6050_PERIOD_MS 20U
#define SERVO_QUEUE_LENGTH 4U
#define DEBUG_QUEUE_LENGTH 8U
#define TASK_STACK_BYTES 3072U
#define MPU6050_TASK_STACK_BYTES 4096U
#define JOYSTICK_TASK_PRIORITY 5U
#define SERVO_TASK_PRIORITY 4U
#define MPU6050_TASK_PRIORITY 4U
#define STATUS_TASK_PRIORITY 3U
#define DEBUG_PRINT_EVERY_SAMPLES 25U
#define SERVO_IDLE_DELAY_MS 1U
#define SERVO_MAX_VELOCITY 12
#define SERVO_ACCEL 2
#define SERVO_CENTER_MAX_VELOCITY 7
#define SERVO_CENTER_ACCEL 1
#define SERVO_TARGET_DEADBAND 2
#define SERVO_OUTPUT_MAX 150
#define SERVO_OUTPUT_MIN -150
#define SERVO_OUTPUT_LOG_ENABLED 0
#define SERVO_RELAX_AFTER_STABLE_SAMPLES 3U
#define SERVO_STARTUP_CENTER_HOLD_MS 800U

static const char *TAG = "app_labyrinth";

typedef struct {
    bsp_joystick_t joystick;
    bsp_mpu6050_t mpu6050;
    bsp_servo_t servo_x;
    bsp_servo_t servo_y;
    rtos_queue_handle_t servo_queue;
    rtos_queue_handle_t debug_queue;
} app_labyrinth_context_t;

static app_labyrinth_context_t s_app;

static esp_err_t create_queue(rtos_queue_handle_t *queue, size_t length, size_t item_size)
{
    *queue = rtos_queue_create(length, item_size);
    return (*queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK;
}

static int approach_int(int current, int target, int step)
{
    const int delta = target - current;
    if (delta > step) {
        return current + step;
    }
    if (delta < -step) {
        return current - step;
    }
    return target;
}

static int integer_sqrt(int value)
{
    int root = 0;
    while (((root + 1) * (root + 1)) <= value) {
        root++;
    }
    return root;
}

static int sign_int(int value)
{
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

static int motion_step_percent(int current, int target, int *velocity, int max_velocity, int accel)
{
    const int delta = target - current;
    if (delta == 0) {
        *velocity = 0;
        return current;
    }

    const int braking_velocity = integer_sqrt(2 * accel * abs(delta));
    const int desired_speed = (braking_velocity > max_velocity) ? max_velocity : braking_velocity;
    const int desired_velocity = sign_int(delta) * desired_speed;
    *velocity = approach_int(*velocity, desired_velocity, accel);

    if (abs(delta) <= abs(*velocity)) {
        *velocity = 0;
        return target;
    }

    return current + *velocity;
}

static int motion_step_for_target(int current, int target, int *velocity)
{
    const bool returning_to_center = (target == 0);
    const int max_velocity = returning_to_center ? SERVO_CENTER_MAX_VELOCITY : SERVO_MAX_VELOCITY;
    const int accel = returning_to_center ? SERVO_CENTER_ACCEL : SERVO_ACCEL;

    return motion_step_percent(current, target, velocity, max_velocity, accel);
}

static int update_servo_target(int current_target, int requested)
{
    return (abs(requested - current_target) < SERVO_TARGET_DEADBAND) ? current_target : requested;
}

static bool target_reversed(int previous_target, int next_target)
{
    return previous_target != 0 && next_target != 0 && sign_int(previous_target) != sign_int(next_target);
}

static int clamp_servo_output(int percent_tenths)
{
    if (percent_tenths > SERVO_OUTPUT_MAX) {
        return SERVO_OUTPUT_MAX;
    }
    if (percent_tenths < SERVO_OUTPUT_MIN) {
        return SERVO_OUTPUT_MIN;
    }
    return percent_tenths;
}

static bool servo_can_relax(int current_percent_tenths, int target_percent_tenths)
{
    return current_percent_tenths == 0 && target_percent_tenths == 0;
}

static char tenths_sign(int value)
{
    return (value < 0) ? '-' : ' ';
}

static int tenths_whole_abs(int value)
{
    return abs(value) / 10;
}

static int tenths_fraction_abs(int value)
{
    return abs(value) % 10;
}

static void joystick_task(void *arg)
{
    app_labyrinth_context_t *ctx = (app_labyrinth_context_t *)arg;

    while (true) {
        bsp_joystick_sample_t sample = {0};
        const esp_err_t err = bsp_joystick_read(&ctx->joystick, &sample);
        if (err == ESP_OK) {
            (void)rtos_queue_send(ctx->servo_queue, &sample, 0);
            (void)rtos_queue_send(ctx->debug_queue, &sample, 0);
        } else {
            ESP_LOGW(TAG, "Falha na leitura do joystick: %s", esp_err_to_name(err));
        }

        rtos_delay_ms(JOYSTICK_PERIOD_MS);
    }
}

static void mpu6050_task(void *arg)
{
    app_labyrinth_context_t *ctx = (app_labyrinth_context_t *)arg;

    while (true) {
        bsp_mpu6050_sample_t sample = {0};
        const esp_err_t err = bsp_mpu6050_read(&ctx->mpu6050, &sample);
        if (err == ESP_OK) {
            printf("{\"sensor\":\"mpu6050\",\"pitch_x_deg\":%.2f,\"roll_y_deg\":%.2f,"
                   "\"accel_pitch_deg\":%.2f,\"accel_roll_deg\":%.2f,\"dt_ms\":%.1f,"
                   "\"accel_g\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
                   "\"gyro_dps\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}}\n",
                   sample.pitch_deg,
                   sample.roll_deg,
                   sample.accel_pitch_deg,
                   sample.accel_roll_deg,
                   sample.dt_s * 1000.0f,
                   sample.accel_x_g,
                   sample.accel_y_g,
                   sample.accel_z_g,
                   sample.gyro_x_dps,
                   sample.gyro_y_dps,
                   sample.gyro_z_dps);
        } else {
            ESP_LOGW(TAG, "Falha na leitura do MPU6050: %s", esp_err_to_name(err));
        }

        rtos_delay_ms(MPU6050_PERIOD_MS);
    }
}

static void servo_task(void *arg)
{
    app_labyrinth_context_t *ctx = (app_labyrinth_context_t *)arg;
    int current_x_percent = 0;
    int current_y_percent = 0;
    int target_x_percent = 0;
    int target_y_percent = 0;
    int velocity_x_percent = 0;
    int velocity_y_percent = 0;
    uint32_t stable_x_sample_count = 0;
    uint32_t stable_y_sample_count = 0;
    bool pulse_x_relaxed = false;
    bool pulse_y_relaxed = false;

    while (true) {
        bsp_joystick_sample_t sample = {0};
        if (rtos_queue_receive(ctx->servo_queue, &sample, RTOS_WAIT_FOREVER) == RTOS_OK) {
            const int previous_x_percent = current_x_percent;
            const int previous_y_percent = current_y_percent;
            const int requested_x_percent = clamp_servo_output(sample.x_percent_tenths);
            const int requested_y_percent = clamp_servo_output(-sample.y_percent_tenths);
            const int previous_target_x_percent = target_x_percent;
            const int previous_target_y_percent = target_y_percent;
            target_x_percent = update_servo_target(target_x_percent, requested_x_percent);
            target_y_percent = update_servo_target(target_y_percent, requested_y_percent);

            if (target_reversed(previous_target_x_percent, target_x_percent)) {
                velocity_x_percent = 0;
            }
            if (target_reversed(previous_target_y_percent, target_y_percent)) {
                velocity_y_percent = 0;
            }

            const int next_x_percent = motion_step_for_target(current_x_percent,
                                                              target_x_percent,
                                                              &velocity_x_percent);
            const int next_y_percent = motion_step_for_target(current_y_percent,
                                                              target_y_percent,
                                                              &velocity_y_percent);
            bool wrote_x = false;
            bool wrote_y = false;

            if (next_x_percent != current_x_percent) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_servo_write_tenths_percent(&ctx->servo_x, next_x_percent));
                current_x_percent = next_x_percent;
                wrote_x = true;
            }

            if (next_y_percent != current_y_percent) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_servo_write_tenths_percent(&ctx->servo_y, next_y_percent));
                current_y_percent = next_y_percent;
                wrote_y = true;
            }

            if (wrote_x || !servo_can_relax(current_x_percent, target_x_percent)) {
                stable_x_sample_count = 0;
                pulse_x_relaxed = false;
            } else if (!pulse_x_relaxed) {
                stable_x_sample_count++;
                if (stable_x_sample_count >= SERVO_RELAX_AFTER_STABLE_SAMPLES) {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_servo_set_pulse_enabled(&ctx->servo_x, false));
                    pulse_x_relaxed = true;
                }
            }

            if (wrote_y || !servo_can_relax(current_y_percent, target_y_percent)) {
                stable_y_sample_count = 0;
                pulse_y_relaxed = false;
            } else if (!pulse_y_relaxed) {
                stable_y_sample_count++;
                if (stable_y_sample_count >= SERVO_RELAX_AFTER_STABLE_SAMPLES) {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_servo_set_pulse_enabled(&ctx->servo_y, false));
                    pulse_y_relaxed = true;
                }
            }

#if SERVO_OUTPUT_LOG_ENABLED
            if (wrote_x || wrote_y) {
                ESP_LOGI(TAG,
                         "servo_out raw=(%4d,%4d) joy_cmd=(%c%3d.%1d%%,%c%3d.%1d%%) request=(%c%3d.%1d%%,%c%3d.%1d%%) target=(%c%3d.%1d%%,%c%3d.%1d%%) prev=(%c%3d.%1d%%,%c%3d.%1d%%) pwm=(%c%3d.%1d%%,%c%3d.%1d%%) write=(%d,%d)",
                         sample.x_raw,
                         sample.y_raw,
                         tenths_sign(sample.x_percent_tenths),
                         tenths_whole_abs(sample.x_percent_tenths),
                         tenths_fraction_abs(sample.x_percent_tenths),
                         tenths_sign(sample.y_percent_tenths),
                         tenths_whole_abs(sample.y_percent_tenths),
                         tenths_fraction_abs(sample.y_percent_tenths),
                         tenths_sign(requested_x_percent),
                         tenths_whole_abs(requested_x_percent),
                         tenths_fraction_abs(requested_x_percent),
                         tenths_sign(requested_y_percent),
                         tenths_whole_abs(requested_y_percent),
                         tenths_fraction_abs(requested_y_percent),
                         tenths_sign(target_x_percent),
                         tenths_whole_abs(target_x_percent),
                         tenths_fraction_abs(target_x_percent),
                         tenths_sign(target_y_percent),
                         tenths_whole_abs(target_y_percent),
                         tenths_fraction_abs(target_y_percent),
                         tenths_sign(previous_x_percent),
                         tenths_whole_abs(previous_x_percent),
                         tenths_fraction_abs(previous_x_percent),
                         tenths_sign(previous_y_percent),
                         tenths_whole_abs(previous_y_percent),
                         tenths_fraction_abs(previous_y_percent),
                         tenths_sign(current_x_percent),
                         tenths_whole_abs(current_x_percent),
                         tenths_fraction_abs(current_x_percent),
                         tenths_sign(current_y_percent),
                         tenths_whole_abs(current_y_percent),
                         tenths_fraction_abs(current_y_percent),
                         wrote_x,
                         wrote_y);
            }
#else
            (void)previous_x_percent;
            (void)previous_y_percent;
            (void)wrote_x;
            (void)wrote_y;
#endif

            rtos_delay_ms(SERVO_IDLE_DELAY_MS);
        }
    }
}

static void status_task(void *arg)
{
    app_labyrinth_context_t *ctx = (app_labyrinth_context_t *)arg;
    uint32_t sample_count = 0;

    ESP_LOGI(TAG, "Sistema inicializado. Mesa pronta para uso.");

    while (true) {
        bsp_joystick_sample_t sample = {0};
        if (rtos_queue_receive(ctx->debug_queue, &sample, RTOS_WAIT_FOREVER) != RTOS_OK) {
            continue;
        }

        sample_count++;
        if (sample_count >= DEBUG_PRINT_EVERY_SAMPLES) {
            ESP_LOGI(TAG,
                     "joy_raw=(%4d,%4d) joy_cmd=(%c%3d.%1d%%,%c%3d.%1d%%) servo_pulse=(%4lu,%4lu)us",
                     sample.x_raw,
                     sample.y_raw,
                     tenths_sign(sample.x_percent_tenths),
                     tenths_whole_abs(sample.x_percent_tenths),
                     tenths_fraction_abs(sample.x_percent_tenths),
                     tenths_sign(sample.y_percent_tenths),
                     tenths_whole_abs(sample.y_percent_tenths),
                     tenths_fraction_abs(sample.y_percent_tenths),
                     (unsigned long)ctx->servo_x.last_pulse_us,
                     (unsigned long)ctx->servo_y.last_pulse_us);
            sample_count = 0;
        }
    }
}

esp_err_t app_labyrinth_start(void)
{
    ESP_RETURN_ON_ERROR(bsp_board_init(), TAG, "board init");

    const bsp_joystick_config_t joystick_config = bsp_joystick_default_config();
    ESP_RETURN_ON_ERROR(bsp_joystick_init(&s_app.joystick, &joystick_config), TAG, "joystick init");

    ESP_RETURN_ON_ERROR(bsp_servo_timer_init(), TAG, "servo timer init");
    const bsp_servo_channel_config_t servo_x_config = bsp_servo_x_default_config();
    const bsp_servo_channel_config_t servo_y_config = bsp_servo_y_default_config();
    ESP_RETURN_ON_ERROR(bsp_servo_init(&s_app.servo_x, &servo_x_config), TAG, "servo x init");
    ESP_RETURN_ON_ERROR(bsp_servo_init(&s_app.servo_y, &servo_y_config), TAG, "servo y init");
    ESP_RETURN_ON_ERROR(bsp_servo_write_tenths_percent(&s_app.servo_x, 0), TAG, "servo x center");
    ESP_RETURN_ON_ERROR(bsp_servo_write_tenths_percent(&s_app.servo_y, 0), TAG, "servo y center");
    rtos_delay_ms(SERVO_STARTUP_CENTER_HOLD_MS);

    const bsp_mpu6050_config_t mpu6050_config = bsp_mpu6050_default_config();
    ESP_RETURN_ON_ERROR(bsp_mpu6050_init(&s_app.mpu6050, &mpu6050_config), TAG, "mpu6050 init");

    ESP_RETURN_ON_ERROR(create_queue(&s_app.servo_queue, SERVO_QUEUE_LENGTH, sizeof(bsp_joystick_sample_t)),
                        TAG, "servo queue");
    ESP_RETURN_ON_ERROR(create_queue(&s_app.debug_queue, DEBUG_QUEUE_LENGTH, sizeof(bsp_joystick_sample_t)),
                        TAG, "debug queue");

    const rtos_task_config_t joystick_task_config = {
        .name = "joystick_task",
        .entry = joystick_task,
        .arg = &s_app,
        .stack_size_bytes = TASK_STACK_BYTES,
        .priority = JOYSTICK_TASK_PRIORITY,
    };
    const rtos_task_config_t servo_task_config = {
        .name = "servo_task",
        .entry = servo_task,
        .arg = &s_app,
        .stack_size_bytes = TASK_STACK_BYTES,
        .priority = SERVO_TASK_PRIORITY,
    };
    const rtos_task_config_t mpu6050_task_config = {
        .name = "mpu6050_task",
        .entry = mpu6050_task,
        .arg = &s_app,
        .stack_size_bytes = MPU6050_TASK_STACK_BYTES,
        .priority = MPU6050_TASK_PRIORITY,
    };
    const rtos_task_config_t status_task_config = {
        .name = "status_task",
        .entry = status_task,
        .arg = &s_app,
        .stack_size_bytes = TASK_STACK_BYTES,
        .priority = STATUS_TASK_PRIORITY,
    };

    if (rtos_task_create(&joystick_task_config, NULL) != RTOS_OK ||
        rtos_task_create(&servo_task_config, NULL) != RTOS_OK ||
        rtos_task_create(&mpu6050_task_config, NULL) != RTOS_OK ||
        rtos_task_create(&status_task_config, NULL) != RTOS_OK) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
