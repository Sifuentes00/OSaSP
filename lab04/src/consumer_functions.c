#include "consumer_functions.h"
#include "shared_buffer_types.h"

extern QueueBuffer *shared_queue_ptr;
extern int semaphore_set_id;
extern pid_t consumer_pids[];
extern size_t active_consumers_count;


void spawn_consumer_process(void) {
    if (active_consumers_count >= MAX_CHILD_PROCESSES - 1) {
        fputs("Maximum number of consumers reached.\n", stderr);
        return;
    }

    switch (consumer_pids[active_consumers_count] = fork()) {
        case 0:
            break;

        case -1:
            perror("fork failed for consumer");
            exit(errno);

        default:
            active_consumers_count++;
            return;
    }

    MsgPayload retrieved_message_payload;
    size_t messages_extracted_local_count;

    while (1) {
    memset(&retrieved_message_payload, 0, sizeof(MsgPayload));
    
        semaphore_wait(FILLED_SLOTS_SEM_IDX);

        semaphore_wait(QUEUE_MUTEX_IDX);
        messages_extracted_local_count = get_message_from_queue(&retrieved_message_payload);
        semaphore_signal(QUEUE_MUTEX_IDX);

        semaphore_signal(EMPTY_SLOTS_SEM_IDX);
        process_and_verify_message(&retrieved_message_payload);

        printf("Consumer pid: %d, consumed msg: hash=%" PRIX16 ", total_extracted=%zu\n",
               getpid(), retrieved_message_payload.msg_hash, messages_extracted_local_count);

        sleep(5);
    }
}

void terminate_last_consumer(void) {
    if (active_consumers_count == 0) {
        fputs("No consumers to terminate.\n", stderr);
        return;
    }

    active_consumers_count--;
    kill(consumer_pids[active_consumers_count], SIGKILL);
    wait(NULL);
}

void process_and_verify_message(MsgPayload *msg) {
    uint16_t original_msg_hash = msg->msg_hash;
    msg->msg_hash = 0;

    uint16_t calculated_checksum = compute_checksum(msg);

    if (original_msg_hash != calculated_checksum) {
        fprintf(stderr, "Checksum mismatch! Calculated=%" PRIX16 " vs Original=%" PRIX16 "\n",
                calculated_checksum, original_msg_hash);
    }
    msg->msg_hash = original_msg_hash;
}
