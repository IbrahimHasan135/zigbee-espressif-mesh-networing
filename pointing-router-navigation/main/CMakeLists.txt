idf_component_register(
    SRCS
        "app_main.cpp"
        "drivers/zigbee/zigbee_router_driver.cpp"
        "drivers/storage/storage_driver.cpp"
        "drivers/status/status_led_driver.cpp"
        "services/config/router_config_service.cpp"
        "services/navigation_protocol/navigation_protocol_service.cpp"
        "services/router_point/router_point_service.cpp"
        "controllers/app_controller.cpp"
        "controllers/zigbee_controller.cpp"
        "controllers/router_point_controller.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_driver_gpio
        espressif__esp-zigbee-lib
)
