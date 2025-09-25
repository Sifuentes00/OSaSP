#ifndef RECORD_H
#define RECORD_H

#include <stdint.h>

#define NAME_LEN   80
#define ADDR_LEN   80

typedef struct record_s {
    char     device_name[NAME_LEN];  
    char     location[ADDR_LEN];    
    uint32_t inventory_number;      
} record_t;

#endif
