#include "config.h"
#include "wifi.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#define WIFI_CONNECTED_BIT BIT0

static const char *WIFI_TAG = "WIFI_RGB_TREE";

static EventGroupHandle_t wifiEventGroup = NULL;

static void wifi_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Keep retrying, both for the first connection and after the link drops
        xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
    }
}

void wifiConnect(void)
{
    // The wifi driver stores its calibration data in nvs
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    wifiEventGroup = xEventGroupCreate();
    if (!wifiEventGroup) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, WLAN_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WLAN_PASS, sizeof(wifi_config.sta.password));

    ESP_LOGI(WIFI_TAG, "Connecting to %s", WLAN_SSID);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // The tree is plugged in, and modem sleep makes commands take longer to arrive
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
