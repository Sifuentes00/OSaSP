#ifndef QUEUE_COND_H
#define QUEUE_COND_H

#include "message.h"
#include "utils.h"
#include <pthread.h> 

typedef struct {
    Message **buffer;       
    int capacity;          
    int head;           
    int tail;             
    int count;             

    long total_added;       
    long total_extracted; 

    pthread_mutex_t mutex;      
    pthread_cond_t can_produce;  
    pthread_cond_t can_consume;  

    volatile sig_atomic_t resize_request;
    int new_capacity_request;
    volatile sig_atomic_t resize_in_progress; 

} CircularQueue_Cond;

extern CircularQueue_Cond queue_cond;

int init_queue_cond(CircularQueue_Cond *q, int size);

void destroy_queue_cond(CircularQueue_Cond *q);

int resize_queue_internal_cond(CircularQueue_Cond *q, int new_capacity);

#endif 