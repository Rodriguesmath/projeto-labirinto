/**
 * @file rtos_port.h
 * @brief Small RTOS abstraction used by the application layer.
 *
 * The implementation is backed by FreeRTOS on ESP-IDF, but application modules
 * depend on this interface instead of directly including FreeRTOS headers.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Wait forever timeout value for RTOS calls. */
#define RTOS_WAIT_FOREVER UINT32_MAX

/** Opaque task handle. */
typedef void *rtos_task_handle_t;

/** Opaque queue handle. */
typedef void *rtos_queue_handle_t;

/** Return status for RTOS wrapper calls. */
typedef enum {
    RTOS_OK = 0,    /**< Operation succeeded. */
    RTOS_ERROR,     /**< Operation failed. */
    RTOS_TIMEOUT,   /**< Timeout expired before the operation completed. */
} rtos_status_t;

/** Task entry function signature. */
typedef void (*rtos_task_fn_t)(void *arg);

/** Task creation parameters. */
typedef struct {
    const char *name;          /**< Task name shown in diagnostics. */
    rtos_task_fn_t entry;      /**< Task entry function. */
    void *arg;                 /**< User argument passed to the task. */
    uint32_t stack_size_bytes; /**< Stack size in bytes. */
    uint32_t priority;         /**< RTOS priority. */
} rtos_task_config_t;

/**
 * @brief Create a task.
 *
 * @param config Task configuration.
 * @param handle Optional output task handle.
 * @return RTOS_OK on success.
 */
rtos_status_t rtos_task_create(const rtos_task_config_t *config, rtos_task_handle_t *handle);

/**
 * @brief Delay the current task.
 *
 * @param delay_ms Delay in milliseconds.
 */
void rtos_delay_ms(uint32_t delay_ms);

/**
 * @brief Create a queue.
 *
 * @param length Number of elements the queue can store.
 * @param item_size Size of each element in bytes.
 * @return Opaque queue handle, or NULL on failure.
 */
rtos_queue_handle_t rtos_queue_create(size_t length, size_t item_size);

/**
 * @brief Send an item to a queue.
 *
 * @param queue Queue handle.
 * @param item Item to copy into the queue.
 * @param timeout_ms Timeout in milliseconds, or RTOS_WAIT_FOREVER.
 * @return RTOS_OK, RTOS_TIMEOUT, or RTOS_ERROR.
 */
rtos_status_t rtos_queue_send(rtos_queue_handle_t queue, const void *item, uint32_t timeout_ms);

/**
 * @brief Receive an item from a queue.
 *
 * @param queue Queue handle.
 * @param item Destination buffer.
 * @param timeout_ms Timeout in milliseconds, or RTOS_WAIT_FOREVER.
 * @return RTOS_OK, RTOS_TIMEOUT, or RTOS_ERROR.
 */
rtos_status_t rtos_queue_receive(rtos_queue_handle_t queue, void *item, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
