idf_component_register(
    SRCS "src/app_labyrinth.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_joystick bsp_servo rtos_port esp_common log
)
