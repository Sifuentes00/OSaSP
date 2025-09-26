#include "thread_funcs.h"
#include "message.h"
#include "queue_sem.h"  
#include "queue_cond.h" 
#include "utils.h"
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h> 
#include <time.h>   
#include <errno.h>  
#include <pthread.h> 

void* producer_thread_sem(void* arg) {
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int id = t_arg->id;
    unsigned int seed = t_arg->seed; 
    long messages_produced_by_thread = 0;

    printf("Producer %d [Thread %lu]: Started (Semaphores).\n", id, pthread_self());

    while (keep_running && producer_active[id]) {
   
        if (queue.resize_request != 0) {
            struct timespec ts = {0, 10000000L}; 
            nanosleep(&ts, NULL); 
            continue; 
        }

        struct timespec wait_time;
        clock_gettime(CLOCK_REALTIME, &wait_time); 
        wait_time.tv_sec += 1;
        if (sem_timedwait(&queue.empty_slots, &wait_time) == -1) {
             if (errno == ETIMEDOUT) {
                 if (!keep_running || !producer_active[id]) break;
                 continue;
             } else {
                 perror("Producer sem_timedwait(empty)");
                 producer_active[id] = 0;
                 break;
             }
        }

        if (!keep_running || !producer_active[id]) {
             sem_post(&queue.empty_slots);
             break;
        }

        Message *msg = create_message(&seed);
        if (!msg) {
            sem_post(&queue.empty_slots);
            fprintf(stderr, "Producer %d: Failed to create message, skipping.\n", id);
            struct timespec error_delay_psem = {0, 100000000L}; 
            nanosleep(&error_delay_psem, NULL); 
            continue;
        }

        pthread_mutex_lock(&queue.mutex);

        if (queue.count >= queue.capacity || queue.resize_request != 0) {
             pthread_mutex_unlock(&queue.mutex);
             sem_post(&queue.empty_slots);
             destroy_message(msg);
             struct timespec retry_delay_psem = {0, 10000000L}; 
             nanosleep(&retry_delay_psem, NULL); 
             printf("Producer %d: Queue full or resize pending after creating message, retrying.\n", id);
             continue;
        }

        queue.buffer[queue.tail] = msg;
        queue.tail = (queue.tail + 1) % queue.capacity;
        queue.count++;
        queue.total_added++;
        long current_total_added = queue.total_added;

        pthread_mutex_unlock(&queue.mutex);
        sem_post(&queue.filled_slots);

        messages_produced_by_thread++;
        printf("Producer %d: Added msg (size=%d). Total Added: %ld. Queue: %d/%d\n",
               id, msg->size + 1, current_total_added, queue.count, queue.capacity);

        struct timespec print_delay = {0, 600000000L};
        nanosleep(&print_delay, NULL);
        long sleep_ns_psem = (rand_r(&seed) % 1000 + 500) * 1000000L;
        struct timespec work_delay_psem = {sleep_ns_psem / 1000000000L, sleep_ns_psem % 1000000000L};
        nanosleep(&work_delay_psem, NULL); 
    }

    producer_active[id] = 0;
    printf("Producer %d [Thread %lu]: Exiting (Semaphores). Produced %ld messages.\n", id, pthread_self(), messages_produced_by_thread);
    free(arg);
    return NULL;
}

void* consumer_thread_sem(void* arg) {
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int id = t_arg->id;
    unsigned int seed = t_arg->seed;
    long messages_consumed_by_thread = 0;

    printf("Consumer %d [Thread %lu]: Started (Semaphores).\n", id, pthread_self());

    while (keep_running && consumer_active[id]) {

        pthread_mutex_lock(&queue.mutex);
        int resize_req = queue.resize_request;
        int new_cap = queue.new_capacity_request;

        if (resize_req != 0) {
            printf("Consumer %d: Detected resize request (%d) to %d.\n", id, resize_req, new_cap);
            if (resize_queue_internal_sem(&queue, new_cap) == 0) {
                queue.resize_request = 0;
                printf("Consumer %d: Resize successful.\n", id);
            } else {
                queue.resize_request = 0;
                printf("Consumer %d: Resize failed, clearing request.\n", id);
            }
            pthread_mutex_unlock(&queue.mutex);
            continue;
        }
        pthread_mutex_unlock(&queue.mutex);

        struct timespec wait_time;
        clock_gettime(CLOCK_REALTIME, &wait_time); 
        wait_time.tv_sec += 1;
        if (sem_timedwait(&queue.filled_slots, &wait_time) == -1) {
             if (errno == ETIMEDOUT) {
                  if (!keep_running || !consumer_active[id]) break;
                  continue;
             } else {
                 perror("Consumer sem_timedwait(filled)");
                 consumer_active[id] = 0;
                 break;
             }
        }

        if (!keep_running || !consumer_active[id]) {
             sem_post(&queue.filled_slots);
             break;
        }

        pthread_mutex_lock(&queue.mutex);

        if (queue.resize_request == -1 && queue.count <= queue.new_capacity_request) {
             printf("Consumer %d: Performing pending resize down request before consuming.\n", id);
             if (resize_queue_internal_sem(&queue, queue.new_capacity_request) == 0) {
                 queue.resize_request = 0;
             } else {
                 queue.resize_request = 0;
                 printf("Consumer %d: Pending resize down failed.\n", id);
             }
             pthread_mutex_unlock(&queue.mutex);
             sem_post(&queue.filled_slots);
             continue;
        }
        else if (queue.resize_request == 1) {
             printf("Consumer %d: Performing pending resize up request before consuming.\n", id);
             if (resize_queue_internal_sem(&queue, queue.new_capacity_request) == 0) {
                 queue.resize_request = 0;
             } else {
                 queue.resize_request = 0;
                 printf("Consumer %d: Pending resize up failed.\n", id);
             }
             pthread_mutex_unlock(&queue.mutex);
             sem_post(&queue.filled_slots);
             continue;
        }


         if (queue.count == 0) {
             pthread_mutex_unlock(&queue.mutex);
             sem_post(&queue.filled_slots);
             fprintf(stderr, "Consumer %d: Woke up but queue empty! Sem count should prevent this.\n", id);
             continue;
         }

        Message *msg = queue.buffer[queue.head];
        queue.head = (queue.head + 1) % queue.capacity;
        queue.count--;
        queue.total_extracted++;
        long current_total_extracted = queue.total_extracted;
        int current_count = queue.count;
        int current_capacity = queue.capacity;

        pthread_mutex_unlock(&queue.mutex);
        sem_post(&queue.empty_slots);

        if (msg) {
            uint16_t expected_hash = msg->hash;
            uint16_t actual_hash = calculate_hash(msg);
            messages_consumed_by_thread++;
            printf("Consumer %d: Got msg (size=%d). Hash %s. Total Extracted: %ld. Queue: %d/%d\n",
                   id, msg->size + 1, (expected_hash == actual_hash ? "OK" : "FAIL!"),
                   current_total_extracted, current_count, current_capacity);

            if (expected_hash != actual_hash) {
                 fprintf(stderr, "Consumer %d: HASH MISMATCH! Expected %04x, Got %04x\n", id, expected_hash, actual_hash);
            }

            destroy_message(msg);
        } else {
             fprintf(stderr,"Consumer %d: ERROR - dequeued a NULL message!\n", id);
        }

        struct timespec print_delay = {0, 600000000L};
        nanosleep(&print_delay, NULL);
        long sleep_ns_csem = (rand_r(&seed) % 1000 + 500) * 1000000L;
        struct timespec work_delay_csem = {sleep_ns_csem / 1000000000L, sleep_ns_csem % 1000000000L};
        nanosleep(&work_delay_csem, NULL); 
    }

    consumer_active[id] = 0;
    printf("Consumer %d [Thread %lu]: Exiting (Semaphores). Consumed %ld messages.\n", id, pthread_self(), messages_consumed_by_thread);
    free(arg);
    return NULL;
}

void* producer_thread_cond(void* arg) {
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int id = t_arg->id;
    unsigned int seed = t_arg->seed;
    long messages_produced_by_thread = 0;

    printf("Producer %d [Thread %lu]: Started (Cond Var).\n", id, pthread_self());

    while (keep_running && producer_active[id]) {
        pthread_mutex_lock(&queue_cond.mutex);

        struct timespec wait_time;
        clock_gettime(CLOCK_REALTIME, &wait_time); 
        wait_time.tv_sec += 1;

        while ((queue_cond.count == queue_cond.capacity || queue_cond.resize_request != 0 || queue_cond.resize_in_progress) && keep_running && producer_active[id]) {
            int wait_result = pthread_cond_timedwait(&queue_cond.can_produce, &queue_cond.mutex, &wait_time);
            if (wait_result == ETIMEDOUT) {
                break;
            } else if (wait_result != 0) {
                perror("Producer pthread_cond_timedwait");
                keep_running = 0;
                producer_active[id] = 0;
                break;
            }
            clock_gettime(CLOCK_REALTIME, &wait_time); 
            wait_time.tv_sec += 1;
        }

        if (!keep_running || !producer_active[id]) {
            pthread_mutex_unlock(&queue_cond.mutex);
            break;
        }

        if (queue_cond.count == queue_cond.capacity || queue_cond.resize_request != 0 || queue_cond.resize_in_progress) {
             pthread_mutex_unlock(&queue_cond.mutex);
             struct timespec retry_delay_pcond = {0, 10000000L}; 
             nanosleep(&retry_delay_pcond, NULL); 
             continue;
        }

        Message *msg = create_message(&seed);
        if (!msg) {
            pthread_mutex_unlock(&queue_cond.mutex);
            fprintf(stderr, "Producer %d Cond: Failed to create message, skipping.\n", id);
            struct timespec error_delay_pcond = {0, 100000000L}; 
            nanosleep(&error_delay_pcond, NULL);
            continue;
        }

        queue_cond.buffer[queue_cond.tail] = msg;
        queue_cond.tail = (queue_cond.tail + 1) % queue_cond.capacity;
        queue_cond.count++;
        queue_cond.total_added++;
        long current_total_added = queue_cond.total_added;

        pthread_cond_signal(&queue_cond.can_consume);
        pthread_mutex_unlock(&queue_cond.mutex);

        messages_produced_by_thread++;
        printf("Producer %d Cond: Added msg (size=%d). Total Added: %ld. Queue: %d/%d\n",
               id, msg->size + 1, current_total_added, queue_cond.count, queue_cond.capacity);

        struct timespec print_delay = {0, 600000000L}; 
        nanosleep(&print_delay, NULL);
        long sleep_ns_pcond = (rand_r(&seed) % 1000 + 500) * 1000000L;
        struct timespec work_delay_pcond = {sleep_ns_pcond / 1000000000L, sleep_ns_pcond % 1000000000L};
        nanosleep(&work_delay_pcond, NULL);
    }

    producer_active[id] = 0;
    printf("Producer %d [Thread %lu]: Exiting (Cond Var). Produced %ld messages.\n", id, pthread_self(), messages_produced_by_thread);

    pthread_mutex_lock(&queue_cond.mutex);
    pthread_cond_broadcast(&queue_cond.can_produce);
    pthread_cond_broadcast(&queue_cond.can_consume);
    pthread_mutex_unlock(&queue_cond.mutex);
    free(arg);
    return NULL;
}

void* consumer_thread_cond(void* arg) {
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int id = t_arg->id;
    unsigned int seed = t_arg->seed;
    long messages_consumed_by_thread = 0;

    printf("Consumer %d [Thread %lu]: Started (Cond Var).\n", id, pthread_self());

    while (keep_running && consumer_active[id]) {
         pthread_mutex_lock(&queue_cond.mutex);

        if (queue_cond.resize_request != 0 && !queue_cond.resize_in_progress) {
             int new_cap = queue_cond.new_capacity_request;
             int request_type = queue_cond.resize_request;
             int can_resize_now = 0;

             if (request_type == 1) {
                 can_resize_now = 1;
             } else if (request_type == -1) {
                 if (queue_cond.count <= new_cap) {
                      can_resize_now = 1;
                 }
             }

             if (can_resize_now) {
                 printf("Consumer %d Cond: Attempting resize request (%d) to %d\n", id, request_type, new_cap);
                 if (resize_queue_internal_cond(&queue_cond, new_cap) == 0) {
                     queue_cond.resize_request = 0;
                     printf("Consumer %d Cond: Resize successful.\n", id);
                 } else {
                     queue_cond.resize_request = 0;
                     printf("Consumer %d Cond: Resize failed.\n", id);
                 }
                 pthread_mutex_unlock(&queue_cond.mutex);
                 struct timespec resize_delay_ccond = {0, 5000000L}; 
                 nanosleep(&resize_delay_ccond, NULL); 
                 continue;
             }
        }

         struct timespec wait_time;
         clock_gettime(CLOCK_REALTIME, &wait_time); 
         wait_time.tv_sec += 1;

         while (queue_cond.count == 0 && keep_running && consumer_active[id]) {
              int wait_result = pthread_cond_timedwait(&queue_cond.can_consume, &queue_cond.mutex, &wait_time);
              if (wait_result == ETIMEDOUT) {
                  break;
              } else if (wait_result != 0) {
                   perror("Consumer pthread_cond_timedwait");
                   keep_running = 0;
                   consumer_active[id] = 0;
                   break;
              }
             clock_gettime(CLOCK_REALTIME, &wait_time); 
             wait_time.tv_sec += 1;
         }

         if (!keep_running || !consumer_active[id]) {
             pthread_mutex_unlock(&queue_cond.mutex);
             break;
         }

         if (queue_cond.count == 0) {
              pthread_mutex_unlock(&queue_cond.mutex);
              struct timespec retry_delay_ccond = {0, 10000000L}; 
              nanosleep(&retry_delay_ccond, NULL); 
              continue;
         }

         Message *msg = queue_cond.buffer[queue_cond.head];
         queue_cond.head = (queue_cond.head + 1) % queue_cond.capacity;
         queue_cond.count--;
         queue_cond.total_extracted++;
         long current_total_extracted = queue_cond.total_extracted;
         int current_count = queue_cond.count;
         int current_capacity = queue_cond.capacity;

         pthread_cond_signal(&queue_cond.can_produce);

         if (queue_cond.resize_request == -1 && !queue_cond.resize_in_progress && queue_cond.count <= queue_cond.new_capacity_request) {
              printf("Consumer %d Cond: Attempting pending decrease request after consuming.\n", id);
              if (resize_queue_internal_cond(&queue_cond, queue_cond.new_capacity_request) == 0) {
                   queue_cond.resize_request = 0;
                   printf("Consumer %d Cond: Pending decrease successful.\n", id);
              } else {
                   queue_cond.resize_request = 0;
                   printf("Consumer %d Cond: Pending decrease failed.\n", id);
              }
         }

         pthread_mutex_unlock(&queue_cond.mutex);

         if (msg) {
             uint16_t expected_hash = msg->hash;
             uint16_t actual_hash = calculate_hash(msg);
             messages_consumed_by_thread++;
             printf("Consumer %d Cond: Got msg (size=%d). Hash %s. Total Extracted: %ld. Queue: %d/%d\n",
                    id, msg->size + 1, (expected_hash == actual_hash ? "OK" : "FAIL!"),
                    current_total_extracted, current_count, current_capacity);

             if (expected_hash != actual_hash) {
                 fprintf(stderr, "Consumer %d Cond: HASH MISMATCH! Expected %04x, Got %04x\n", id, expected_hash, actual_hash);
             }
             destroy_message(msg);
         } else {
              fprintf(stderr,"Consumer %d Cond: ERROR - dequeued NULL message!\n", id);
         }

         struct timespec print_delay = {0, 600000000L}; 
         nanosleep(&print_delay, NULL);
         long sleep_ns_ccond = (rand_r(&seed) % 1000 + 500) * 1000000L;
         struct timespec work_delay_ccond = {sleep_ns_ccond / 1000000000L, sleep_ns_ccond % 1000000000L};
         nanosleep(&work_delay_ccond, NULL);
    }

    consumer_active[id] = 0;
    printf("Consumer %d [Thread %lu]: Exiting (Cond Var). Consumed %ld messages.\n", id, pthread_self(), messages_consumed_by_thread);

    pthread_mutex_lock(&queue_cond.mutex);
    pthread_cond_broadcast(&queue_cond.can_produce);
    pthread_cond_broadcast(&queue_cond.can_consume);
    pthread_mutex_unlock(&queue_cond.mutex);
    free(arg);
    return NULL;
}