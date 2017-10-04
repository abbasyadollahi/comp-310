#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>

#define STD_OUT dup(1)
#define MAX_ARGS 20
#define MAX_HIST 10

// This code is given for illustration purposes. You need not include or follow this
// strictly. Feel free to writer better or bug free code. This example code block does not
// worry about deallocating memory. You need to ensure memory is allocated and deallocated
// properly so that your shell works without leaking memory.

int getcmd(char *prompt, char *args[], int *background)
{
	// Clear args in memory
	for (int i = 0; i < MAX_ARGS; i++)
		args[i] = NULL;

	int length, i = 0;
	size_t linecap = 0;
	char *token, *loc, *line = NULL;


	printf("%s", prompt);
	length = getline(&line, &linecap, stdin);  // Num of characters in line

	if (length <= 0)
		exit(-1);

	// Check if background is specified..
	if ((loc = index(line, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
	} else {
		*background = 0;
	}


	while ((token = strsep(&line, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}

	for (int sss = 0; args[sss]; sss++)
		printf("1. %s\n", args[sss]);

	return i;
}


int main(void) {
	char *redirectTo;
	char *args[MAX_ARGS];
	int bg, count, pid, fg, jobs, fileRdt;

	while(1) {
		bg = fg = jobs = 0;
		count = getcmd("\n>> ", args, &bg);

		// Skip if command line was empty
		if (count <= 0)
			continue;

		if (strcmp(args[0], "cd") == 0) {
			// If args[1] == ~, find home dir
			chdir(args[1]);
			continue;
		} else if (strcmp(args[0], "exit") == 0) {
			exit(-1);
		}


		if (pid = fork()) {
			waitpid(0, NULL, 0);
		} else {
			for (int idx = 0; args[idx]; idx++) {
				if (strcmp(args[idx], ">") == 0) {
					redirectTo = args[idx+1];
					args[idx] = args[idx+1] = NULL;

					close(1);
					fileRdt = open(redirectTo, O_WRONLY | O_CREAT, 0755);
				}
			}

			if (strcmp(args[0], "fg") == 0)
				fg = 1;
			else if (strcmp(args[0], "jobs") == 0)
				jobs = 1;
			else {
				execvp(args[0], args);

				// Reset stdout after redirection was used
				if (redirectTo) {
					close(fileRdt);
					dup2(STD_OUT, 1);
					close(STD_OUT);
					redirectTo = NULL;
				}
			}
		}

		/* the steps can be..:
		(1) fork a child process using fork()
		(2) the child process will invoke execvp()
		(3) if background is not specified, the parent will wait,
		otherwise parent starts the next command... */
	}
}