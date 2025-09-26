#include "queue_sem.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> 

CircularQueue_Sem queue;

int init_queue_sem(CircularQueue_Sem *q, int size) {
    q->buffer = (Message **)malloc(size * sizeof(Message *));
    if (!q->buffer) {
        perror("Failed to allocate queue buffer");
        return -1;
    }

    q->capacity = size;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->total_added = 0;
    q->total_extracted = 0;
    q->resize_request = 0;
    q->new_capacity_request = 0;

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        perror("Mutex initialization failed");
        free(q->buffer);
        return -1;
    }

    if (sem_init(&q->empty_slots, 0, size) != 0) {
        perror("Semaphore empty_slots initialization failed");
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        return -1;
    }

    if (sem_init(&q->filled_slots, 0, 0) != 0) {
        perror("Semaphore filled_slots initialization failed");
        sem_destroy(&q->empty_slots);
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        return -1;
    }
    return 0; 
}


int resize_queue_internal_sem(CircularQueue_Sem *q, int new_capacity) {
    if (new_capacity <= 0) {
        fprintf(stderr, "Resize Error: Invalid new capacity %d\n", new_capacity);
        return -1;
    }

    if (new_capacity < q->count) {
         fprintf(stderr, "Resize Error: New capacity %d < current count %d. Cannot shrink.\n", new_capacity, q->count);
         return -1;
    }

    Message **new_buffer = (Message **)malloc(new_capacity * sizeof(Message *));
    if (!new_buffer) {
        fprintf(stderr, "Resize Error: Failed to allocate new buffer of size %d\n", new_capacity);
        return -1;
    }

    printf("Resizing queue from %d to %d (current count: %d)...\n", q->capacity, new_capacity, q->count);

    int current_index = q->head;
    for (int i = 0; i < q->count; ++i) {
        new_buffer[i] = q->buffer[current_index];
        current_index = (current_index + 1) % q->capacity; 
    }

    free(q->buffer); 

    q->buffer = new_buffer;
    q->head = 0;          
    q->tail = q->count;  
    int old_capacity = q->capacity;
    q->capacity = new_capacity;

    int diff = new_capacity - old_capacity;
    if (diff > 0) { 
        for (int i = 0; i < diff; ++i) {
            if (sem_post(&q->empty_slots) != 0) {
                perror("sem_post failed during resize up");
            }
        }
    } else if (diff < 0) { 
        int failed_trywaits = 0;
        for (int i = 0; i < -diff; ++i) {
            if (sem_trywait(&q->empty_slots) != 0) {
                if (errno == EAGAIN) {
                    failed_trywaits++; 
                } else {
                    perror("sem_trywait failed during resize down");
                }
            }
        }
        if(failed_trywaits > 0) {
             printf("Note: During resize down, %d empty slots could not be immediately removed via trywait.\n", failed_trywaits);
        }
    }

    printf("Resize complete. New capacity: %d, Count: %d\n", q->capacity, q->count);
    return 0;
}


void destroy_queue_sem(CircularQueue_Sem *q) {

    if (pthread_mutex_lock(&q->mutex) != 0) {
        perror("Failed to lock mutex for destroy");
    }

    printf("Destroying queue (Sem)...\n");
    while (q->count > 0) {
        Message *msg = q->buffer[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;
        if (msg) { 
            destroy_message(msg);
        }
    }

    free(q->buffer); 
    q->buffer = NULL;
    q->capacity = 0;
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    pthread_mutex_unlock(&q->mutex);

    if (sem_destroy(&q->empty_slots) != 0) perror("Failed to destroy empty_slots semaphore");
    if (sem_destroy(&q->filled_slots) != 0) perror("Failed to destroy filled_slots semaphore");
    if (pthread_mutex_destroy(&q->mutex) != 0) perror("Failed to destroy mutex");

    printf("Queue (Sem) destroyed.\n");
}