#ifndef CONSUMER_FUNCTIONS_H_
#define CONSUMER_FUNCTIONS_H_

#include "shared_buffer_types.h"

#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void spawn_consumer_process(void);
void terminate_last_consumer(void);
void process_and_verify_message(MsgPayload *msg);

#endif
