#include "wifi.h"
#include <string.h>
#include "credentials.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#define BIT_CONNECTED BIT0

static const char *TAG = "wifi";
static EventGroupHandle_t s_flags;

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_flags, BIT_CONNECTED);
        ESP_LOGW(TAG, "verbinding weg, opnieuw proberen");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "verbonden");
        xEventGroupSetBits(s_flags, BIT_CONNECTED);
    }
}

void wifi_start(void)
{
    s_flags = xEventGroupCreate();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    wifi_config_t config = { 0 };
    strncpy((char *)config.sta.ssid, WIFI_SSID, sizeof(config.sta.ssid) - 1);
    strncpy((char *)config.sta.password, WIFI_PASSWORD, sizeof(config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);   /* geen energiebesparing: de knop moet direct reageren */
}

void wifi_wait_connected(void)
{
    xEventGroupWaitBits(s_flags, BIT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
}

bool wifi_get_rssi(int *rssi)
{
    if (!(xEventGroupGetBits(s_flags) & BIT_CONNECTED)) {
        return false;
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return false;
    }
    *rssi = ap.rssi;
    return true;
}
