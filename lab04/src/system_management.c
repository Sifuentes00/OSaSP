#include "system_management.h"
#include "shared_buffer_types.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>

extern int semaphore_set_id;
extern pid_t producer_pids[];
extern size_t active_producers_count;
extern pid_t consumer_pids[];
extern size_t active_consumers_count;
extern QueueBuffer *shared_queue_ptr;

static pid_t main_process_pid;

union SemaphoreArg {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void system_init(void) {
    main_process_pid = getpid();

    int shmid = shmget(SHARED_MEM_IPC_KEY, sizeof(QueueBuffer), SHARED_MEM_PERMS);
    if (shmid == -1) {
        perror("shmget failed");
        exit(errno);
    }

    shared_queue_ptr = (QueueBuffer *)shmat(shmid, NULL, 0);
    if (shared_queue_ptr == (void *)-1) {
        perror("shmat failed");
        exit(errno);
    }

    initialize_queue();

    semaphore_set_id = semget(SEMAPHORE_IPC_KEY, 3, SEMAPHORE_PERMS);
    if (semaphore_set_id == -1) {
        perror("semget failed");
        exit(errno);
    }

    union SemaphoreArg arg;

    arg.val = 1;
    if (semctl(semaphore_set_id, QUEUE_MUTEX_IDX, SETVAL, arg) == -1) {
        perror("semctl mutex initialization failed");
        exit(errno);
    }

    arg.val = BUFFER_CAPACITY;
    if (semctl(semaphore_set_id, EMPTY_SLOTS_SEM_IDX, SETVAL, arg) == -1) {
        perror("semctl free_slots initialization failed");
        exit(errno);
    }

    arg.val = 0;
    if (semctl(semaphore_set_id, FILLED_SLOTS_SEM_IDX, SETVAL, arg) == -1) {
        perror("semctl filled_slots initialization failed");
        exit(errno);
    }
}

void system_cleanup(void) {
    if (getpid() == main_process_pid) {
        for (size_t i = 0; i < active_producers_count; ++i) {
            kill(producer_pids[i], SIGKILL);
            wait(NULL);
        }
        for (size_t i = 0; i < active_consumers_count; ++i) {
            kill(consumer_pids[i], SIGKILL);
            wait(NULL);
        }

        int shmid = shmget(SHARED_MEM_IPC_KEY, 0, 0);
        if (shmid != -1) {
            shmctl(shmid, IPC_RMID, NULL);
        }

        if (semaphore_set_id != -1) {
            semctl(semaphore_set_id, 0, IPC_RMID);
        }
    }
}
