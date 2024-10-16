#include <stdio.h>
#include <unistd.h>
#include "cc.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"

#define SSID "tp_doos_2,4GHz"
#define PASSWD "Zsombika70671"
#define AUTMETOD WIFI_AUTH_WPA2_PSK
#define WIFICHANEL 5
#define WIFIDONEBIT BIT0

static const char *TAG = "wifi_teszt";
EventGroupHandle_t wifi_eventGroup;
struct localIp{
	uint32_t ip;
	uint32_t mask;
} localIp;
void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data){
	if (event_base == WIFI_EVENT) {
	
	switch (event_id) {
		case WIFI_EVENT_STA_START:
			esp_wifi_connect();
			break;
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "connected to wifi");
			esp_err_t err = esp_netif_dhcpc_start(arg);
			if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
				ESP_LOGI(TAG, "dhcp aready running");
			} else{
				ESP_ERROR_CHECK(esp_netif_dhcpc_start(arg));
			}
			break;
			case WIFI_EVENT_STA_DISCONNECTED:
				ESP_LOGI(TAG, "disconected trying to reconnect");
				esp_wifi_connect();
				break;
			default:
				ESP_LOGI(TAG, "unhandelered event");
		}
		}
	};

	void ip_event_handler(void* arg, esp_event_base_t event_base,
			int32_t event_id, void* event_data){
	if (event_base == IP_EVENT) {
		if (event_id == IP_EVENT_STA_GOT_IP) {
			//ESP_ERROR_CHECK(esp_netif_dhcpc_stop(arg));
			ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
			ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
			localIp.ip = event->ip_info.ip.addr;
			localIp.mask = event->ip_info.netmask.addr;
			xEventGroupSetBits(wifi_eventGroup, WIFIDONEBIT);
		
		}
	}
	};

	void app_main(void){
		ESP_ERROR_CHECK(nvs_flash_init());
		ESP_LOGI(TAG, "seting up wifi");
		wifi_eventGroup = xEventGroupCreate();
		ESP_ERROR_CHECK(esp_event_loop_create_default());
		ESP_ERROR_CHECK(esp_netif_init());
		esp_netif_t *netint = esp_netif_create_default_wifi_sta();
	if (netint == NULL) {
		ESP_LOGE(TAG, "netif == NULL");
		ESP_LOGE(TAG, "jumping to cleanup");
		goto CLEANUP;
	}
	wifi_init_config_t wifiInitconf = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifiInitconf));
	esp_event_handler_instance_t wifieventh;
	esp_event_handler_instance_t ipeventh;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, netint, &wifieventh));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, netint, &ipeventh));
	wifi_config_t wificonfig = {
		.sta = {
			.ssid = SSID,
			.password = PASSWD,
			.threshold.authmode = AUTMETOD,
			.channel = WIFICHANEL,
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wificonfig));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "event mageic started");
	xEventGroupWaitBits(wifi_eventGroup, WIFIDONEBIT, pdFALSE, pdFALSE, portMAX_DELAY);

	ESP_LOGI(TAG, "done!!!!!");
	int soc = socket(AF_INET, SOCK_DGRAM, 0);
	if (soc < 0) {
		ESP_LOGE(TAG, "Unable to create socket errno: %d", errno);
	}
	struct sockaddr_in  dest;
	dest.sin_addr.s_addr = localIp.ip | ~localIp.mask; //brotcast addres
	dest.sin_family = AF_INET;
	dest.sin_port = htons(40000);
	connect(soc, (struct socaddr *) &dest, sizeof(dest));
	char *brbuff = "hello sir";
	send(soc, brbuff, strlen(brbuff), 0);
	ESP_LOGI(TAG, "send data");
        vTaskDelay(1000*60 / portTICK_PERIOD_MS);

CLEANUP:
	esp_wifi_stop();
	esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifieventh);
	esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipeventh);
	esp_wifi_deinit();
	esp_netif_destroy_default_wifi(netint);
	esp_netif_deinit();
	esp_event_loop_delete_default();
	nvs_flash_deinit();
}
