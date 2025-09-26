#ifndef SYSTEM_MANAGEMENT_H_
#define SYSTEM_MANAGEMENT_H_

#include "shared_buffer_types.h"

#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <stdlib.h>

#define SHARED_MEM_IPC_KEY 0x1234
#define SEMAPHORE_IPC_KEY 0x5678

#define SHARED_MEM_PERMS (IPC_CREAT | 0666)
#define SEMAPHORE_PERMS (IPC_CREAT | 0666)

extern int semaphore_set_id;
extern pid_t producer_pids[MAX_CHILD_PROCESSES];
extern size_t active_producers_count;
extern pid_t consumer_pids[MAX_CHILD_PROCESSES];
extern size_t active_consumers_count;
extern QueueBuffer *shared_queue_ptr;

void system_init(void);
void system_cleanup(void);

#endif
