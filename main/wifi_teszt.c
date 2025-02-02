#include "broadcaster.h"
#include "btconfig.h"
#include "commands.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sys/poll.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WIFIDONEBIT BIT0
#define WIFIFAILED BIT1
#define NUMBEROFCON 14

#define COMPORT 40001

#define QUEUELENGTH 32

static const char *MAIN_TAG = "MAIN";
EventGroupHandle_t wifi_eventGroup;
struct localIp {
	uint32_t ip;
	uint32_t mask;
	SemaphoreHandle_t lock;
	TaskHandle_t broadcaster;
};
struct connection {
	int fd;
	struct sockaddr_storage target;
	bool valid; // used to indicate open connetions
};

int getValidConnectionsNum(const struct connection *connections, const unsigned int conectionsNumber, int *target,
			   int *targetId, const unsigned int targetSize) {
	int output = 0;
	for(int i = 0; i < conectionsNumber; i++) {
		if(connections[i].valid) {
			if(output < targetSize) {
				target[output] = connections[i].fd;
				targetId[output] = i;
				output++;
			}
		}
	}
	return output;
};

/*
 *used to get first avalable connection slot index
 -1 if no connection avalable
 */
int getFreeConnection(const struct connection *connections, unsigned int N) {
	for(int i = 0; i < N; i++) {
		if(!connections[i].valid) {
			return i;
		}
	}
	return -1;
};

void closeConnection(struct connection *connection) {
	connection->valid = false;
	close(connection->fd);
};

struct localIp *localIp;

int retry = 0;
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if(event_base == WIFI_EVENT) {
		ESP_LOGI(MAIN_TAG, "wifi event");
		switch(event_id) {
		case WIFI_EVENT_STA_START:
			esp_wifi_connect();
			break;
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(MAIN_TAG, "connected to wifi");
			esp_err_t err = esp_netif_dhcpc_start(arg);
			if(err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
				ESP_LOGI(MAIN_TAG, "dhcp aready running");
			};
			break;
		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(MAIN_TAG, "disconected trying to reconnect");
			retry++;
			if(retry > 3) {
				xEventGroupSetBits(wifi_eventGroup, WIFIFAILED);
				break;
			};
			esp_wifi_connect();
			break;
		default:
			ESP_LOGI(MAIN_TAG, "unhandelered event");
		}
	}
};

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if(event_base == IP_EVENT) {
		ESP_LOGI(MAIN_TAG, "ip event");
		if(event_id == IP_EVENT_STA_GOT_IP) {
			// ESP_ERROR_CHECK(esp_netif_dhcpc_stop(arg));
			ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
			ESP_LOGI(MAIN_TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
			xSemaphoreTake(localIp->lock, portMAX_DELAY);
			localIp->ip = event->ip_info.ip.addr;
			localIp->mask = event->ip_info.netmask.addr;
			xTaskCreate(broadcaster, "broadcaster", 1024 * 2, localIp, tskIDLE_PRIORITY,
				    &(localIp->broadcaster));
			xSemaphoreGive(localIp->lock);
			xEventGroupSetBits(wifi_eventGroup, WIFIDONEBIT);
			retry = 0;

		} else if(event_id == IP_EVENT_STA_LOST_IP) {
			ESP_LOGI(MAIN_TAG, "lost ip address");
			xSemaphoreTake(localIp->lock, portMAX_DELAY);
			vTaskDelete(localIp->broadcaster);
			xSemaphoreGive(localIp->lock);
		};
	}
};

void commandloop(QueueHandle_t *queue) {
	if(queue == NULL) {
		ESP_LOGE(MAIN_TAG, "worng queue");
		return;
	};
	while(true) {
		command *com = NULL;
		ESP_LOGI(MAIN_TAG, "ok");
		xQueueReceive(*queue, &com, portMAX_DELAY);
		ESP_LOGI(MAIN_TAG, "?ok");
		if(com != NULL) {
			doCommand(com);
			freeCommand(com);
		};
	}
}
void app_main(void) {
	ESP_ERROR_CHECK(nvs_flash_init());

	ESP_LOGI(MAIN_TAG, "seting up wifi");
	wifi_eventGroup = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_netif_init());
	esp_netif_t *netint = esp_netif_create_default_wifi_sta();
	if(netint == NULL) {
		ESP_LOGE(MAIN_TAG, "netif == NULL");
		ESP_LOGE(MAIN_TAG, "jumping to cleanup");
		goto CLEANUP;
	}
	wifi_init_config_t wifiInitconf = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifiInitconf));
	esp_event_handler_instance_t wifieventh;
	esp_event_handler_instance_t ipeventh;
	localIp = malloc(sizeof *localIp);
	localIp->lock = xSemaphoreCreateMutex();

	ESP_ERROR_CHECK(
	    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, netint, &wifieventh));
	ESP_ERROR_CHECK(
	    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, netint, &ipeventh));

	wifi_config_t wificonfig = {};
WIFISETUP:
	// reading config from nvs
	nvs_handle_t nvsHandle;
	{
		nvs_open("wifi_auth", NVS_READONLY, &nvsHandle);
		size_t wifiLen;
		nvs_get_str(nvsHandle, "ssid", NULL, &wifiLen);
		if(wifiLen > 32) {
			ESP_LOGE(MAIN_TAG, "too long ssid stored in nvs. Jumping to bt setup");
			btconfig(); // todo: implemnet
			retry = 0;
			goto WIFISETUP;
		};
		nvs_get_str(nvsHandle, "ssid", (char *)&wificonfig.sta.ssid, &wifiLen);

		nvs_get_str(nvsHandle, "passwd", NULL, &wifiLen);
		if(wifiLen > 64) { // implemention limit to passwd lenght is 64
			ESP_LOGE(MAIN_TAG, "too long ssid stored in nvs. Jumping to bt setup");
			btconfig(); // todo: implemnet
			goto WIFISETUP;
		};
		nvs_get_str(nvsHandle, "passwd", (char *)&wificonfig.sta.password, &wifiLen);

		uint8_t auth;
		nvs_get_u8(nvsHandle, "authmetod", (uint8_t *)&auth);
		wificonfig.sta.threshold.authmode = auth;
		nvs_get_u8(nvsHandle, "channel", &wificonfig.sta.channel);
	};
	ESP_LOGI(MAIN_TAG, "ssid: %s, password:%s, channel: %d", wificonfig.sta.ssid, wificonfig.sta.password, wificonfig.sta.channel);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wificonfig));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(MAIN_TAG, "wifi_init_sta finished.");
	ESP_LOGI(MAIN_TAG, "event magic started");
	EventBits_t bits = xEventGroupWaitBits(wifi_eventGroup, WIFIDONEBIT | WIFIFAILED, pdTRUE, pdFALSE,
					       portMAX_DELAY); // todo: add fail bit and check
	if (bits | WIFIFAILED) {
		ESP_LOGI(MAIN_TAG, "event magic failed entering config mode");
		btconfig();
		ESP_LOGI(MAIN_TAG, "Jumping to wifi setup");
		goto WIFISETUP;
	};

	ESP_LOGI(MAIN_TAG, "wifi done");

	// command queue
	TaskHandle_t queueExecuter;
	QueueHandle_t commandQueue = xQueueCreate(QUEUELENGTH, sizeof(command *));
	xTaskCreate((void (*)(void *))commandloop, "command loop", 1024 * 5, &commandQueue, tskIDLE_PRIORITY + 1,
		    &queueExecuter);
	// starting tcp server
	int soc = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(COMPORT)

	};
	int err = bind(soc, (struct sockaddr *)&addr, sizeof(addr));
	if(err < 0) {
		ESP_LOGE(MAIN_TAG, "failed to bind soc jumping to cleanup");
		close(soc);
		goto CLEANUPWITHSOC;
	}
	err = listen(soc, 1);
	if(err < 0) {
		ESP_LOGE(MAIN_TAG, "failed to lisen soc jumping to cleanup");
		close(soc);
		goto CLEANUPWITHSOC;
	}
	struct pollfd lisener = {
	    .fd = soc,
	    .events = POLLIN | POLLERR,
	    .revents = 0,
	};
	struct connection cons[NUMBEROFCON];
	memset(cons, 0, sizeof(struct connection) * NUMBEROFCON);
	int fds[NUMBEROFCON];
	int fdids[NUMBEROFCON];
	int fdnum;
	socklen_t sockaddr_storage_len = sizeof(struct sockaddr_storage);
	while(true) {
		poll(&lisener, 1, 0);
		if(lisener.revents & POLLIN) {
			int connectionId = getFreeConnection(cons, NUMBEROFCON);
			if(connectionId < 0) {
				ESP_LOGI(MAIN_TAG, "no free connection slot left");
				goto DONTADDCON;
			}
			cons[connectionId].fd =
			    lwip_accept(soc, (struct sockaddr *)&cons[connectionId].target, &sockaddr_storage_len);
			if(cons[connectionId].fd < 0) {
				cons[connectionId].valid = false;
			} else {

				cons[connectionId].valid = true;
			};
		}
		if(lisener.revents & POLLERR) {
			ESP_LOGE(MAIN_TAG, "failed to lisen soc jumping to cleanup");
			goto CLEANUPWITHSOC;
		}
	DONTADDCON:
		fdnum = getValidConnectionsNum(cons, NUMBEROFCON, fds, fdids, NUMBEROFCON);
		struct pollfd pollfds[fdnum];
		for(int i = 0; i < fdnum; i++) {
			pollfds[i].fd = fds[i];
			pollfds[i].events = POLLIN | POLLERR;
			pollfds[i].revents = 0;
		}
		if(fdnum == 0) {
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}
		ESP_LOGI(MAIN_TAG, "2th poll loop");
		poll((struct pollfd *)&pollfds, fdnum, -1);
		ESP_LOGI(MAIN_TAG, "2th poll done loop");
		for(int i = 0; i < fdnum; i++) {
			printf("pollfds[%d].revents = %x\n", i, pollfds[i].revents);
			if(pollfds[i].revents & POLLERR) {
				ESP_LOGI(MAIN_TAG, "cleaing up bad connection");
				closeConnection(&(cons[fdids[i]]));
				continue;
			}
			if(pollfds[i].revents & POLLIN) {
				command *com = readCommand(pollfds[i].fd);
				ESP_LOGI(MAIN_TAG, "got command");
				if(com == NULL) {
					ESP_LOGE(MAIN_TAG, "NULL command");
					ESP_LOGE(MAIN_TAG, "closing connection");
					closeConnection(&(cons[fdids[i]]));
					continue;
				};
				ESP_LOGI(MAIN_TAG, "adding to queue");
				if(xQueueSend(commandQueue, &com, 0) == errQUEUE_FULL) {
					freeCommand(com);
					ESP_LOGI(MAIN_TAG, "command queue full dropping command");
				};
			};
		};
	}
CLEANUPWITHSOC:
	close(soc);
	vTaskDelete(queueExecuter);
	vQueueDelete(commandQueue);
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
