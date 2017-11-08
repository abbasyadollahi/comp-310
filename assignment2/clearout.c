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
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

#define BUFF_SHM "/OS_BUFF_SHM_ABBAS"
#define BUFF_MUTEX_A "/OS_MUTEX_A_ABBAS"
#define BUFF_MUTEX_B "/OS_MUTEX_B_ABBAS"

int main() {
    printf("Starting to clear namespaces.\n");
    // Clear buffer namespace

    // Unlink shm
    shm_unlink(BUFF_SHM);

    // Unlink semaphore
    sem_unlink(BUFF_MUTEX_A);
    sem_unlink(BUFF_MUTEX_B);

    printf("Cleared namespaces.\n");
    return 0;
}
