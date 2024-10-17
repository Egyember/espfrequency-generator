#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "lwip/sockets.h"
#include "gpioout.h"

static char *commandTag = "COMMANDS";

command *readCommand(int soc){
	command *ret = malloc(sizeof( command));
	int err = read(soc, &(ret->command), sizeof(int));
	if (err <0) {
		free(ret);
		return NULL;
	}
	ret->command = ntohl(ret->command);
	switch (ret->command) {
		case INVALID:
			ESP_LOGE(commandTag, "invalid command");
			free(ret);
			return NULL;
			break;
		case PLAY:
			 ret->args = (node *) malloc(sizeof( node));
			 int buff[2];
			 int err = read(soc, &(buff), sizeof(int)*2);
			 if (err <0) {
				 free(ret->args);
				 free(ret);
				 return NULL;
			 }
			node *dest = ret->args; 
			dest->freq = ntohl(buff[0]);
			dest->time = ntohl(buff[1]);
			break;
		case PLAYMULTI:
			ESP_LOGW(commandTag, "not inplemented command");
			free(ret);
			return NULL;
			break;
		default:
			 ESP_LOGE(commandTag, "invalid command");

	};
	return ret;
};

void doCommand(command *comm){
	ESP_LOGI(commandTag, "doing command");
	switch (comm->command) {
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
