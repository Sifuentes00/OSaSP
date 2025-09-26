#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <stdlib.h> 

typedef struct {
    uint8_t type;       
    uint16_t hash;      
    uint8_t size;       
    uint8_t data[];     
} Message;

#define MESSAGE_HEADER_SIZE (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint8_t))
#define PADDED_DATA_SIZE(s) ((((s) + 1) + 3) & ~3)
#define TOTAL_MESSAGE_SIZE(s) (MESSAGE_HEADER_SIZE + PADDED_DATA_SIZE(s))

uint16_t calculate_hash(const Message *msg);
Message* create_message(unsigned int *seed);
void destroy_message(Message *msg);

#endif 