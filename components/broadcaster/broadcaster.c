#include "lwip/sockets.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <unistd.h>
#include <stdbool.h>


struct localIp{
	uint32_t ip;
	uint32_t mask;
	SemaphoreHandle_t lock;
	TaskHandle_t broadcaster;
};

static const char *BRTAG = "broadcaster";
static char *brbuff = "hello sir";
static void sendBroadcast(int soc){
	int err = send(soc, brbuff, strlen(brbuff), 0);
	if (err<0) {
		ESP_LOGI(BRTAG, "failed to send");
	};
	return;

};
void broadcaster(struct localIp* locIp){
	ESP_LOGI(BRTAG, "starting broadcaster");
	if (locIp == NULL) {
		ESP_LOGE(BRTAG, "null arg");
		return;
	}
	int soc = socket(AF_INET, SOCK_DGRAM, 0);
	if (soc < 0) {
		ESP_LOGE(BRTAG, "Unable to create socket errno: %d", errno);
	}
	struct sockaddr_in  dest;
	xSemaphoreTake(locIp->lock, portMAX_DELAY);
	dest.sin_addr.s_addr = locIp->ip | ~locIp->mask; //brotcast addres
	xSemaphoreGive(locIp->lock);
	dest.sin_family = AF_INET;
	dest.sin_port = htons(40000);
	int err = lwip_connect(soc, (const struct socaddr *) &dest, sizeof(dest));
	if (err<0) {
		return;
	}
	ESP_LOGI(BRTAG, "entering loop");
	while(true){
		ESP_LOGI(BRTAG, "send broadcast data");
		sendBroadcast(soc);
		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
	return;

};

