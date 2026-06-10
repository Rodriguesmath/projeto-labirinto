idf_component_register(
    SRCS "src/bsp_mpu6050.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_i2c esp_common esp_rom esp_timer log
)

target_link_libraries(${COMPONENT_LIB} PRIVATE m)
