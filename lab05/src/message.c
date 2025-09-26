#include "message.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stddef.h>

uint16_t calculate_hash(const Message *msg) {
    uint16_t hash_val = 0;
    if (!msg) {
        fprintf(stderr, "Error in calculate_hash: msg is NULL\n");
        return 0;
    }

    size_t data_len = msg->size + 1; 
    size_t padded_data_size = PADDED_DATA_SIZE(msg->size); 

    hash_val ^= msg->type;
    hash_val ^= msg->size;

    for (size_t i = 0; i < data_len; ++i) {
       if (i >= padded_data_size) {
             fprintf(stderr, "CRITICAL ERROR in calculate_hash: Read index %zu out of bounds (max allowed index %zu) for msg->size %u\n",
                     i, padded_data_size - 1, msg->size);
             return 0xFFFF;
         }
         hash_val ^= msg->data[i];
    }

    hash_val = (hash_val << 8) | (hash_val >> 8);
    return hash_val;
}

Message* create_message(unsigned int *seed) {
    uint8_t data_size_field = rand_r(seed) % (MAX_DATA_SIZE + 1); 
    size_t alloc_size = TOTAL_MESSAGE_SIZE(data_size_field)+1;
    size_t padded_data_size = PADDED_DATA_SIZE(data_size_field); 

    Message *msg = (Message *)malloc(alloc_size);
    if (!msg) {
        perror("Failed to allocate message");
        return NULL;
    }

    msg->type = rand_r(seed) % 256;
    msg->size = data_size_field;
    msg->hash = 0;

    size_t actual_data_len = data_size_field + 1; 
    for (size_t i = 0; i < actual_data_len; ++i) {

        if (i >= padded_data_size) {
             fprintf(stderr, "CRITICAL ERROR in create_message: Write index %zu out of bounds (max allowed index %zu) for size_field %u\n",
                     i, padded_data_size - 1, data_size_field);
             free(msg);
             return NULL;
         }
        msg->data[i] = rand_r(seed) % 256;
    }


    if (padded_data_size > actual_data_len) {
        memset(msg->data + actual_data_len, 0, padded_data_size - actual_data_len);
    }

    msg->hash = calculate_hash(msg);

    return msg;
}

void destroy_message(Message *msg) {
    if (msg) {
        free(msg);
    }
}