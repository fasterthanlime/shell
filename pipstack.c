#include <stdio.h>
#include <stdlib.h>
#include "pipstack.h"

typedef struct {
    int count;
    int size;
    int* list;
} pipe_stack;

void pip_push(pipe_stack* pipes, int fd) {
    if(!pipes) {
        warn("called pip_push on null stack!");
        exit(1);
    }

    if (pipes->count + 1 >= pipes->size) {
        warn("overflow: trying to push pipes on a full stack");
        exit(1);
    }
    pipes->list[pipes->count++];
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

