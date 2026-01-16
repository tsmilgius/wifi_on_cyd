#include "wifi_scanner.h"
#include "cyd_display_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <string.h>

static const char *TAG = "wifi_scanner";

#define WIFI_SCAN_INTERVAL_MS 2000
#define MAX_AP_COUNT 20

static TaskHandle_t s_scan_task_handle = NULL;
static lv_obj_t *s_list_container = NULL;
static lv_obj_t *s_list_labels[MAX_AP_COUNT] = {0};
static lv_obj_t *s_exit_button = NULL;
static bool s_wifi_initialized = false;

/* WiFi scan result structure */
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_info_t;

/* Forward declarations */
static void exit_button_event_cb(lv_event_t *e);

static void create_wifi_list_ui(lv_obj_t *parent_screen)
{
    /* Lock LVGL before making UI updates from another task */
    lv_lock();

    /* Create main container for WiFi list */
    s_list_container = lv_obj_create(parent_screen);
    lv_obj_set_size(s_list_container, LCD_H_RES - 20, LCD_V_RES - 40);
    lv_obj_align(s_list_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(s_list_container, lv_color_make(20, 20, 40), 0);
    lv_obj_set_style_bg_opa(s_list_container, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s_list_container, lv_color_make(100, 100, 150), 0);
    lv_obj_set_style_border_width(s_list_container, 2, 0);
    lv_obj_set_style_pad_all(s_list_container, 10, 0);
    lv_obj_set_flex_flow(s_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_list_container, 5, 0);
    lv_obj_set_scrollbar_mode(s_list_container, LV_SCROLLBAR_MODE_OFF);

    /* Create title label and exit button container */
    lv_obj_t *header_container = lv_obj_create(s_list_container);
    lv_obj_set_size(header_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_container, 0, 0);
    lv_obj_set_style_pad_all(header_container, 0, 0);
    lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Add exit button on the left */
    s_exit_button = lv_button_create(header_container);
    lv_obj_set_size(s_exit_button, 60, 30);
    lv_obj_set_style_bg_color(s_exit_button, lv_color_make(200, 0, 0), LV_PART_MAIN);
    lv_obj_add_event_cb(s_exit_button, exit_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *exit_label = lv_label_create(s_exit_button);
    lv_label_set_text(exit_label, "Exit");
    lv_obj_center(exit_label);
    lv_obj_set_style_text_color(exit_label, lv_color_white(), 0);

    /* Create title label */
    lv_obj_t *title = lv_label_create(header_container);
    lv_label_set_text(title, "WiFi Networks");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_flex_grow(title, 1);

    /* Create a separator */
    lv_obj_t *separator = lv_obj_create(s_list_container);
    lv_obj_set_size(separator, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(separator, lv_color_make(100, 100, 150), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(separator, 0, 0);

    /* Pre-create labels for WiFi networks */
    for (int i = 0; i < MAX_AP_COUNT; i++) {
        s_list_labels[i] = lv_label_create(s_list_container);
        lv_label_set_text(s_list_labels[i], "");
        lv_obj_set_style_text_color(s_list_labels[i], lv_color_white(), 0);
        lv_obj_set_width(s_list_labels[i], LV_PCT(100));
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_add_flag(s_list_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Add scanning indicator */
    lv_obj_t *scan_label = lv_label_create(s_list_container);
    lv_label_set_text(scan_label, "Scanning...");
    lv_obj_set_style_text_color(scan_label, lv_color_make(150, 150, 150), 0);

    /* Unlock LVGL */
    lv_unlock();
}

static const char *get_auth_mode_name(wifi_auth_mode_t authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "?";
    }
}

static int compare_rssi(const void *a, const void *b)
{
    const wifi_ap_info_t *ap_a = (const wifi_ap_info_t *)a;
    const wifi_ap_info_t *ap_b = (const wifi_ap_info_t *)b;
    return ap_b->rssi - ap_a->rssi; /* Sort descending */
}

static void exit_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        /* Hide the WiFi scanner container */
        if (s_list_container) {
            lv_obj_add_flag(s_list_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_wifi_list_ui(wifi_ap_info_t *ap_list, uint16_t ap_count)
{
    if (!s_list_container) {
        return;
    }

    /* Lock LVGL before making UI updates from another task */
    lv_lock();

    /* Sort by signal strength */
    qsort(ap_list, ap_count, sizeof(wifi_ap_info_t), compare_rssi);

    /* Update labels */
    for (int i = 0; i < MAX_AP_COUNT; i++) {
        if (i < ap_count) {
            char buf[100];
            snprintf(buf, sizeof(buf), "%s (%ddBm) [%s]",
                    ap_list[i].ssid,
                    ap_list[i].rssi,
                    get_auth_mode_name(ap_list[i].authmode));
            lv_label_set_text(s_list_labels[i], buf);
            lv_obj_remove_flag(s_list_labels[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_list_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Unlock LVGL */
    lv_unlock();
}

static void wifi_scan_task(void *pvParameters)
{
    (void)pvParameters;
    wifi_ap_info_t ap_list[MAX_AP_COUNT];
    
    ESP_LOGI(TAG, "WiFi scan task started");

    while (1) {
        /* Start WiFi scan */
        wifi_scan_config_t scan_config = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active.min = 0,
            .scan_time.active.max = 0,
        };

        esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
            continue;
        }

        /* Yield to allow watchdog reset */
        vTaskDelay(1);

        /* Get scan results */
        uint16_t ap_count = 0;
        ret = esp_wifi_scan_get_ap_num(&ap_count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
            continue;
        }

        if (ap_count > MAX_AP_COUNT) {
            ap_count = MAX_AP_COUNT;
        }

        if (ap_count > 0) {
            wifi_ap_record_t ap_records[MAX_AP_COUNT];
            ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
                continue;
            }

            /* Copy to our simplified structure */
            for (int i = 0; i < ap_count; i++) {
                strncpy(ap_list[i].ssid, (char *)ap_records[i].ssid, sizeof(ap_list[i].ssid) - 1);
                ap_list[i].ssid[sizeof(ap_list[i].ssid) - 1] = '\0';
                ap_list[i].rssi = ap_records[i].rssi;
                ap_list[i].authmode = ap_records[i].authmode;
            }

            ESP_LOGI(TAG, "Found %d WiFi networks", ap_count);

            /* Update UI in LVGL context */
            update_wifi_list_ui(ap_list, ap_count);
        } else {
            ESP_LOGI(TAG, "No WiFi networks found");
            update_wifi_list_ui(NULL, 0);
        }

        /* Wait before next scan */
        vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
    }
}

esp_err_t wifi_scanner_init(void)
{
    if (s_wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return ESP_OK;
    }

    /* Initialize NVS (required by WiFi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize network interface */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create event loop */
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create WiFi station interface */
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
        ESP_LOGE(TAG, "Failed to create WiFi STA interface");
        return ESP_FAIL;
    }

    /* Initialize WiFi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set WiFi mode to station */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Start WiFi */
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized for scanning");
    return ESP_OK;
}

esp_err_t wifi_scanner_start(lv_obj_t *parent_screen)
{
    if (!s_wifi_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized. Call wifi_scanner_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_scan_task_handle != NULL) {
        ESP_LOGW(TAG, "Scan task already running");
        /* Make sure container is visible */
        if (s_list_container) {
            lv_lock();
            lv_obj_remove_flag(s_list_container, LV_OBJ_FLAG_HIDDEN);
            lv_unlock();
        }
        return ESP_OK;
    }

    /* Create UI */
    create_wifi_list_ui(parent_screen);

    /* Create scan task */
    BaseType_t ret = xTaskCreate(
        wifi_scan_task,
        "wifi_scan",
        8192,
        NULL,
        5,
        &s_scan_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi scan task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WiFi scanner started");
    return ESP_OK;
}

esp_err_t wifi_scanner_stop(void)
{
    if (s_scan_task_handle != NULL) {
        vTaskDelete(s_scan_task_handle);
        s_scan_task_handle = NULL;
        ESP_LOGI(TAG, "WiFi scan task stopped");
    }
    return ESP_OK;
}

lv_obj_t *wifi_scanner_get_list_container(void)
{
    return s_list_container;
}
