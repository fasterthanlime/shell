CFLAGS := ${CFLAGS} -g -Wall -Wextra -pedantic -std=gnu99

all: shell 

shell: pipstack.o

clean:
	rm -f shell shell.o pipstack.o *.core core *~
