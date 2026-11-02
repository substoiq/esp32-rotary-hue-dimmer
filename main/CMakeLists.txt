idf_component_register(
    SRCS "main.c" "dimmer.c" "display_port.c" "input.c" "wifi.c"
         "hue.c" "hue_json.c" "hue_sse.c" "ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_client esp-tls json
             esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_pcnt esp_timer
)
