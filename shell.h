#ifndef __SHELL_H__
#define __SHELL_H__

#include "debug.h"

typedef struct endpoint { 
    int type; // one of ENDPOINT_TYPES
    int direction;

    // EP_FILE
    char *file;

    // EP_PIPE
    int fd_read; 
    int fd_write;
} endpoint;

enum ENDPOINT_DIRECTIONS {
    EP_READ,
    EP_WRITE,
};

enum ENDPOINT_TYPES {
    EP_PIPE,
    EP_FILE,
    EP_TTY,
};

typedef struct command {
    int argc;
    char **argv;
    struct endpoint input;
    struct endpoint output;
} command;


enum PROCESS_FLAGS {
    P_BG =  (1 << 0),
    P_OR =  (1 << 1),
    P_AND = (1 << 2),
};

typedef int (*builtin_cmd)(int, char **);

struct builtin {
	const char *name;
	builtin_cmd func;
};

#define	BIN(n)	{ #n, (int (*)()) builtin_ ## n }

#endif // __SHELL_H__
