#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

static pid_t parentPid = 0;

static pid_t currentPid = 0;

struct job *head = NULL;
struct job *tail = NULL;

struct job {
    int num;
    pid_t pid;
    struct job *next;
};

void addJob(pid_t *pid) {
    struct job *job = malloc(sizeof(struct job));

    if (head == NULL) {
        job->num = 1;
        job->pid = *pid;
        job->next = NULL;
        head = job;
        tail = job;
    }
    else {
        job->num = tail->num + 1;
        job->pid = *pid;
        job->next = NULL;
        tail->next = job;
        tail = job;
    }
}

pid_t findJob(int num) {
    struct job *current = head;
    while (current != NULL) {
        if (current->num == num) {
            return current-> pid;
        }
        current = current->next;
    }
    return -1;
}

void freeMemory() {
    struct job *current = head;
    while (current != NULL) {
        struct job *temp = current;
        current = current->next;
        free(temp);
    }
}

static void signalHandler(int sig) {
    if (currentPid != parentPid) {
        kill(currentPid, SIGKILL);
    }
}

void initialize(char *args[]) {
    for (int i = 0; i < 20; i++) {
        args[i] = NULL;
    }
    return;
}

int getcmd(char *prompt, char *line, char *args[], int *background) {
    int length, i = 0;
    char *token, *loc;
    size_t linecap = 0;

    printf("%s", prompt);

    length = getline(&line, &linecap, stdin);
    if (length <= 0){
        return -1;
    }

    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    }
    else {
        *background = 0;
    }

    char *lineCopy = line;

    while ((token = strsep(&lineCopy, " \t\n"))!= NULL){
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) {
                token[j] = '\0';
            }
        }
        if (strlen(token)>0){
            args[i++] = token;
        }
    }

    args[i] = NULL;

    return i;
}

void executeFg(char *args[]) {
    pid_t pid = -1;
    int status;

    if (args[1] != NULL){
        pid = findJob(atoi(args[1]));
    }
    else if (tail != NULL) {
        pid = tail->pid;
    }
    if (pid  != -1){
        currentPid = pid;
    }
    if (pid == -1 || (waitpid(pid, &status, WUNTRACED) == -1)) {
        fprintf(stderr, "fg: No such job");
    }

}

void executeCd(char *args[]) {
    int result = 0;
    if (args[1] == NULL) {
        char *home = getenv("HOME");
        if (home != NULL) {
            result = chdir(home);
        }
        else {
            fprintf(stderr, "cd, No $HOME variable declared in this environment");
        }
    }
    else {
        result = chdir(args[1]);
    }
    if (result == -1){
        fprintf(stderr, "cd: %s: No such file or directory", args[1]);
    }
}

void executeJobs() {
    int status;
    struct job *current = head;
    struct job *pre;

    while (current != NULL) {
        if (waitpid(current->pid, &status, WNOHANG) == 0) {
            printf("\n[%d] Running pid: %d", current->num, current->pid);
            pre = current;
            current = current->next;
        }
        else {
            printf("\n[%d] Done pid: %d", current->num, current->pid);
            if (current == head) {
                head = current->next;
                free(current);
                current = head;
            }
            else {
                pre->next = current->next;
                free(current);
                current = pre->next;
            }
        }
    }
}

void executePwd(char *args[]) {
    char cwd[100];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "pwd: Errow calling getcwd().");
    }
    else {
        printf("\n%s", cwd);
    }
}

void handleRedirection(char *args[]){
    int i = 0;

    while (args[i] != NULL) {
        if (!strcmp(">", args[i])) {
            if (args[i+1] != NULL) {
                close(1);
                open(args[i+1], O_WRONLY | O_CREAT | S_IRWXU);
            }
            else {
                fprintf(stderr, "No output destination is foudn");
            }
            args[i] = NULL;
            return;
        }
        i++;
    }
}

void resetRedirection() {
    fflush(stdout);
    close(1);
    dup(2);
}

int main(void) {
    char *args[20];
    int bg;
    parentPid = getpid();

    if (signal(SIGINT, signalHandler) == SIG_ERR) {
        fprintf(stderr, "Could not bind signalHandler to SIGINT");
        freeMemory();
        exit(-1);
    }
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Could not bind SIG_IGN to SIGSTP");
        freeMemory();
        exit(-1);
    }

    while(1) {
        currentPid = getpid();
        initialize(args);
        bg = 0;
        char *line = NULL;

        int cnt = getcmd("\n>> ", line, args, &bg);
        if (cnt == -1) {
            fprintf(stderr, "fail to get command");
            continue;
        }
        if (args[0] == NULL) {
            continue;
        }

        handleRedirection(args);

        if (!strcmp("exit", args[0])) {
            freeMemory();
            exit(0);
        }
        else if (!strcmp("fg", args[0])) {
            executeFg(args);
        }
        else if (!strcmp("cd", args[0])) {
            executeCd(args);
        }
        else if (!strcmp("jobs", args[0])) {
            executeJobs();
        }
        else if (!strcmp("pwd", args[0])) {
            executePwd(args);
        }
        else {
            pid_t pid = fork();

            if (pid < 0) {
                fprintf(stderr, "An error occured during fork()");
                freeMemory();
                exit(-1);
            }
            if (pid == 0) {
                sleep(5);
                execvp(args[0], args);
            }
            else {
                if (bg == 1) {
                    addJob(&pid);
                }
                else {
                    currentPid = pid;
                    int status;
                    int wpid = waitpid(pid, &status, WUNTRACED);
                    if (wpid == -1) {
                        fprintf(stderr, "Error, calling waitpid(), child process status: %d", status);
                    }
                }
            }
        }
        free(line);
        resetRedirection();
    }
}
