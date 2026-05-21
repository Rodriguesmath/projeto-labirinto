idf_component_register(
    SRCS "src/bsp_status_led.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_gpio
)
