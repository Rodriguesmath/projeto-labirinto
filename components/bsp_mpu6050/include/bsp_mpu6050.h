/**
 * @file bsp_mpu6050.h
 * @brief ESP-IDF I2C driver for MPU6050 orientation sensing.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** MPU6050 processed sample used by the application layer. */
typedef struct {
    int16_t accel_x_raw;    /**< Raw accelerometer X reading. */
    int16_t accel_y_raw;    /**< Raw accelerometer Y reading. */
    int16_t accel_z_raw;    /**< Raw accelerometer Z reading. */
    int16_t gyro_x_raw;     /**< Raw gyroscope X reading. */
    int16_t gyro_y_raw;     /**< Raw gyroscope Y reading. */
    int16_t gyro_z_raw;     /**< Raw gyroscope Z reading. */
    float accel_x_g;        /**< Accelerometer X value in g. */
    float accel_y_g;        /**< Accelerometer Y value in g. */
    float accel_z_g;        /**< Accelerometer Z value in g. */
    float gyro_x_dps;       /**< Gyroscope X value in degrees per second. */
    float gyro_y_dps;       /**< Gyroscope Y value in degrees per second. */
    float gyro_z_dps;       /**< Gyroscope Z value in degrees per second. */
    float accel_pitch_deg;  /**< Pitch estimated from the accelerometer only. */
    float accel_roll_deg;   /**< Roll estimated from the accelerometer only. */
    float pitch_deg;        /**< Filtered pitch angle in degrees. */
    float roll_deg;         /**< Filtered roll angle in degrees. */
    float dt_s;             /**< Elapsed time used by the complementary filter. */
} bsp_mpu6050_sample_t;

/** Static MPU6050 driver configuration. */
typedef struct {
    i2c_port_num_t i2c_port;      /**< I2C controller port. */
    gpio_num_t sda_gpio;          /**< SDA GPIO. */
    gpio_num_t scl_gpio;          /**< SCL GPIO. */
    uint8_t device_address;       /**< 7-bit I2C device address. */
    uint32_t scl_speed_hz;        /**< I2C bus speed. */
    int transaction_timeout_ms;   /**< I2C transaction timeout. */
    uint16_t calibration_samples;  /**< Samples used for startup gyro calibration. */
    uint8_t calibration_max_attempts;       /**< Maximum gyro calibration attempts. */
    float gyro_stability_threshold_dps;     /**< Maximum gyro range accepted as stable. */
    float complementary_alpha;     /**< Gyro weight in the complementary filter. */
    float accel_filter_alpha;      /**< EMA factor applied to acceleration in g. */
} bsp_mpu6050_config_t;

/** MPU6050 driver instance. Allocate it in the caller scope. */
typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t device_handle;
    bsp_mpu6050_config_t config;
    float gyro_x_offset_dps;
    float gyro_y_offset_dps;
    float gyro_z_offset_dps;
    float accel_x_filtered_g;
    float accel_y_filtered_g;
    float accel_z_filtered_g;
    float pitch_filtered_deg;
    float roll_filtered_deg;
    int64_t last_read_time_us;
    bool accel_filter_initialized;
    bool angle_filter_initialized;
} bsp_mpu6050_t;

/**
 * @brief Fill an MPU6050 configuration with board defaults.
 *
 * @return Default configuration for the current board.
 */
bsp_mpu6050_config_t bsp_mpu6050_default_config(void);

/**
 * @brief Initialize the I2C bus and wake/configure the MPU6050.
 *
 * @param mpu Driver instance to initialize.
 * @param config Driver configuration.
 * @return ESP_OK on success.
 */
esp_err_t bsp_mpu6050_init(bsp_mpu6050_t *mpu, const bsp_mpu6050_config_t *config);

/**
 * @brief Read acceleration, gyroscope, pitch, and roll.
 *
 * @param mpu Initialized MPU6050 instance.
 * @param sample Output sample.
 * @return ESP_OK on success.
 */
esp_err_t bsp_mpu6050_read(bsp_mpu6050_t *mpu, bsp_mpu6050_sample_t *sample);

#ifdef __cplusplus
}
#endif
