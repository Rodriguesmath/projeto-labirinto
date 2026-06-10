#include "bsp_mpu6050.h"

#include <stdbool.h>
#include <math.h>

#include "bsp_board.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define MPU6050_ADDR 0x68U
#define MPU6050_WHO_AM_I_EXPECTED 0x68U
#define MPU6050_REG_SMPLRT_DIV 0x19U
#define MPU6050_REG_CONFIG 0x1AU
#define MPU6050_REG_GYRO_CONFIG 0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_PWR_MGMT_1 0x6BU
#define MPU6050_REG_WHO_AM_I 0x75U
#define MPU6050_ACCEL_LSB_PER_G 16384.0f
#define MPU6050_GYRO_LSB_PER_DPS 131.0f
#define DEG_PER_RAD 57.2957795f
#define DEFAULT_DT_S 0.02f
#define MAX_DT_S 0.20f
#define CALIBRATION_DELAY_US 2000U
#define FLOAT_MAX_INIT 1000000.0f

static const char *TAG = "bsp_mpu6050";

static int16_t read_i16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static esp_err_t write_reg(bsp_mpu6050_t *mpu, uint8_t reg, uint8_t value)
{
    const uint8_t data[2] = {reg, value};
    return i2c_master_transmit(mpu->device_handle, data, sizeof(data), mpu->config.transaction_timeout_ms);
}

static esp_err_t read_regs(bsp_mpu6050_t *mpu, uint8_t start_reg, uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(mpu->device_handle,
                                       &start_reg,
                                       sizeof(start_reg),
                                       data,
                                       length,
                                       mpu->config.transaction_timeout_ms);
}

static esp_err_t read_motion_raw(bsp_mpu6050_t *mpu,
                                 int16_t *accel_x_raw,
                                 int16_t *accel_y_raw,
                                 int16_t *accel_z_raw,
                                 int16_t *gyro_x_raw,
                                 int16_t *gyro_y_raw,
                                 int16_t *gyro_z_raw)
{
    uint8_t data[14] = {0};
    ESP_RETURN_ON_ERROR(read_regs(mpu, MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data)), TAG, "read motion");

    *accel_x_raw = read_i16_be(&data[0]);
    *accel_y_raw = read_i16_be(&data[2]);
    *accel_z_raw = read_i16_be(&data[4]);
    *gyro_x_raw = read_i16_be(&data[8]);
    *gyro_y_raw = read_i16_be(&data[10]);
    *gyro_z_raw = read_i16_be(&data[12]);

    return ESP_OK;
}

static float ema_filter(float previous, float current, float alpha)
{
    return (alpha * current) + ((1.0f - alpha) * previous);
}

static float clamp_dt_s(float dt_s)
{
    if (dt_s <= 0.0f || dt_s > MAX_DT_S) {
        return DEFAULT_DT_S;
    }
    return dt_s;
}

static esp_err_t calibrate_gyro(bsp_mpu6050_t *mpu)
{
    float last_x_offset_dps = 0.0f;
    float last_y_offset_dps = 0.0f;
    float last_z_offset_dps = 0.0f;
    float last_max_range_dps = 0.0f;

    for (uint8_t attempt = 1; attempt <= mpu->config.calibration_max_attempts; attempt++) {
        float gyro_x_total_dps = 0.0f;
        float gyro_y_total_dps = 0.0f;
        float gyro_z_total_dps = 0.0f;
        float gyro_x_min_dps = FLOAT_MAX_INIT;
        float gyro_y_min_dps = FLOAT_MAX_INIT;
        float gyro_z_min_dps = FLOAT_MAX_INIT;
        float gyro_x_max_dps = -FLOAT_MAX_INIT;
        float gyro_y_max_dps = -FLOAT_MAX_INIT;
        float gyro_z_max_dps = -FLOAT_MAX_INIT;

        for (uint16_t i = 0; i < mpu->config.calibration_samples; i++) {
            int16_t accel_x_raw = 0;
            int16_t accel_y_raw = 0;
            int16_t accel_z_raw = 0;
            int16_t gyro_x_raw = 0;
            int16_t gyro_y_raw = 0;
            int16_t gyro_z_raw = 0;
            ESP_RETURN_ON_ERROR(read_motion_raw(mpu,
                                                &accel_x_raw,
                                                &accel_y_raw,
                                                &accel_z_raw,
                                                &gyro_x_raw,
                                                &gyro_y_raw,
                                                &gyro_z_raw),
                                TAG,
                                "gyro calibration");
            (void)accel_x_raw;
            (void)accel_y_raw;
            (void)accel_z_raw;

            const float gyro_x_dps = (float)gyro_x_raw / MPU6050_GYRO_LSB_PER_DPS;
            const float gyro_y_dps = (float)gyro_y_raw / MPU6050_GYRO_LSB_PER_DPS;
            const float gyro_z_dps = (float)gyro_z_raw / MPU6050_GYRO_LSB_PER_DPS;
            gyro_x_total_dps += gyro_x_dps;
            gyro_y_total_dps += gyro_y_dps;
            gyro_z_total_dps += gyro_z_dps;
            gyro_x_min_dps = fminf(gyro_x_min_dps, gyro_x_dps);
            gyro_y_min_dps = fminf(gyro_y_min_dps, gyro_y_dps);
            gyro_z_min_dps = fminf(gyro_z_min_dps, gyro_z_dps);
            gyro_x_max_dps = fmaxf(gyro_x_max_dps, gyro_x_dps);
            gyro_y_max_dps = fmaxf(gyro_y_max_dps, gyro_y_dps);
            gyro_z_max_dps = fmaxf(gyro_z_max_dps, gyro_z_dps);
            esp_rom_delay_us(CALIBRATION_DELAY_US);
        }

        const float samples = (float)mpu->config.calibration_samples;
        last_x_offset_dps = gyro_x_total_dps / samples;
        last_y_offset_dps = gyro_y_total_dps / samples;
        last_z_offset_dps = gyro_z_total_dps / samples;
        const float gyro_x_range_dps = gyro_x_max_dps - gyro_x_min_dps;
        const float gyro_y_range_dps = gyro_y_max_dps - gyro_y_min_dps;
        const float gyro_z_range_dps = gyro_z_max_dps - gyro_z_min_dps;
        last_max_range_dps = fmaxf(gyro_x_range_dps, fmaxf(gyro_y_range_dps, gyro_z_range_dps));

        if (last_max_range_dps <= mpu->config.gyro_stability_threshold_dps) {
            mpu->gyro_x_offset_dps = last_x_offset_dps;
            mpu->gyro_y_offset_dps = last_y_offset_dps;
            mpu->gyro_z_offset_dps = last_z_offset_dps;
            ESP_LOGI(TAG,
                     "Gyro calibrado: offset=(%.3f, %.3f, %.3f)dps range=%.3fdps tentativa=%u",
                     mpu->gyro_x_offset_dps,
                     mpu->gyro_y_offset_dps,
                     mpu->gyro_z_offset_dps,
                     last_max_range_dps,
                     (unsigned int)attempt);
            return ESP_OK;
        }

        ESP_LOGW(TAG,
                 "Mesa ainda em movimento durante calibracao do gyro: range=%.3fdps limite=%.3fdps tentativa=%u",
                 last_max_range_dps,
                 mpu->config.gyro_stability_threshold_dps,
                 (unsigned int)attempt);
    }

    mpu->gyro_x_offset_dps = last_x_offset_dps;
    mpu->gyro_y_offset_dps = last_y_offset_dps;
    mpu->gyro_z_offset_dps = last_z_offset_dps;
    ESP_LOGW(TAG,
             "Gyro calibrado com a ultima janela, ainda com movimento: offset=(%.3f, %.3f, %.3f)dps range=%.3fdps",
             mpu->gyro_x_offset_dps,
             mpu->gyro_y_offset_dps,
             mpu->gyro_z_offset_dps,
             last_max_range_dps);
    return ESP_OK;
}

bsp_mpu6050_config_t bsp_mpu6050_default_config(void)
{
    return (bsp_mpu6050_config_t) {
        .i2c_port = I2C_NUM_0,
        .sda_gpio = BSP_MPU6050_SDA_GPIO,
        .scl_gpio = BSP_MPU6050_SCL_GPIO,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = 400000U,
        .transaction_timeout_ms = 100,
        .calibration_samples = 200,
        .calibration_max_attempts = 8,
        .gyro_stability_threshold_dps = 1.5f,
        .complementary_alpha = 0.96f,
        .accel_filter_alpha = 0.20f,
    };
}

esp_err_t bsp_mpu6050_init(bsp_mpu6050_t *mpu, const bsp_mpu6050_config_t *config)
{
    if (mpu == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *mpu = (bsp_mpu6050_t) {
        .config = *config,
    };

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = config->i2c_port,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &mpu->bus_handle), TAG, "i2c bus");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->device_address,
        .scl_speed_hz = config->scl_speed_hz,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(mpu->bus_handle, &device_config, &mpu->device_handle),
                        TAG,
                        "i2c device");

    uint8_t who_am_i = 0;
    ESP_RETURN_ON_ERROR(read_regs(mpu, MPU6050_REG_WHO_AM_I, &who_am_i, sizeof(who_am_i)), TAG, "who am i");
    if (who_am_i != MPU6050_WHO_AM_I_EXPECTED) {
        ESP_LOGE(TAG, "MPU6050 inesperado: WHO_AM_I=0x%02x", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(write_reg(mpu, MPU6050_REG_PWR_MGMT_1, 0x00), TAG, "wake");
    ESP_RETURN_ON_ERROR(write_reg(mpu, MPU6050_REG_SMPLRT_DIV, 0x09), TAG, "sample rate");
    ESP_RETURN_ON_ERROR(write_reg(mpu, MPU6050_REG_CONFIG, 0x04), TAG, "dlpf");
    ESP_RETURN_ON_ERROR(write_reg(mpu, MPU6050_REG_GYRO_CONFIG, 0x00), TAG, "gyro range");
    ESP_RETURN_ON_ERROR(write_reg(mpu, MPU6050_REG_ACCEL_CONFIG, 0x00), TAG, "accel range");
    ESP_RETURN_ON_ERROR(calibrate_gyro(mpu), TAG, "gyro calibration");

    mpu->last_read_time_us = esp_timer_get_time();

    return ESP_OK;
}

esp_err_t bsp_mpu6050_read(bsp_mpu6050_t *mpu, bsp_mpu6050_sample_t *sample)
{
    if (mpu == NULL || sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t accel_x_raw = 0;
    int16_t accel_y_raw = 0;
    int16_t accel_z_raw = 0;
    int16_t gyro_x_raw = 0;
    int16_t gyro_y_raw = 0;
    int16_t gyro_z_raw = 0;
    ESP_RETURN_ON_ERROR(read_motion_raw(mpu,
                                        &accel_x_raw,
                                        &accel_y_raw,
                                        &accel_z_raw,
                                        &gyro_x_raw,
                                        &gyro_y_raw,
                                        &gyro_z_raw),
                        TAG,
                        "read motion");

    const float accel_x_g = (float)accel_x_raw / MPU6050_ACCEL_LSB_PER_G;
    const float accel_y_g = (float)accel_y_raw / MPU6050_ACCEL_LSB_PER_G;
    const float accel_z_g = (float)accel_z_raw / MPU6050_ACCEL_LSB_PER_G;

    if (!mpu->accel_filter_initialized) {
        mpu->accel_x_filtered_g = accel_x_g;
        mpu->accel_y_filtered_g = accel_y_g;
        mpu->accel_z_filtered_g = accel_z_g;
        mpu->accel_filter_initialized = true;
    } else {
        mpu->accel_x_filtered_g = ema_filter(mpu->accel_x_filtered_g, accel_x_g, mpu->config.accel_filter_alpha);
        mpu->accel_y_filtered_g = ema_filter(mpu->accel_y_filtered_g, accel_y_g, mpu->config.accel_filter_alpha);
        mpu->accel_z_filtered_g = ema_filter(mpu->accel_z_filtered_g, accel_z_g, mpu->config.accel_filter_alpha);
    }

    const float accel_pitch_deg = atan2f(-mpu->accel_x_filtered_g,
                                         sqrtf((mpu->accel_y_filtered_g * mpu->accel_y_filtered_g) +
                                               (mpu->accel_z_filtered_g * mpu->accel_z_filtered_g))) *
                                  DEG_PER_RAD;
    const float accel_roll_deg = atan2f(mpu->accel_y_filtered_g, mpu->accel_z_filtered_g) * DEG_PER_RAD;
    const float gyro_x_dps = ((float)gyro_x_raw / MPU6050_GYRO_LSB_PER_DPS) - mpu->gyro_x_offset_dps;
    const float gyro_y_dps = ((float)gyro_y_raw / MPU6050_GYRO_LSB_PER_DPS) - mpu->gyro_y_offset_dps;
    const float gyro_z_dps = ((float)gyro_z_raw / MPU6050_GYRO_LSB_PER_DPS) - mpu->gyro_z_offset_dps;

    const int64_t now_us = esp_timer_get_time();
    const float dt_s = clamp_dt_s((float)(now_us - mpu->last_read_time_us) / 1000000.0f);
    mpu->last_read_time_us = now_us;

    if (!mpu->angle_filter_initialized) {
        mpu->pitch_filtered_deg = accel_pitch_deg;
        mpu->roll_filtered_deg = accel_roll_deg;
        mpu->angle_filter_initialized = true;
    } else {
        const float alpha = mpu->config.complementary_alpha;
        const float gyro_pitch_deg = mpu->pitch_filtered_deg + (gyro_y_dps * dt_s);
        const float gyro_roll_deg = mpu->roll_filtered_deg + (gyro_x_dps * dt_s);
        mpu->pitch_filtered_deg = (alpha * gyro_pitch_deg) + ((1.0f - alpha) * accel_pitch_deg);
        mpu->roll_filtered_deg = (alpha * gyro_roll_deg) + ((1.0f - alpha) * accel_roll_deg);
    }

    *sample = (bsp_mpu6050_sample_t) {
        .accel_x_raw = accel_x_raw,
        .accel_y_raw = accel_y_raw,
        .accel_z_raw = accel_z_raw,
        .gyro_x_raw = gyro_x_raw,
        .gyro_y_raw = gyro_y_raw,
        .gyro_z_raw = gyro_z_raw,
        .accel_x_g = accel_x_g,
        .accel_y_g = accel_y_g,
        .accel_z_g = accel_z_g,
        .gyro_x_dps = gyro_x_dps,
        .gyro_y_dps = gyro_y_dps,
        .gyro_z_dps = gyro_z_dps,
        .accel_pitch_deg = accel_pitch_deg,
        .accel_roll_deg = accel_roll_deg,
        .pitch_deg = mpu->pitch_filtered_deg,
        .roll_deg = mpu->roll_filtered_deg,
        .dt_s = dt_s,
    };

    return ESP_OK;
}
