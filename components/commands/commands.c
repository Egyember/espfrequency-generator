#include "commands.h"
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gpioout.h"
#include "lwip/sockets.h"
#include "sys/ioctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* timeout in ms*/
#define TIMEOUT 10000

static char *commandTag = "COMMANDS";

void IRAM_ATTR ntohlRange(int32_t *buff, uint len) { // I call it a lot so put it in sram
	for(int i = 0; i < len; i++) {
		buff[i] = ntohl(buff[i]);
	};
};

int readWithTimeout(int soc, void *buffer, size_t needed, uint64_t timout) {
	gptimer_handle_t timer = NULL;
	gptimer_config_t timerConfig = {
	    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
	    .direction = GPTIMER_COUNT_UP,
	    .resolution_hz = 10000000,
	};
	ESP_ERROR_CHECK(gptimer_new_timer(&timerConfig, &timer));
	gptimer_enable(timer);
	uint32_t resolution;
	ESP_ERROR_CHECK(gptimer_get_resolution(timer, &resolution));
	gptimer_start(timer);
	bool run = true;
	while(run) {
		uint64_t tick;
		gptimer_get_raw_count(timer, &tick);
		if(tick / resolution > timout / 1000) {
			ESP_LOGW(commandTag, "timeout");
			gptimer_stop(timer);
			gptimer_disable(timer);
			gptimer_del_timer(timer);
			return -1;
		};
		int avalable;
		int err = lwip_ioctl(soc, FIONREAD, &avalable);
		// fuck this shit
		// https://github.com/espressif/esp-idf/issues/6215
		// https://github.com/espressif/esp-idf/issues/319
		if(err < 0) {
			ESP_LOGE(commandTag, "ioctl failed errno: %s", strerror(errno));
			gptimer_stop(timer);
			gptimer_disable(timer);
			gptimer_del_timer(timer);
			return -1;
		};
		if(avalable >= needed) {
			run = false;
		} else {
			vTaskDelay(100 / portTICK_PERIOD_MS);
		};
	};
	gptimer_stop(timer);
	gptimer_disable(timer);
	gptimer_del_timer(timer);
	int err = read(soc, buffer, needed);
	if(err < 0) {
		ESP_LOGE(commandTag, "read failed");
		return -1;
	};
	return 0;
};

command *readCommand(int soc) {
	int32_t commandbuffer[3];
	if(readWithTimeout(soc, &commandbuffer, sizeof commandbuffer, TIMEOUT)) {
		ESP_LOGE(commandTag, "time out");
		return NULL;
	}
	ntohlRange(commandbuffer, 3);
	command *com = malloc(sizeof(command));
	if(com == NULL) { // malloc failed
		ESP_LOGE(commandTag, "failed to alloc memory for command");
		return NULL;
	};
	if(commandbuffer[0] == COMMAND) {
		com->command = commandbuffer[1];
		com->argnum = commandbuffer[2];
	} else {
		ESP_LOGE(commandTag, "non a command");
		free(com);
		return NULL;
	}; // todo: this can error
	if(com->argnum == 0) {
		com->args = NULL; // not initialized when created
		return com;
	};
	int32_t *argbuffer = malloc(sizeof(int32_t) * 3 * com->argnum);
	if(argbuffer == NULL) { // malloc failed
		ESP_LOGE(commandTag, "failed to alloc memory for command argument");
		free(com);
		return NULL;
	};
	size_t needed = sizeof(int32_t) * 3 * com->argnum;

	if(readWithTimeout(soc, argbuffer, needed, TIMEOUT)) {
		ESP_LOGE(commandTag, "arg read failed");
		free(argbuffer);
		free(com);
		return NULL;
	};
	ntohlRange(argbuffer, 3 * com->argnum);
	void *args = NULL;
	switch(com->command) {
	case PLAY:
		args = malloc(sizeof(node) * com->argnum);
		node *nargs = args;
		for(int i = 0; i < com->argnum; i++) {
			if(argbuffer[i * 3] == DATA) {
				nargs[i].freq = argbuffer[1 + i * 3];
				nargs[i].time = argbuffer[2 + i * 3];
			} else {
				ESP_LOGE(commandTag, "invalid data packet");
				free(args);
				free(com);
				free(argbuffer);
				return NULL;
			}
		}
		break;
	default:
		ESP_LOGI(commandTag, "not implemented command");
		break;
	}
	free(argbuffer);
	com->args = args;
	return com;
};

void doCommand(command *comm) {
	ESP_LOGI(commandTag, "doing command?");
	switch(comm->command) {
	case INVALID:
		ESP_LOGI(commandTag, "invalid command");
		return;
		break;
	case PLAY:
		ESP_LOGI(commandTag, "play command");
		node *arg = comm->args;
		dac_cosine_handle_t handler = startFreq(arg->freq);
		vTaskDelay(arg->time / portTICK_PERIOD_MS);
		if(comm->argnum > 1) {
			for(int i = 1; i < comm->argnum; i++) {
				setFreq(arg[i].freq);
				vTaskDelay(arg[i].time / portTICK_PERIOD_MS);
			}
		}
		stopFreq(handler);
		break;
	default:
		ESP_LOGI(commandTag, "not implemented command");
		break;
	};
	return;
};

void freeCommand(command *command) {
	if(command == NULL) {
		ESP_LOGW(commandTag, "NULL command free atempt");
		return;
	}
	if(command->command == PLAY) {
		free(command->args);
	}
	free(command);
};
