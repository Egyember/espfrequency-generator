#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_default.h"
#include "freertos/idf_additions.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include "freertos/task.h"
#include "lwip/inet.h"
#include "broadcaster.h"
#include "commands.h"

#define SSID "Látlak"
#define PASSWD "Zsombika70671"
#define AUTMETOD WIFI_AUTH_WPA2_PSK
#define WIFICHANEL 1
#define WIFIDONEBIT BIT0
#define NUMBEROFCON 16

#define COMPORT 40001

static const char *TAG = "wifi_teszt";
EventGroupHandle_t wifi_eventGroup;
<<<<<<< HEAD
struct localIp {
  uint32_t ip;
  uint32_t mask;
  SemaphoreHandle_t lock;
  TaskHandle_t broadcaster;
};
struct connection {
  int fd;
  struct sockaddr_storage target;
  bool valid; //used to indicate open connetions
};

void validConnections(const struct connection *connections, const unsigned int conectionsNumber,
                      int *target, int *targetId ,const  unsigned int targetSize) {
	int output =0;
	for (int i = 0; i<conectionsNumber; i++) {
		if (connections[i].valid) {
			if (output < targetSize) {
				target[output] = connections[i].fd;
				targetId[output] = i;
				output ++;
			}	
		}
	}
};

/*
 *used to get first avalable connection slot index
 -1 if no connection avalable
 */
int unusedConnections(const struct connection *connections, unsigned int N){
	for (int i = 0; i < N; i++) {
		if (!connections[i].valid) {
			return i;
		}
	}
	return -1;

};

void closeConnection(struct connection *connection){
	connection->valid = false;
	close(connection->fd);
};

struct localIp *localIp;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    ESP_LOGI(TAG, "wifi event");
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "connected to wifi");
      esp_err_t err = esp_netif_dhcpc_start(arg);
      if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGI(TAG, "dhcp aready running");
      } else {
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

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                      void *event_data) {
  if (event_base == IP_EVENT) {
    ESP_LOGI(TAG, "ip event");
    if (event_id == IP_EVENT_STA_GOT_IP) {
      // ESP_ERROR_CHECK(esp_netif_dhcpc_stop(arg));
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
      xSemaphoreTake(localIp->lock, portMAX_DELAY);
      localIp->ip = event->ip_info.ip.addr;
      localIp->mask = event->ip_info.netmask.addr;
      xTaskCreate(broadcaster, "broadcaster", 1024 * 2, localIp,
                  tskIDLE_PRIORITY, &(localIp->broadcaster));
      xSemaphoreGive(localIp->lock);
      xEventGroupSetBits(wifi_eventGroup, WIFIDONEBIT);

    } else if (event_id == IP_EVENT_STA_LOST_IP) {
      xSemaphoreTake(localIp->lock, portMAX_DELAY);
      vTaskDelete(localIp->broadcaster);
      xSemaphoreGive(localIp->lock);
    };
  }
};

void app_main(void) {
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
  localIp = malloc(sizeof *localIp);
  localIp->lock = xSemaphoreCreateMutex();

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, netint, &wifieventh));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, netint, &ipeventh));
  wifi_config_t wificonfig = {
      .sta =
          {
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
  xEventGroupWaitBits(wifi_eventGroup, WIFIDONEBIT, pdFALSE, pdFALSE,
                      portMAX_DELAY);

  ESP_LOGI(TAG, "done!!!!!");
  int soc = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr.s_addr = INADDR_ANY,
                             .sin_port = htons(COMPORT)

  };
  int err = bind(soc, (struct sockaddr *)&addr, sizeof(addr));
  if (err < 0) {
    ESP_LOGE(TAG, "failed to bind soc jumping to cleanup");
    close(soc);
    goto CLEANUP;
  }
  err = listen(soc, 1);
  if (err < 0) {
    ESP_LOGE(TAG, "failed to lisen soc jumping to cleanup");
    close(soc);
    goto CLEANUP;
  }
  struct timeval zerotime = {0};
  struct connection cons[NUMBEROFCON];
  memset(cons, 0, sizeof(struct connection)*NUMBEROFCON);
  while (true) {
	select(1, fd_set *restrict readfds, NULL, , struct timeval *restrict timeout)
	

    command *com = readCommand(confd);
    ESP_LOGI(TAG, "got command");
    if (com == NULL) {
      close(confd);
      close(soc);
      freeCommand(com);
      goto CLEANUP;
    };
    doCommand(com);
    freeCommand(com);
  }
  close(soc);
  vTaskDelay(60 * 1000 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "bro");
CLEANUP:
  esp_wifi_stop();
  esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifieventh);
  esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipeventh);
  esp_wifi_deinit();
  esp_netif_destroy_default_wifi(netint);
  esp_netif_deinit();
  esp_event_loop_delete_default();
  nvs_flash_deinit();
}
=======
struct localIp{
	uint32_t ip;
	uint32_t mask;
	SemaphoreHandle_t lock;
	TaskHandle_t broadcaster;
};
struct  localIp *localIp;

void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data){
	if (event_base == WIFI_EVENT) {
		ESP_LOGI(TAG, "wifi event");
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
		ESP_LOGI(TAG, "ip event");
		if (event_id == IP_EVENT_STA_GOT_IP) {
			//ESP_ERROR_CHECK(esp_netif_dhcpc_stop(arg));
			ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
			ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
			xSemaphoreTake(localIp->lock, portMAX_DELAY);
			localIp->ip = event->ip_info.ip.addr;
			localIp->mask = event->ip_info.netmask.addr;
			xTaskCreate(broadcaster, "broadcaster", 1024*2, localIp, tskIDLE_PRIORITY, &(localIp->broadcaster));
			xSemaphoreGive(localIp->lock);
			xEventGroupSetBits(wifi_eventGroup, WIFIDONEBIT);
		
		}else if (event_id == IP_EVENT_STA_LOST_IP) {
			xSemaphoreTake(localIp->lock, portMAX_DELAY);
			vTaskDelete(localIp->broadcaster);
			xSemaphoreGive(localIp->lock);
		};
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
	localIp = malloc(sizeof *localIp);
	localIp->lock = xSemaphoreCreateMutex();

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
	int soc = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(COMPORT)

	};
	int err = bind(soc, (struct sockaddr *) & addr, sizeof(addr));
	if (err <0) {
		ESP_LOGE(TAG, "failed to bind soc jumping to cleanup");
		close(soc);
		goto CLEANUP;
	}
	err = listen(soc, 1);
	if (err <0) {
		ESP_LOGE(TAG, "failed to lisen soc jumping to cleanup");
		close(soc);
		goto CLEANUP;
	}
	struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int confd = accept(soc, (struct sockaddr *)&source_addr, &addr_len);
	ESP_LOGI(TAG, "got connection");
	while (true) {
		command *com = readCommand(confd);
		ESP_LOGI(TAG, "got command");
		if (com == NULL) {
			close(confd);
			close(soc);
			freeCommand(com);
			goto CLEANUP;
		};
		doCommand(com);
		freeCommand(com);
	}
	close(confd);
	close(soc);
	vTaskDelay(60*1000 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "bro");
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
>>>>>>> pc/master
