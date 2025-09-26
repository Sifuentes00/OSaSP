#include "producer_functions.h"
#include "shared_buffer_types.h"

extern QueueBuffer *shared_queue_ptr;
extern int semaphore_set_id;
extern pid_t producer_pids[];
extern size_t active_producers_count;


void spawn_producer_process(void) {
    if (active_producers_count >= MAX_CHILD_PROCESSES - 1) {
        fputs("Maximum number of producers reached.\n", stderr);
        return;
    }

    switch (producer_pids[active_producers_count] = fork()) {
        case 0:
            srand(getpid());
            break;

        case -1:
            perror("fork failed for producer");
            exit(errno);

        default:
            active_producers_count++;
            return;
    }

    MsgPayload current_message_payload;
    size_t messages_added_local_count;

    while (1) {
    memset(&current_message_payload, 0, sizeof(MsgPayload));
    
        generate_and_fill_message(&current_message_payload);

        semaphore_wait(EMPTY_SLOTS_SEM_IDX);

        semaphore_wait(QUEUE_MUTEX_IDX);
        messages_added_local_count = add_message_to_queue(&current_message_payload);
        semaphore_signal(QUEUE_MUTEX_IDX);

        semaphore_signal(FILLED_SLOTS_SEM_IDX);

        printf("Producer pid: %d, added msg: hash=%" PRIX16 ", total_added=%zu\n",
               getpid(), current_message_payload.msg_hash, messages_added_local_count);

        sleep(5);
    }
}

void terminate_last_producer(void) {
    if (active_producers_count == 0) {
        fputs("No producers to terminate.\n", stderr);
        return;
    }

    active_producers_count--;
    kill(producer_pids[active_producers_count], SIGKILL);
    wait(NULL);
}

void generate_and_fill_message(MsgPayload *msg) {
    size_t random_value_for_size = rand() % 257;

    if (random_value_for_size == 256) {
        msg->msg_type = -1;
        msg->data_len_bytes = 0;
    } else {
        msg->msg_type = 0;
        msg->data_len_bytes = (uint8_t)random_value_for_size;
    }

    for (size_t i = 0; i < msg->data_len_bytes; ++i) {
        msg->payload_data[i] = (char)(rand() % 256);
    }

    msg->msg_hash = 0;
    msg->msg_hash = compute_checksum(msg);
}
