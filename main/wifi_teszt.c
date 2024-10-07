#include <stdio.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi_teszt";

void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data){
};


void app_main(void){
	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_LOGI(TAG, "seting up wifi");
	EventGroupHandle_t wifi_eventGroup = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_netif_init());
	esp_netif_t *netint = esp_netif_create_default_wifi_sta();
	if (netint == NULL) {
		ESP_LOGE(TAG, "netif == NULL");
		ESP_LOGE(TAG, "jumping to cleanup");
		goto CLEANUP;
	}
	wifi_init_config_t wifiConf = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifiConf));
	esp_event_handler_instance_t wifieventh;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, netint, &wifieventh));

CLEANUP:
	esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifieventh);
	esp_wifi_deinit();
	esp_netif_destroy_default_wifi(netint);
	esp_netif_deinit();
	esp_event_loop_delete_default();
	nvs_flash_deinit();
}
