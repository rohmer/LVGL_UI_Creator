set(EXAMPLE_PB_COMPONENT_NAME "$ENV{LVGL_PB_EXAMPLE_COMPONENT_NAME}")
idf_component_register(SRCS main.c INCLUDE_DIRS .
    REQUIRES ${EXAMPLE_PB_COMPONENT_NAME} esp_lcd esp_driver_spi esp_driver_gpio)
