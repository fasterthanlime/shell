CFLAGS := ${CFLAGS} -g -Wall -Wextra -pedantic -std=gnu99

all: shell 

shell: pipstack.o shell.o

clean:
	rm -f shell shell.o pipstack.o *.core core *~
