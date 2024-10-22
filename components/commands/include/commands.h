#ifndef COMMANDS
#define COMMANDS

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
	unsigned int argnum;
	void *args; //pointer to array of args with argnum length
} command;
typedef struct node {
	int freq;
	int time; // in ms
} node;
typedef struct sheet {
	int N;
	node *data;
} sheet;
command *readCommand(int soc);
void doCommand(command *comm);
void freeCommand(command *comm);
#endif
