idf_component_register(
    SRCS "src/bsp_servo.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_mcpwm esp_driver_gpio
)
