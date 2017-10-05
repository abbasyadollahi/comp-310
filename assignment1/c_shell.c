#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
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

int getcmd(char *prompt, char *args[], int *background) {
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

void sigStpHandler(int sig_num) {
  signal(SIGTSTP, sigStpHandler);
  printf("Cannot be terminated using Ctrl+Z.\n");
  fflush(stdout);
  continue;
}

int main(void) {
	char *args[MAX_ARGS];
	int bg, count, pid, fg, jobs, fileRdt;
	char *home = getenv("HOME");

	while(1) {
		signal(SIGTSTP, sigStpHandler);
		bg = fg = jobs = 0;
		count = getcmd("\n>> ", args, &bg);

		// Skip if command line was empty
		if (count <= 0)
			continue;

		// Kernel can't interpret ~
		for (int idx = 0; args[idx]; idx++) {
			if (strchr(args[idx], 126)) {
				char* replaced;
				char* copy = args[idx];
				int len = strlen(copy);
				memmove(&copy[0], &copy[1], len);

				if ((replaced = malloc(len + strlen(home) + 1)) != NULL) {
					replaced[0] = '\0';
					strcat(strcat(replaced, home), copy);
					args[idx] = replaced;
				} else {
					fprintf(stderr, "Memory allocation failed for ~.\n");
					continue;
				}
			}
		}

		if (strcmp(args[0], "cd") == 0) {
			chdir(args[1]);
			continue;
		} else if (strcmp(args[0], "exit") == 0) {
			exit(-1);
		}


		if (pid = fork()) {
			waitpid(0, NULL, 0);
		} else {
			// If redirect/append operator found
			for (int idx = 0; args[idx]; idx++) {
				if (strcmp(args[idx], ">") == 0 || strcmp(args[idx], ">>") == 0) {
					int flag = O_TRUNC;
					char *redirectTo = args[idx+1];
					close(1);

					if (strcmp(args[idx], ">>") == 0)
						flag = O_APPEND;

					open(redirectTo, O_WRONLY | O_CREAT | flag, 0755);
					args[idx] = args[idx+1] = NULL;
				}
			}

			if (strcmp(args[0], "fg") == 0)
				fg = 1;
			else if (strcmp(args[0], "jobs") == 0)
				jobs = 1;
			else {
				execvp(args[0], args);

				// Reset stdout after redirection was used
				// if (redirectTo) {
				// 	close(fileRdt);
				// 	dup2(STD_OUT, 1);
				// 	close(STD_OUT);
				// 	redirectTo = NULL;
				// }
			}
		}

		/* the steps can be..:
		(1) fork a child process using fork()
		(2) the child process will invoke execvp()
		(3) if background is not specified, the parent will wait,
		otherwise parent starts the next command... */
	}
}
