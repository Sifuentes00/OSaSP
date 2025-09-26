#ifndef QUEUE_SEM_H
#define QUEUE_SEM_H

#include "message.h"
#include "utils.h"    
#include <pthread.h>
#include <semaphore.h> 

typedef struct {
    Message **buffer;     
    int capacity;          
    int head;              
    int tail;             
    int count;             

    long total_added;     
    long total_extracted;   

    pthread_mutex_t mutex; 
    sem_t empty_slots;     
    sem_t filled_slots;  

    volatile sig_atomic_t resize_request; 
    int new_capacity_request; 
} CircularQueue_Sem;

extern CircularQueue_Sem queue;

int init_queue_sem(CircularQueue_Sem *q, int size);
void destroy_queue_sem(CircularQueue_Sem *q);
int resize_queue_internal_sem(CircularQueue_Sem *q, int new_capacity);

#endif