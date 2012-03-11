#ifndef __SHELL_H__
#define __SHELL_H__

#include "debug.h"

typedef struct {
    int fd_read;
    int fd_write;
} pipe_t;

typedef struct { 
    int type; // one of ENDPOINT_TYPES
    int direction;

    // EP_FILE
    char *path;
    int fd;

    // EP_PIPE
    pipe_t pipe;
} endpoint_t;

enum ENDPOINT_DIRECTIONS {
    EP_READ = 0,
    EP_WRITE = 1,
};

enum ENDPOINT_TYPES {
    EP_PIPE,
    EP_FILE,
    EP_TTY,
};

typedef struct {
    int argc;
    char **argv;
    int flags;
    endpoint_t *input;
    endpoint_t *output;
} command_t;

enum PROCESS_FLAGS {
    P_BG =  (1 << 0),
    P_OR =  (1 << 1),
    P_AND = (1 << 2),
};

void endpoint_sanity_check(endpoint_t* ep) {
    if (ep->type != EP_TTY && ep->type != EP_PIPE &&
            ep->type != EP_FILE) {
        dbg("invalid value for ep->type: %d\n", ep->type);
        exit(1);
    }

    if (ep->direction != EP_READ && ep->direction != EP_WRITE) {
        dbg("invalid value for ep->direction: %d\n", ep->direction);
        exit(1);
    }

    if (ep->type == EP_FILE && !ep->path) {
        dbg("%s", "null path for ep of type EP_FILE\n");
        exit(1);
    }
}

endpoint_t *endpoint_new(int direction) {
    endpoint_t *ep = calloc(1, sizeof(endpoint_t));
    ep->direction = direction;
    ep->type = EP_TTY;
    return ep;
}

void endpoint_destroy(endpoint_t *ep) {
    if(ep->type == EP_FILE) {
        free(ep->path); // it's a dup
    }
}

void endpoint_close(endpoint_t* ep) {
    if (ep->type == EP_PIPE) {
        close(ep->pipe.fd_read);
        close(ep->pipe.fd_write);
    }
}

void endpoint_setup(endpoint_t* ep) {
    endpoint_sanity_check(ep);

    switch (ep->type) {
        case EP_TTY:
            // nothing to do
            break;
        case EP_FILE: {
            if (ep->direction == EP_READ) {
                ep->fd = open(ep->path, O_RDONLY);
            } else {
                ep->fd = open(ep->path, O_CREAT | O_WRONLY, 0644);
            }
            if(dup2(ep->fd, ep->direction) == -1) {
                dbg("error while setting endpoint %d to fd %d\n", ep->direction, ep->fd);
                exit(1);
            }
            close(ep->fd);
            break;
        }
        case EP_PIPE: {
            if (ep->direction == EP_READ) {
                ep->fd = ep->pipe.fd_read;
                close(ep->pipe.fd_write);
            } else {
                ep->fd = ep->pipe.fd_write;
                close(ep->pipe.fd_read);
            }
        }
    }
}

void command_sanity_check(command_t* cm) {
    if(cm->argc < 1) {
        dbg("Invalid number of arguments for a command: %d\n", cm->argc);
        exit(1);
    } 
    if(!cm->argv) {
        dbg("%s", "Null argument list for a command.");
        exit(1);
    }
    if(!cm->input || !cm->output) {
        dbg("%s", "Null input or output for a command.");
        exit(1);
    }
    endpoint_sanity_check(cm->input);
    endpoint_sanity_check(cm->output);
}

command_t* command_new(void) {
    command_t *cm = calloc(1, sizeof(command_t));
    // TTY by default
    cm->input  = endpoint_new(EP_READ);
    cm->output = endpoint_new(EP_WRITE);
    return cm;
}

void command_close_endpoints(command_t *cm) {
    endpoint_close(cm->input);
    endpoint_close(cm->output);
}

void command_copy_args(command_t *cm, char **args) {
    char **nargs = args;

    cm->argc = 0;
    while (*nargs) {
        cm->argc++;
        nargs++;
    }

    cm->argv = calloc(cm->argc, sizeof(void*));
    nargs = args;
    for (int i = 0; i < cm->argc; i++) {
        cm->argv[i] = strdup(args[i]);
    }
}

void command_destroy(command_t *cm) {
    endpoint_destroy(cm->input);
    endpoint_destroy(cm->output);

    for (int i = 0; i < cm->argc; i++) {
        free(cm->argv[i]);
    }
}

void command_setup_endpoints(command_t* cm) {
    endpoint_setup(cm->input);
    endpoint_setup(cm->output);
}

pipe_t pipe_new() {
    int pip[2];
    if(pipe(pip) == -1) {
        dbg("%s", "could not create pipe!");
    }
    return (pipe_t) { pip[0], pip[1] };
}

typedef int (*builtin_cmd)(int, char **);

struct builtin {
	const char *name;
	builtin_cmd func;
};

#define	BIN(n)	{ #n, (int (*)()) builtin_ ## n }

#endif // __SHELL_H__
