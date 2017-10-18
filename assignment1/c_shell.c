#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_ARGS 20
#define MAX_HIST 10
#define STD_OUT dup(2)
#define loopProcess(item, prev, head) \
				for(item = head; item != NULL; prev = item, item = item->next)


struct process *headJob = NULL;
struct process *currentJob = NULL;


struct process {
	int number;						// The job number
	int pid;							// The process id of the specific process
	char *command;				// The command used to create the process
	struct process *next;	// When another process is called you add to the end of the linked list
};


// Reset memory
void resetMemory() {
	struct process *current = headJob;
	while (current != NULL) {
		struct process *job = current;
		current = current->next;
		free(job);
	}
}


// Add a job to the list of jobs
void addToJobList(char *args[], int processPID, char *line[]) {
	struct process *job = malloc(sizeof(struct process));

	// If job list is empty, create a new head
	if (headJob == NULL) {
		job->number = 1;
		job->pid = processPID;
		job->command = line[0];
		job->next = NULL;

		headJob = currentJob = job;
	}
	// Else create new job process and link the current process to it
	else {
		job->number = currentJob->number + 1;
		job->pid = processPID;
		job->command = line[0];
		job->next = NULL;

		currentJob->next = job;
		currentJob = job;
	}
}


// Read the command line
int getcmd(char *prompt, char *args[], int *background, char *line[]) {
	int length, i = 0;
	size_t lineCap = 0;
	char *token, *loc = NULL;

	printf("%s", prompt);
	length = getline(&line[0], &lineCap, stdin);

	if (length <= 0) {
		resetMemory();
		exit(-1);
	}

	// Check if background is specified
	if ((loc = index(line[0], '&')) != NULL) {
		*background = 1;
		*loc = '\0';
	} else {
		*background = 0;
	}

	// Create copy of command line
	char *lineCopy = malloc(strlen(line[0]) + 1);
	if (lineCopy)
		strcpy(lineCopy, line[0]);
	else
		fprintf(stderr, "Memory allocation failure!");

	// Remove non characters and store in args
	while ((token = strsep(&lineCopy, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}

	free(lineCopy);
	return i;
}


// Kill child process with ctrl+c
void ctrlCHandler() {
	signal(SIGINT, ctrlCHandler);
	printf("Killed process.\n");
	fflush(stdout);
	exit(-1);
}


// Clear args memory
int resetArgs(char *args[]) {
	for (int i = 0; i < MAX_ARGS; i++)
		args[i] = NULL;
}


// Replace ~ with the home directory
int replaceTilde(char *args[]) {
	char *home = getenv("HOME");

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
				return -1;
			}
		}
	}
	return 1;
}


// Redirect stdout to a file
void redirectStdout(char *args[]) {
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
}


// Reset stdout
void resetStdout() {
	fflush(stdout);
	close(1);
	dup(STD_OUT);
}


// List all jobs in background
int executeJobs() {
	struct process *prev;
	struct process *current = headJob;

	if (current != NULL)
		printf("\n%-10s%-10s%-15s%-25s", "Job", "PID", "Status", "Command");

	while (current != NULL) {
		if (waitpid(current->pid, NULL, WNOHANG) == 0) {
			printf("\n%-10d%-10d%-15s%-25s", current->number, current->pid, "Running", current->command);
			prev = current;
			current = current->next;
		} else {
			printf("\n%-10d%-10d%-15s%-25s", current->number, current->pid, "Done", current->command);
			if (current == headJob) {
				headJob = current->next;
				free(current);
				current = headJob;
			} else {
				prev->next = current->next;
				free(current);
				current = prev->next;
			}
		}
	}
}

// Bring a background process to foreground
void executeFg(char *args[]) {
	int jobNum = atoi(args[1]);

	struct process *prev = NULL;
	struct process *current = NULL;

	loopProcess(current, prev, headJob) {
		if (current->number == jobNum) {
			kill(current->pid, SIGCONT);
			waitpid(current->pid, NULL, WUNTRACED);
		}
	}
}


// Main function
int main(void) {
	char *line[MAX_ARGS/5];;
	char *args[MAX_ARGS];
	int bg, count, pid;

	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	while(1) {
		bg = 0;
		resetStdout();
		resetArgs(args);
		resetArgs(line);
		count = getcmd("\n>> ", args, &bg, line);

		// Skip if command line was empty
		if (count <= 0)
			continue;

		// Change stdout if redirect/append operator found
		redirectStdout(args);

		// Exit C shell
		if (strcmp(args[0], "exit") == 0) {
			resetMemory();
			exit(1);
		}

		// Replace ~ and restart loop if failed
		if (replaceTilde(args) == -1)
			continue;

		// Change directory and restart loop
		if (strcmp(args[0], "cd") == 0) {
			chdir(args[1]);
			continue;
		}

		if (strcmp(args[0], "jobs") == 0) {
			executeJobs();
			resetStdout();
			continue;
		}

		if (strcmp(args[0], "fg") == 0) {
			if (args[1] != NULL) {
				executeFg(args);
				continue;
			}
			else
				printf("Add the job number you want to foreground.\n");
		}

		// Main execuction statement
		if ((pid = fork()) > 0) { // Parent
			if (bg == 0) {
				waitpid(pid, NULL, WUNTRACED);
			}
			else {
				addToJobList(args, pid, line);
			}
		} else if (pid == 0) { // Child
			signal(SIGINT, ctrlCHandler);

			// Execute the commands using syscall
			execvp(args[0], args);
		} else {
			perror("Fork failed.\n");
			resetMemory();;
			exit(-1);
		}

	}
}
