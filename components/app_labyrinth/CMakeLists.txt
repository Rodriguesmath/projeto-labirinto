idf_component_register(
    SRCS "src/app_labyrinth.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_joystick bsp_mpu6050 bsp_servo bsp_victory_sensor rtos_port esp_common log
)
