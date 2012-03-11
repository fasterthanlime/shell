#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#define FD_READ 0
#define FD_WRITE 1

static int error;
static int inout[2];
static int toclose[2];
static int flags;

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

#define DEBUG 1

// string handling in C suxxorz

#define dbg(fmt, ...) \
    do { if(DEBUG) { \
        char __s1[2059]; \
        char __s2[2049]; \
        snprintf(__s1, 10, "[%d] ", getpid()); \
        snprintf(__s2, 2048, fmt, __VA_ARGS__); \
        strncat(__s1, __s2, 2048); \
        fprintf(stderr, "%s", __s1); \
    }} while(0)

int
builtin_cd(int argc, char **argv)
{
        if (argc < 2) {
            warn("Usage: cd <directory>");
            return (1);
        }
        int code = chdir(argv[1]);
        if (code != 0) {
            fprintf(stderr, "cd: %s\n", strerror(errno));
        }
	return code;
}

int
builtin_exit(void)
{
        exit(0);
}

int
builtin_status(void)
{
        printf("%d\n", error);
	return (0);
}

static struct builtin builtins[] = {
	BIN(cd),
	BIN(exit),
	BIN(status),
	{ NULL, NULL }
};

/*
 * run_builtin handles setting up the handlers for your builtin commands
 *
 * It accepts the array of strings, and tries to match the first argument
 * with the builtin mappings defined in the builtins[].
 * Returns 0 if it did not manage to find builtin command, or 1 if args
 * contained a builtin command
 */
static int
run_builtin(char **args)
{
	int argc;
	struct builtin *b;

	for (b = builtins; b->name != NULL; b++) {
		if (strcmp(b->name, args[0]) == 0) {
			for (argc = 0; args[argc] != NULL; argc++)
				/* NOTHING */;
			error = b->func(argc, args);
			return (1);
		}
	}

	return (0);
}

/*
 * run_command handles 
 */
static int
run_command(char **args)
{
        // DEBUG start
        char **nargs = args;
        int argc = 0;
        while (*nargs) {
            argc++;
            nargs++;
        }
        // DEBUG end

        if (flags & P_AND) {
            // only execute if previous command was successful
            if (error != 0) {
                flags = 0;
                return (0);
            }
        } else if (flags & P_OR) {
            // only execute if previous command was unsuccessful
            if (error == 0) {
                flags = 0;
                return (0);
            }
        }

        // try built-in first
        if (run_builtin(args) == 1) {
            return (1);
        }

        // execute external command
        pid_t child_pid;
        if ((child_pid = fork()) == 0) {
            /* Child process code */    
            dbg("r%d ==> $(%s) ==> w%d\n", inout[0], args[0], inout[1]);

            // close ends of the pipe we don't use
            for (int i = 0; i < 2; i++) if (toclose[i] != -1) {
                dbg("closing %c%d\n", i == 0 ? 'r' : 'w', toclose[i]);
                close(toclose[i]);
                toclose[i] = -1;
            }
            
            if (inout[0] != STDIN_FILENO) {
                if (dup2(inout[0], 0) == -1) {
                    dbg("error while doing stdin = %d, %s\n", inout[0], strerror(errno));
                }
                close(inout[0]);
            }

            if (inout[1] != STDOUT_FILENO) {
                if (dup2(inout[1], 1) == -1) {
                    dbg("error while doing stdout = %d, %s\n", inout[1], strerror(errno));
                }
                close(inout[1]);
            }

            // fprintf(stderr, "Launching '%s'\n", args[0]);
            // dbg("%s", "exec\n");
            if (execvp(args[0], args) == -1) {
                dbg("Launching %s failed with error: %s", args[0], strerror(errno));
            }
        } else if (child_pid > 0) {
            /* Parent process code */

            // close the child's input and output
            if (inout[0] != STDIN_FILENO) {
                dbg("closing r%d\n", inout[0]);
                close(inout[0]);
            }

            if (inout[1] != STDOUT_FILENO) {
                dbg("closing w%d\n", inout[1]);
                close(inout[1]);
            }

            if (!(flags & P_BG)) {
                dbg("waiting for [%d]\n", child_pid);
               
                if(waitpid(child_pid, &error, 0) == -1) {
                    dbg("Error while waiting for [%d]: %s\n", child_pid, strerror(errno));
                }
            }
        } else {
            dbg("%s", "Failed to fork! Cannot launch command.\n");
        }

        flags = 0;

        return (1);
}

/*
 * Takes a pointer to a string pointer and advances this string pointer
 * to the word delimiter after the next word.
 * Returns a pointer to the beginning of the next word in the string,
 * or NULL if there is no more word available.
 */
static char *
parseword(char **pp)
{
	char *p = *pp;
	char *word;

	for (; isspace(*p); p++)
		/* NOTHING */;

	word = p;

        // Japanese schoolgirl is NOT AMUSED.
	// for (; fprintf(stderr, "Testing char %c aka %d\n", *p, *p), strchr("\t&> <;|\n", *p) == NULL; p++)
	for (; strchr("\t&> <;|\n", *p) == NULL; p++)
		/* NOTHING */;

	*pp = p;

	return (p != word ? word : NULL);
}

static void
process(char *line)
{
	int ch, ch2;
	char *p, *word;
	char *args[100], **narg;
	int pip[2];

	p = line;
        if (*p == '#') {
            // comment line, don't execute
            return;
        }

        pip[0] = 0;
        pip[1] = 0;

newcmd:
	inout[0] = STDIN_FILENO;
	inout[1] = STDOUT_FILENO;
        toclose[0] = -1;
        toclose[1] = -1;
        flags = 0;

newcmd2:
	narg = args;
	*narg = NULL;

	for (; *p != 0; p++) {
		word = parseword(&p);

		ch = *p;
		*p = 0;

		// printf("parseword: '%s', '%c', '%s'\n", word, ch, p + 1);

		if (word != NULL) {
			*narg++ = word;
			*narg = NULL;
		}

nextch:
		switch (ch) {
		case ' ':
		case '\t': p++; ch = *p; goto nextch;
		case '<': {
                        p++;
                        char *path = parseword(&p);
                        *p = 0;

                        inout[0] = open(path, O_RDONLY);

                        p++; ch = *p;
                        goto nextch;
                }
		case '>': {
                        p++;
                        char* path = parseword(&p);
                        *p = 0;

                        inout[1] = open(path, O_CREAT | O_WRONLY, 0644);

                        p++; ch = *p;
                        goto nextch;
                }
                case '|': {
                        p++; 
                        ch2 = *p;
                        if (ch2 == '|') {
                            p++;
                            run_command(args);
                            flags |= P_OR;
                            goto newcmd;
                        } else {
                            if (pip[FD_READ]) {
                                // (pipe n-1) -> (process)
                                inout[FD_READ] = pip[FD_READ];
                                toclose[FD_WRITE] = pip[FD_WRITE];
                            }

                            if(pipe(pip) == -1) {
                                dbg("%s", "Couldn't create a pipe");
                                exit(1);
                            }
                            dbg("==> w%d | r%d ==>\n", pip[1], pip[0]);

                            // (process) -> (pipe n)
                            inout[FD_WRITE] = pip[FD_WRITE];
                            toclose[FD_READ] = pip[FD_READ];
                            flags |= P_BG;

                            run_command(args);
                            
                            // reset fds, they'll be set accordingly
                            toclose[FD_READ] = -1;
                            toclose[FD_WRITE] = -1;
                            inout[FD_READ] = STDIN_FILENO;
                            inout[FD_WRITE] = STDOUT_FILENO;

                            goto newcmd2;
                        }
                        break;
                }
                case '&': {
                        p++;
                        ch2 = *p;
                        if (ch2 == '&') {
                            p++;
                            run_command(args);
                            flags |= P_AND;
                            goto newcmd;
                        } else {
                            flags |= P_BG;
                            run_command(args);
                            goto newcmd;
                        }
                        break;
                }
		case ';':
                        p++;
                        if (pip[1]) {
                            inout[FD_READ] = pip[FD_READ];
                            toclose[FD_WRITE] = pip[FD_WRITE];
                        }
                        run_command(args);
                        goto newcmd;
		case '\n':
		case '\0':
                        if (pip[1]) {
                            inout[FD_READ] = pip[FD_READ];
                            toclose[FD_WRITE] = pip[FD_WRITE];
                        }
                        if (args[0]) {
                            run_command(args);
                        }
                        return;
		default:
                        p--; // will be handled by next iteration
                        break;
		}
	}
}

static void
do_nothing()
{
        ; 
}

static void
reap_zombie_jesus()
{
        int cadaver;

        // dbg("%s", "Reaping zombie children!\n");
        // reap all zombie children
        while((cadaver = waitpid(-1, NULL, WNOHANG)) > 0) {
            /* magic! */
            dbg("%d has terminated\n", cadaver);
        }
        // dbg("%s", "Done!\n");
}

int
main(void)
{
	char cwd[MAXPATHLEN+1];
	char line[1000];
	char *res;
       
        // ignore interruptions: the shell shall survive!
        signal(SIGINT, do_nothing);

        // elect the kernel to automatically reap zombie children
        signal(SIGCHLD, (__sighandler_t) reap_zombie_jesus);
	
        for (;;) {
		getcwd(cwd, sizeof(cwd));
		printf("%s %% ", cwd);

		res = fgets(line, sizeof(line), stdin);
		if (res == NULL)
			break;

		process(line);
	}

	return (error);
}
