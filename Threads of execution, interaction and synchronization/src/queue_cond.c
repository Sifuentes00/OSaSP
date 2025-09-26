#include "queue_cond.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> 
#include <unistd.h> 

CircularQueue_Cond queue_cond;

int init_queue_cond(CircularQueue_Cond *q, int size) {
    q->buffer = (Message **)malloc(size * sizeof(Message *));
    if (!q->buffer) {
        perror("Failed to allocate queue buffer (Cond Var)");
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
    q->resize_in_progress = 0; 

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        perror("Mutex initialization failed (Cond Var)");
        free(q->buffer);
        return -1;
    }
    if (pthread_cond_init(&q->can_produce, NULL) != 0) {
        perror("Condition variable can_produce initialization failed");
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        return -1;
    }
    if (pthread_cond_init(&q->can_consume, NULL) != 0) {
        perror("Condition variable can_consume initialization failed");
        pthread_cond_destroy(&q->can_produce);
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        return -1;
    }
    return 0;
}

int resize_queue_internal_cond(CircularQueue_Cond *q, int new_capacity) {
     if (new_capacity <= 0) {
        fprintf(stderr, "Resize Error: Invalid new capacity %d (Cond Var)\n", new_capacity);
        return -1;
    }

     if (new_capacity < q->count) {
          fprintf(stderr, "Resize Error: New capacity %d < current count %d. Cannot shrink (Cond Var).\n", new_capacity, q->count);
          return -1;
     }

     q->resize_in_progress = 1; 
     printf("Resizing queue (Cond Var) from %d to %d (Count: %d)...\n", q->capacity, new_capacity, q->count);

     Message **new_buffer = (Message **)malloc(new_capacity * sizeof(Message *));
     if (!new_buffer) {
          fprintf(stderr, "Resize Error: Failed to allocate new buffer of size %d (Cond Var)\n", new_capacity);
          q->resize_in_progress = 0; 
          return -1;
     }

     int current_index = q->head;
     for (int i = 0; i < q->count; ++i) {
         new_buffer[i] = q->buffer[current_index];
         current_index = (current_index + 1) % q->capacity;
     }

     free(q->buffer); 

     q->buffer = new_buffer;
     q->head = 0;
     q->tail = q->count;
     q->capacity = new_capacity;

     printf("Resize complete (Cond Var). New capacity: %d, Count: %d\n", q->capacity, q->count);
     q->resize_in_progress = 0; 

     pthread_cond_broadcast(&q->can_produce);
     pthread_cond_broadcast(&q->can_consume);

     return 0; 
}


void destroy_queue_cond(CircularQueue_Cond *q) {
    if (pthread_mutex_lock(&q->mutex) != 0) {
         perror("Failed to lock mutex for destroy (Cond Var)");
    }

    printf("Destroying queue (Cond Var)...\n");

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

    q->resize_request = 0;
    q->resize_in_progress = 1; 

    keep_running = 0; 
    pthread_cond_broadcast(&q->can_produce);
    pthread_cond_broadcast(&q->can_consume);

    pthread_mutex_unlock(&q->mutex);

    struct timespec destroy_delay = {0, 100000000L}; 
    nanosleep(&destroy_delay, NULL);

    if (pthread_cond_destroy(&q->can_produce) != 0) perror("Failed to destroy can_produce condition variable");
    if (pthread_cond_destroy(&q->can_consume) != 0) perror("Failed to destroy can_consume condition variable");
    if (pthread_mutex_destroy(&q->mutex) != 0) perror("Failed to destroy mutex (Cond Var)");

    printf("Queue (Cond Var) destroyed.\n");
}