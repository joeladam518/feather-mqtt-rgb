idf_component_register(
    SRCS
        "debug.cpp"
        "main.cpp"
        "MqttEventProcessing.cpp"
        "NeoPixelRing.cpp"
        "wifi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_driver_gpio
        esp_event
        esp_netif
        esp_wifi
        nvs_flash
)
