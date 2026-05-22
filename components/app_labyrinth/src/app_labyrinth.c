#include "app_labyrinth.h"

#include "bsp_board.h"
#include "bsp_joystick.h"
#include "bsp_servo.h"
#include "bsp_status_led.h"
#include "esp_check.h"
#include "esp_log.h"
#include "rtos_port.h"

#define JOYSTICK_PERIOD_MS 20U
#define SERVO_QUEUE_LENGTH 4U
#define DEBUG_QUEUE_LENGTH 8U
#define TASK_STACK_BYTES 3072U
#define JOYSTICK_TASK_PRIORITY 5U
#define SERVO_TASK_PRIORITY 4U
#define STATUS_TASK_PRIORITY 3U
#define DEBUG_PRINT_EVERY_SAMPLES 25U
#define SERVO_IDLE_DELAY_MS 1U
#define SERVO_INVALID_PERCENT 101

static const char *TAG = "app_labyrinth";

typedef struct {
    bsp_joystick_t joystick;
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

static void servo_task(void *arg)
{
    app_labyrinth_context_t *ctx = (app_labyrinth_context_t *)arg;
    int last_x_percent = SERVO_INVALID_PERCENT;
    int last_y_percent = SERVO_INVALID_PERCENT;

    while (true) {
        bsp_joystick_sample_t sample = {0};
        if (rtos_queue_receive(ctx->servo_queue, &sample, RTOS_WAIT_FOREVER) == RTOS_OK) {
            if (sample.x_percent != last_x_percent) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_servo_write_percent(&ctx->servo_x, sample.x_percent));
                last_x_percent = sample.x_percent;
            }

            if (sample.y_percent != last_y_percent) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_servo_write_percent(&ctx->servo_y, sample.y_percent));
                last_y_percent = sample.y_percent;
            }

            rtos_delay_ms(SERVO_IDLE_DELAY_MS);
        }
    }
}

static void status_task(void *arg)
{
    app_labyrinth_context_t *ctx = (app_labyrinth_context_t *)arg;
    uint32_t sample_count = 0;

    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_status_led_set(true));
    ESP_LOGI(TAG, "Sistema inicializado. Mesa pronta para uso.");

    while (true) {
        bsp_joystick_sample_t sample = {0};
        if (rtos_queue_receive(ctx->debug_queue, &sample, RTOS_WAIT_FOREVER) != RTOS_OK) {
            continue;
        }

        sample_count++;
        if (sample_count >= DEBUG_PRINT_EVERY_SAMPLES) {
            ESP_LOGI(TAG,
                     "joy_raw=(%4d,%4d) joy_cmd=(%4d%%,%4d%%)",
                     sample.x_raw,
                     sample.y_raw,
                     sample.x_percent,
                     sample.y_percent);
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

    ESP_RETURN_ON_ERROR(bsp_status_led_init(), TAG, "status led init");
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
    const rtos_task_config_t status_task_config = {
        .name = "status_task",
        .entry = status_task,
        .arg = &s_app,
        .stack_size_bytes = TASK_STACK_BYTES,
        .priority = STATUS_TASK_PRIORITY,
    };

    if (rtos_task_create(&joystick_task_config, NULL) != RTOS_OK ||
        rtos_task_create(&servo_task_config, NULL) != RTOS_OK ||
        rtos_task_create(&status_task_config, NULL) != RTOS_OK) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
