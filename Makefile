CFLAGS := ${CFLAGS} -g -Wall -Wextra -pedantic -std=gnu99

all: shell

clean:
	rm -f shell shell.o *.core core *~
