#ifndef COMMANDS
#define COMMANDS
#include <stdint.h>

enum packettype {
	COMMAND =0,
	DATA = 1,
};

enum commandType {
	INVALID = 0,
	HELP = 1,
	PLAY = 2,
	PLAYMULTI = 3,
	QUIT = 4,
};
typedef struct command {
	enum commandType command;
	int32_t argnum;
	void *args; //pointer to array of args with argnum length
} command;
typedef struct node {
	int32_t freq;
	int32_t time; // in ms
} node;
typedef struct sheet {
	int32_t N;
	node *data;
} sheet;
command *readCommand(int soc);
void doCommand(command *comm);
void freeCommand(command *comm);
#endif
