idf_component_register(
    SRCS "src/bsp_joystick.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_adc
)
