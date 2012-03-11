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
#include "pipstack.h"
#include "shell.h"
#include "debug.h"

static int error;

// -----------------------
// Built-ins
// -----------------------

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
run_command(command_t *cm, char **args)
{
        // try built-in first
        if (run_builtin(args) == 1) {
            return (1);
        }

        command_copy_args(cm, args);

        if (cm->flags & P_AND) {
            // only execute if previous command was successful
            if (error != 0) {
                return (0);
            }
        } else if (cm->flags & P_OR) {
            // only execute if previous command was unsuccessful
            if (error == 0) {
                return (0);
            }
        }

        // execute external command
        pid_t child_pid;
        if ((child_pid = fork()) == 0) {
            /* Child process code */    
            command_setup_endpoints(cm);

            if (execvp(args[0], args) == -1) {
                dbg("Launching %s failed with error: %s\n", args[0], strerror(errno));
            }
        } else if (child_pid > 0) {
            // queue fds to close later
            command_close_endpoints(cm);

            /* Parent process code */
            if (!(cm->flags & P_BG)) {
                dbg("waiting for [%d]\n", child_pid);
               
                if(waitpid(child_pid, &error, 0) == -1) {
                    dbg("Error while waiting for [%d]: %s\n", child_pid, strerror(errno));
                }
            }
        } else {
            dbg("%s", "Failed to fork! Cannot launch command.\n");
        }

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
        // character look-aheads
	int ch, ch2;
        int next_cm_flags = 0;
	char *p, *word;
	char *args[100], **narg;

        command_t *cm = NULL, *prev_cm = NULL;

	p = line;

newcmd:
        // destroy previous command if any
        if (prev_cm) {
            command_destroy(prev_cm);
        }
        prev_cm = cm;

        cm = command_new();
        // connect pipes if needed
        if (prev_cm && prev_cm->output->type == EP_PIPE) {
            cm->input->type = EP_PIPE;
            cm->input->pipe = prev_cm->output->pipe;
        }
        cm->flags = next_cm_flags;
        next_cm_flags = 0;

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
                case '#':
                        return; // skip comments
		case ' ':
		case '\t': p++; ch = *p; goto nextch;
		case '<': {
                        p++;
                        char *path = parseword(&p);
                        *p = 0;

                        cm->input->type = EP_FILE;
                        cm->input->path = strdup(path);

                        p++; ch = *p;
                        goto nextch;
                }
		case '>': {
                        p++;
                        char* path = parseword(&p);
                        *p = 0;

                        cm->output->type = EP_FILE;
                        cm->output->path = strdup(path);

                        p++; ch = *p;
                        goto nextch;
                }
                case '|': {
                        p++; 
                        ch2 = *p;
                        if (ch2 == '|') {
                            // ||
                            p++;

                            run_command(cm, args);

                            next_cm_flags |= P_OR;
                            goto newcmd;
                        } else {
                            // |
                            cm->output->type = EP_PIPE;
                            cm->output->pipe = pipe_new();
                            cm->flags |= P_BG;

                            run_command(cm, args);
                            goto newcmd;
                        }
                        break;
                }
                case '&': {
                        p++;
                        ch2 = *p;
                        if (ch2 == '&') {
                            // &&
                            p++;
                            run_command(cm, args);

                            next_cm_flags |= P_AND;
                            goto newcmd;
                        } else {
                            // &
                            cm->flags |= P_BG;
                            run_command(cm, args);

                            goto newcmd;
                        }
                        break;
                }
		case ';':
                        p++;
                        run_command(cm, args);
                        goto newcmd;
		case '\n':
		case '\0':
                        if (args[0]) {
                            run_command(cm, args);
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
