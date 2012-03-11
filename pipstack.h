#ifndef __PIP_STACK__
#define __PIP_STACK__

typedef struct pipe_stack pipe_stack;

int pip_get_size(pipe_stack* pipes);
void pip_push(pipe_stack* pipes, int fd);
int pip_pop(pipe_stack* pipes);
pipe_stack* pip_new();
void pip_destroy(pipe_stack* pipes);
void pip_close_all(pipe_stack* pipes);
int pip_is_empty(pipe_stack* pipes);

#endif // __PIP_STACK__
