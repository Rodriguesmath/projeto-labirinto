#include "rtos_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static TickType_t timeout_to_ticks(uint32_t timeout_ms)
{
    return (timeout_ms == RTOS_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
}

rtos_status_t rtos_task_create(const rtos_task_config_t *config, rtos_task_handle_t *handle)
{
    if (config == NULL || config->entry == NULL || config->name == NULL) {
        return RTOS_ERROR;
    }

    TaskHandle_t native_handle = NULL;
    const BaseType_t ok = xTaskCreate(config->entry,
                                      config->name,
                                      config->stack_size_bytes / sizeof(StackType_t),
                                      config->arg,
                                      config->priority,
                                      &native_handle);
    if (ok != pdPASS) {
        return RTOS_ERROR;
    }

    if (handle != NULL) {
        *handle = (rtos_task_handle_t)native_handle;
    }

    return RTOS_OK;
}

void rtos_delay_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

rtos_queue_handle_t rtos_queue_create(size_t length, size_t item_size)
{
    return (rtos_queue_handle_t)xQueueCreate((UBaseType_t)length, (UBaseType_t)item_size);
}

rtos_status_t rtos_queue_send(rtos_queue_handle_t queue, const void *item, uint32_t timeout_ms)
{
    if (queue == NULL || item == NULL) {
        return RTOS_ERROR;
    }

    const BaseType_t ok = xQueueSend((QueueHandle_t)queue, item, timeout_to_ticks(timeout_ms));
    return (ok == pdPASS) ? RTOS_OK : RTOS_TIMEOUT;
}

rtos_status_t rtos_queue_receive(rtos_queue_handle_t queue, void *item, uint32_t timeout_ms)
{
    if (queue == NULL || item == NULL) {
        return RTOS_ERROR;
    }

    const BaseType_t ok = xQueueReceive((QueueHandle_t)queue, item, timeout_to_ticks(timeout_ms));
    return (ok == pdPASS) ? RTOS_OK : RTOS_TIMEOUT;
}
