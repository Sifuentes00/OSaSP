#ifndef SHARED_BUFFER_TYPES_H_
#define SHARED_BUFFER_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>

#define MAX_PAYLOAD_SIZE (((256 + 3) / 4) * 4)
#define MAX_CHILD_PROCESSES 1024
#define BUFFER_CAPACITY 4096

#define QUEUE_MUTEX_IDX 0
#define EMPTY_SLOTS_SEM_IDX 1
#define FILLED_SLOTS_SEM_IDX 2

#define SHARED_MEM_PATH "/shared_queue_obj"
#define MUTEX_NAME "queue_access_mutex"
#define EMPTY_SEM_NAME "free_buffer_slots"
#define FILLED_SEM_NAME "messages_in_queue"


typedef struct
{
    int8_t msg_type;
    uint16_t msg_hash;
    uint8_t data_len_bytes;
    char payload_data[MAX_PAYLOAD_SIZE];
} MsgPayload;

typedef struct {
    size_t total_added_count;
    size_t total_extracted_count;

    size_t current_msg_count;

    size_t buffer_head_idx;
    size_t buffer_tail_idx;
    MsgPayload message_buffer[BUFFER_CAPACITY];

} QueueBuffer;

extern QueueBuffer *shared_queue_ptr;
extern int semaphore_set_id;


void semaphore_wait(int sem_num);
void semaphore_signal(int sem_num);

uint16_t compute_checksum(MsgPayload *msg);

void initialize_queue(void);

size_t add_message_to_queue(MsgPayload *msg);
size_t get_message_from_queue(MsgPayload *msg);


#endif
