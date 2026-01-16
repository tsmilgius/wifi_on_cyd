idf_component_register(
    SRCS "main.c" "cyd_hw.c" "ui.c" "wifi_scanner.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer driver esp_lcd lvgl esp_wifi esp_netif nvs_flash
)
