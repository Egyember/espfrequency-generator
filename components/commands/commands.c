#include "commands.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gpioout.h"
#include "lwip/sockets.h"
#include "sys/ioctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char *commandTag = "COMMANDS";

void ntohlRange(int32_t *buff, uint len) {
	for(int i = 0; i < len; i++) {
		buff[i] = ntohl(buff[i]);
	};
};

command *readCommand(int soc) {
	int avalable;
	int err = ioctl(soc, FIONREAD, &avalable);
	if(err < 0) {
		ESP_LOGE(commandTag, "ioctl failed");
		return NULL;
	}
	printf("avalable: %d\n", avalable);
	if(avalable < (3 * sizeof(int))) {
		return NULL;
	};
	avalable = avalable % (3 * sizeof(int));
	int32_t commandbuffer[3];
	err = read(soc, commandbuffer, sizeof(int32_t) * 3);
	if(err < 0) {
		ESP_LOGE(commandTag, "read failed");
		return NULL;
	}
	ntohlRange(commandbuffer, 3);
	command *com = malloc(sizeof(command));
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
		ESP_LOGE(commandTag,
			 "failed to alloc memory for command argument");
		free(com);
		return NULL;
	};
	int readBytes = 0;
	do {
		err = read(soc, argbuffer,
			   (sizeof(int32_t) * 3 * com->argnum) - readBytes);
		if(err < 0) {
			ESP_LOGE(commandTag, "arg read failed");
			free(argbuffer);
			free(com);
			return NULL;
		};
		readBytes += err;
		ESP_LOGI(commandTag,
			 "recived %d bythes", readBytes);
	} while(readBytes >= sizeof(int32_t) * 3 * com->argnum);
	for(int i = 0; i < com->argnum; i++) {
	}
	return com;
};

void doCommand(command *comm) {
	ESP_LOGI(commandTag, "doing command");
	switch(comm->command) {
	case INVALID:
		return;
		break;
	case PLAY:
		ESP_LOGI(commandTag, "play command");
		node *arg = comm->args;
		dac_cosine_handle_t handler = startFreq(arg->freq);
		vTaskDelay(arg->time / portTICK_PERIOD_MS);
		stopFreq(handler);
		break;
	default:
		return;
		break;
	};
};

void freeCommand(command *command) {
	if(command->command == PLAY) {
		free(command->args);
	}
	free(command);
};
