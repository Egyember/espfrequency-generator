#include "commands.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gpioout.h"
#include "sys/ioctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lwip/sockets.h"
/*
#define LWIP_PROVIDE_ERRNO
#undef LWIP_HDR_ERRNO_H
*/
#include "lwip/errno.h"


static char *commandTag = "COMMANDS";

void IRAM_ATTR ntohlRange(int32_t *buff, uint len) { //I call it a lot so put it in sram
	for(int i = 0; i < len; i++) {
		buff[i] = ntohl(buff[i]);
	};
};

command *readCommand(int soc) {
	int avalable;
	int err = lwip_ioctl(soc, FIONREAD, &avalable); // fuck this shit https://github.com/espressif/esp-idf/issues/6215
	if(err < 0) {
		ESP_LOGE(commandTag, "ioctl failed errno: %s", strerror(errno));
		return NULL;
	};
	printf("avalable: %d\n", avalable);
	if(avalable < (3 * sizeof(int))) {
		ESP_LOGE(commandTag, "not enugh data");
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
	if(com == NULL) { // malloc failed
		ESP_LOGE(commandTag,
			 "failed to alloc memory for command");
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
	ESP_LOGI( commandTag, "size: %lu\n", sizeof(int32_t) * 3 * com->argnum);
	int32_t *argbuffer = malloc(sizeof(int32_t) * 3 * com->argnum);
	if(argbuffer == NULL) { // malloc failed
		ESP_LOGE(commandTag,
			 "failed to alloc memory for command argument");
		free(com);
		return NULL;
	};
	int readBytes = 0;
	do {
		err = read(soc, argbuffer+readBytes,
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
	} while(false);
			//readBytes >= sizeof(int32_t) * 3 * com->argnum);
	ntohlRange(argbuffer, 3 * com->argnum);
	void *args = NULL;
	switch (com->command) {
		case PLAY:
			args = malloc(sizeof(node)* com->argnum);
			node *nargs = args;
			for(int i = 0; i < com->argnum; i++) {
				if (argbuffer[i*3] == DATA) {
					nargs[i].freq = argbuffer[1+ i*3];
					nargs[i].time = argbuffer[2+ i*3];
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
		printf("comand: %d, freq: %ld time: %ld\n", comm->command, arg->freq, arg->time);
		dac_cosine_handle_t handler = startFreq(arg->freq);
		vTaskDelay(arg->time / portTICK_PERIOD_MS);
		stopFreq(handler);
		break;
	default:
		ESP_LOGI(commandTag, "not implemented command");
		break;
	};
	return;
};

void freeCommand(command *command) {
	if (command == NULL) {
		ESP_LOGW(commandTag, "NULL command free atempt");
		return;
	}
	if(command->command == PLAY) {
		free(command->args);
	}
	free(command);
};
