#include "shared_buffer_types.h"
#include <stddef.h>

extern QueueBuffer *shared_queue_ptr;
extern int semaphore_set_id;

uint16_t compute_checksum(MsgPayload *msg) {
    uint32_t hash_val = 5381;
    const uint8_t *byte_ptr = (const uint8_t*)msg;

    size_t data_to_hash_size = sizeof(msg->msg_type) + sizeof(msg->data_len_bytes) + msg->data_len_bytes;
    size_t hash_field_offset = offsetof(MsgPayload, msg_hash);

    for (size_t i = 0; i < data_to_hash_size; ++i) {
        if (i >= hash_field_offset && i < hash_field_offset + sizeof(msg->msg_hash)) {
            continue;
        }
        hash_val = ((hash_val << 5) + hash_val) + byte_ptr[i];
    }
    return (uint16_t)(hash_val & 0xFFFF);
}

void initialize_queue(void) {
    shared_queue_ptr->total_added_count = 0;
    shared_queue_ptr->total_extracted_count = 0;
    shared_queue_ptr->current_msg_count = 0;
    shared_queue_ptr->buffer_head_idx = 0;
    shared_queue_ptr->buffer_tail_idx = 0;
    memset(shared_queue_ptr->message_buffer, 0, sizeof(shared_queue_ptr->message_buffer));
}

size_t add_message_to_queue(MsgPayload *msg) {
    if (shared_queue_ptr->current_msg_count == BUFFER_CAPACITY - 1) {
        fputs("Queue is full, cannot add message!\n", stderr);
        exit(EXIT_FAILURE);
    }

    if (shared_queue_ptr->buffer_head_idx == BUFFER_CAPACITY) {
        shared_queue_ptr->buffer_head_idx = 0;
    }

    shared_queue_ptr->message_buffer[shared_queue_ptr->buffer_head_idx] = *msg;
    shared_queue_ptr->buffer_head_idx++;
    shared_queue_ptr->current_msg_count++;

    return ++shared_queue_ptr->total_added_count;
}

size_t get_message_from_queue(MsgPayload *msg) {
    if (shared_queue_ptr->current_msg_count == 0) {
        fputs("Queue is empty, cannot get message!\n", stderr);
        exit(EXIT_FAILURE);
    }

    if (shared_queue_ptr->buffer_tail_idx == BUFFER_CAPACITY) {
        shared_queue_ptr->buffer_tail_idx = 0;
    }

    *msg = shared_queue_ptr->message_buffer[shared_queue_ptr->buffer_tail_idx];
    shared_queue_ptr->buffer_tail_idx++;
    shared_queue_ptr->current_msg_count--;

    return ++shared_queue_ptr->total_extracted_count;
}

void semaphore_wait(int sem_num) {
    struct sembuf operation = {sem_num, -1, 0};
    if (semop(semaphore_set_id, &operation, 1)) {
        perror("semaphore_wait operation failed");
        exit(EXIT_FAILURE);
    }
}

void semaphore_signal(int sem_num) {
    struct sembuf operation = {sem_num, 1, 0};
    if (semop(semaphore_set_id, &operation, 1)) {
        perror("semaphore_signal operation failed");
        exit(EXIT_FAILURE);
    }
}
