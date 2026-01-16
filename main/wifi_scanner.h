#pragma once

#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief Initialize WiFi in station mode for scanning
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_scanner_init(void);

/**
 * @brief Start WiFi scanning task
 * 
 * Creates a task that scans for WiFi networks every 2 seconds
 * and updates the UI with the results
 * 
 * @param parent_screen Parent LVGL screen to create UI on
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_scanner_start(lv_obj_t *parent_screen);

/**
 * @brief Stop WiFi scanning task
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_scanner_stop(void);

/**
 * @brief Get the LVGL container object for the WiFi list
 * 
 * @return Pointer to the WiFi list container or NULL if not initialized
 */
lv_obj_t *wifi_scanner_get_list_container(void);
