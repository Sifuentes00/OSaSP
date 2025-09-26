#ifndef PRODUCER_FUNCTIONS_H_
#define PRODUCER_FUNCTIONS_H_

#include "shared_buffer_types.h"

#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void spawn_producer_process(void);
void terminate_last_producer(void);
void generate_and_fill_message(MsgPayload *msg);

#endif
