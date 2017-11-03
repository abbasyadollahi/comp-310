#define _BSD_SOURCE
#define _SVID_SOURCE
#define _XOPEN_SOURCE 500
#define _XOPEN_SORUCE 600
#define _XOPEN_SORUCE 600

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

#define MAX_ARGS 100
#define BUFF_SIZE 20
#define BUFF_SHM "abbas_a2_shm"
#define BUFF_MUTEX_A "/OS_MUTEX_A"
#define BUFF_MUTEX_B "/OS_MUTEX_B"

// Declaring semaphores names for local usage
sem_t *mutexA;
sem_t *mutexB;

// Declaring the shared memory and base address
int shm_fd;
void *base;

// Structure for individual table
struct table {
    int num;
    char name[10];
};

void initTables(struct table *base) {
    // Capture both mutexes using sem_wait

    // Initialise the tables with table numbers

    // Perform a random sleep
    sleep(rand() % 10);

    // Release the mutexes using sem_post
    return;
}

void printTableInfo(struct table *base) {
    // Capture both mutexes using sem_wait

    // Print the tables with table numbers and name

    // Perform a random sleep
    sleep(rand() % 10);

    // Release the mutexes using sem_post
    return;
}

void reserveSpecificTable(struct table *base, char *name_hld, char *section, int table_num) {
    switch (section[0]) {
        case 'a':
        case 'A':
            // Capture mutex for section A

            // Check if table number belongs to section specified
            if (table_num < 100 || table_num > 109) {
                printf("Reservation failed: Table %d does not exist for section A.\n", table_num);
                return;
            }


            // Reserve table for the name specified
            // If cant reserve (already reserved by someone) : print "Cannot reserve table"

            // Release mutex
            break;
        case 'b':
        case 'B':
            // Capture mutex for section B

            // Check if table number belongs to section specified
            if (table_num < 200 || table_num > 209) {
                printf("Reservation failed: Table %d does not exist for section B.\n", table_num);
                return;
            }

            // Reserve table for the name specified ie copy name to that struct
            // If cant reserve (already reserved by someone) : print "Cannot reserve table"

            // Release mutex
            break;
        default:
            printf("Reservation failed: Section %c does not exist.", section[0]);
            return;
        }
    return;
}

void reserveRandomTable(struct table *base, char *name_hld, char *section) {
    int idx = -1;
    int i;

    switch (section[0]) {
        case 'a':
        case 'A':
            // Capture mutex for section A

            // Look for empty table and reserve it ie copy name to that struct
            printf("hI");
            // If no empty table print : Cannot find empty table

            // Release mutex for section A
            break;
        case 'b':
        case 'B':
            // Capture mutex for section A

            // Look for empty table and reserve it ie copy name to that struct
            printf("hI");

            // If no empty table print : Cannot find empty table

            // Release mutex for section A
            break;
        default:
            printf("Reservation failed: Section %c does not exist.", section[0]);
            return;
    }
}

int processCmd(char *cmd, struct table *base) {
    int table_num;
    char *token;
    char *name_hld;
    char *section;
    char *table_char;
    token = strtok(cmd, " ");

    switch (token[0]) {
        case 'r':
            name_hld = strtok(NULL, " ");
            section = strtok(NULL, " ");
            table_char = strtok(NULL, " ");

            if (table_char != NULL) {
                table_num = atoi(table_char);
                reserveSpecificTable(base, name_hld, section, table_num);
            } else {
                reserveRandomTable(base, name_hld, section);
            }

            sleep(rand() % 10);
            break;
        case 's':
            printTableInfo(base);
            break;
        case 'i':
            initTables(base);
            break;
        case 'e':
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int fd_stdin = STDIN_FILENO;
    int mem_size = sizeof(struct table) * BUFF_SIZE;

    // File name is specifed
    if (argc > 1) {
        // Perform stdin rewiring if input file exists
        if (access(argv[1], R_OK) == 0) {
            close(0);
            open(argv[1], O_RDONLY);
        } else {
            printf("The specified input file does not exist.\n");
            exit(-1);
        }
    }

    // Open mutex BUFF_MUTEX_A and BUFF_MUTEX_B with inital value 1 using sem_open
    // mutexA = ;
    // mutexB = ;

    // Opening the shared memory buffer ie BUFF_SHM using shm open
    shm_fd = shm_open(BUFF_SHM, O_CREAT | O_RDWR, S_IRWXU);
    if (shm_fd == -1) {
        printf("Shared memory failed: %s.\n", strerror(errno));
        exit(-1);
    }

    // Configuring the size of the shared memory to sizeof(struct table) * BUFF_SIZE using ftruncate
    ftruncate(shm_fd, mem_size);

    // Map this shared memory to kernel space
    base = mmap(NULL , mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (base == MAP_FAILED) {
        // Close and shm_unlink
        printf("Memory map failed: %s.\n", strerror(errno));
        shm_unlink(BUFF_SHM);
        exit(-1);
    }

    // Intialising random number generator
    time_t now;
    srand((unsigned int)(time(&now)));

    // Array in which the user command is held
    char cmd[MAX_ARGS];
    int cmdType;
    int retStatus = 1;

    while (retStatus) {
        printf("\n>> ");
        gets(cmd);

        if (argc > 1)
            printf("Executing Command: %s\n", cmd);

        retStatus = processCmd(cmd, base);
    }

    // Close the semphores


    // Reset the standard input
    if (argc > 1) {
        close(0);
        dup(fd_stdin);
    }

    // Unmap the shared memory
    munmap(base, mem_size);
    close(shm_fd);
    return 0;
}