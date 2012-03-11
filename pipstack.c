#include <stdio.h>
#include <stdlib.h>
#include "pipstack.h"
#include "debug.h"
#include <err.h>

struct pipe_stack {
    int count;
    int size;
    int* list;
};

int pip_is_empty(pipe_stack* pipes) {
    return pip_get_size(pipes) <= 0;
}

void pip_close_all(pipe_stack* pipes) {
    while (!pip_is_empty(pipes)) {
        int fd = pip_pop(pipes);
        dbg("closing %d\n", fd);
        close(fd);
    }
}

void pip_push(pipe_stack* pipes, int fd) {
    if(!pipes) {
        warn("called pip_push on null stack!");
        exit(1);
    }

    if (pipes->count + 1 >= pipes->size) {
        warn("overflow: trying to push pipes on a full stack");
        exit(1);
    }
    pipes->list[pipes->count++] = fd;
}

int pip_get_size(pipe_stack* pipes) {
    if(!pipes) {
        warn("called pip_get_size on null stack!");
        exit(1);
    }
    return pipes->count;        
}

int pip_pop(pipe_stack* pipes) {
    if(!pipes) {
        warn("called pip_pop on null stack!");
        exit(1); 
    }
    if(pipes->count == 0) {
        return -1;
    }
    return pipes->list[--pipes->count];
}

pipe_stack* pip_new() {
    pipe_stack* pipes = malloc(sizeof(pipe_stack));
    *pipes = (pipe_stack) {
        0,
        1024,
        malloc(1024 * sizeof(int))
    };
    return pipes;
}

void pip_destroy(pipe_stack* pipes) {
    if(!pipes) {
        warn("called pip_destroy on null stack!");
        exit(1); 
    }
    free(pipes->list);
    free(pipes);
}

