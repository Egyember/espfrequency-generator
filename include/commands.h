#ifndef COMMANDS
#define COMMANDS

enum commandType {
	INVALID = 0,
	HELP = 1,
	PLAY = 2,
	PLAYMULTI = 3,
	QUIT = 4,
};
typedef struct command {
	enum commandType command;
	void *args;
} command;
typedef struct node {
	int freq;
	int time; // in ms
} node;
typedef struct sheet {
	int N;
	node *data;
} sheet;
command *readCommand();
#endif
