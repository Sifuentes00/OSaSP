#include "utils.h"
#include "message.h"
#include "queue_sem.h"
#include "queue_cond.h"
#include "thread_funcs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h> 
#include <time.h>  


void show_status_sem();
void show_status_cond();
void request_resize_sem(char op);
void request_resize_cond(char op);
void run_lab_logic(
    void* (*producer_func)(void*),
    void* (*consumer_func)(void*),
    void (*show_status_func)(),
    void (*request_resize_func)(char),
    void (*signal_cleanup_func)(), 
    const char* lab_name);
void signal_cleanup_sem();
void signal_cleanup_cond();
void clear_stdin_buffer();


int main() {
    srand(time(NULL)); 

    char choice;
    int run_main_loop = 1;

    while (run_main_loop) {

        printf("\n--- Main Menu ---\n");
        printf("1: Run Lab. 5.1 (Semaphores)\n");
        printf("2: Run Lab. 5.2 (Cond. Variables)\n");
        printf("q: Exit\n");
        printf("Your choice: ");


        choice = getchar();
        clear_stdin_buffer(); 

        switch (choice) { 
            case '1':
                printf("\n--- starting lab 5.1 ---\n");
                if (init_queue_sem(&queue, INITIAL_QUEUE_SIZE) == 0) {
                    run_lab_logic(producer_thread_sem, consumer_thread_sem,
                                  show_status_sem, request_resize_sem,
                                  signal_cleanup_sem, "semaphores");
                    destroy_queue_sem(&queue);
                    printf("\n--- lab 5.1 complited ---\n");
                } else {
                    fprintf(stderr, "Error initializing queue (semaphores)\n");
                }

                break;

            case '2':
                printf("\n--- starting lab 5.2 ---\n");
                if (init_queue_cond(&queue_cond, INITIAL_QUEUE_SIZE) == 0) {
                     run_lab_logic(producer_thread_cond, consumer_thread_cond,
                                   show_status_cond, request_resize_cond,
                                   signal_cleanup_cond, "condition var");
                    destroy_queue_cond(&queue_cond);
                     printf("\n--- lab 5.2 complited ---\n");
                } else {
                     fprintf(stderr, "Error initializing queue (condition var)\n");
                }
                break;

            case 'q':
                printf("Exit: \n");
                run_main_loop = 0; 
                break;

            default:
                printf("Incorrect selection. Please try again.\n");
                break;
        }
    }

    return 0; 
}

void show_status_sem() {
    pthread_mutex_lock(&queue.mutex); 
    printf("\n--- STATUS (Semaphores) ---\n");
    printf("Queue capacity: %d\n",queue.capacity);
    printf("Elements occupied: %d\n",queue.count);
    printf("Free slots: %d\n",queue.capacity -queue.count);
    int empty_val, filled_val;
    sem_getvalue(&queue.empty_slots, &empty_val);
    sem_getvalue(&queue.filled_slots, &filled_val);
    printf("Semaphore empty_slots:  %d\n", empty_val);
    printf("Semaphore filled_slots: %d\n", filled_val);
    printf("Total added:    %ld\n", queue.total_added);
    printf("Total extracted:    %ld\n", queue.total_extracted);
    printf("Active producers: %d\n", producer_count);
    printf("Active consumers:   %d\n", consumer_count);
    printf("Resize request: %d (Target: %d)\n", queue.resize_request, queue.new_capacity_request);
    printf("------------------------\n");
    pthread_mutex_unlock(&queue.mutex);
}

void show_status_cond() {
    pthread_mutex_lock(&queue_cond.mutex);
    printf("\n--- STATUS (Cond. Var.) ---\n");
    printf("Queue capacity: %d\n", queue_cond.capacity);
    printf("Elements occupied: %d\n", queue_cond.count);
    printf("Free spaces: %d\n", queue_cond.capacity - queue_cond.count);
    printf("Total added: %ld\n", queue_cond.total_added);
    printf("Total extracted: %ld\n", queue_cond.total_extracted);
    printf("Active producers: %d\n", producer_count);
    printf("Active consumers: %d\n", consumer_count);
    printf("Resize request: %d (Target: %d)\n", queue_cond.resize_request, queue_cond.new_capacity_request);
        printf("Resize in progress: %d\n", queue_cond.resize_in_progress);
        printf("---------------------------------------\n");
        pthread_mutex_unlock(&queue_cond.mutex);
}

void request_resize_sem(char op) {
    pthread_mutex_lock(&queue.mutex);
    if (queue.resize_request == 0) {
        int current_capacity = queue.capacity;
        int target_size = (op == '+') ? current_capacity + 5 : current_capacity - 5;
        if (target_size <= 0 && op == '-') {
            printf("Cannot reduce size further (semaphores).\n");
        } else {
            queue.new_capacity_request = target_size;
            queue.resize_request = (op == '+') ? 1 : -1;
            printf("Request for %s queue to %d (semaphores).\n", 
                (op == '+') ? "increase" : "decrease", target_size);
        }
    } else {
        printf("Resize request already in queue (semaphores).\n");
    }
    pthread_mutex_unlock(&queue.mutex);
}

void request_resize_cond(char op) {
    pthread_mutex_lock(&queue_cond.mutex);
    if (queue_cond.resize_request == 0 && !queue_cond.resize_in_progress) {
        int current_capacity = queue_cond.capacity;
        int target_size = (op == '+') ? current_capacity + 5 : current_capacity - 5;
        if (target_size <= 0 && op == '-') {
            printf("Main: Cannot reduce size further (condition var).\n");
        } else {
            queue_cond.new_capacity_request = target_size;
            queue_cond.resize_request = (op == '+') ? 1 : -1;
            printf("Main: Request for %s queue to %d (condition var).\n",
                (op == '+') ? "increase" : "decrease", target_size);
            pthread_cond_signal(&queue_cond.can_consume); 
            if (op == '-') {
                 pthread_cond_broadcast(&queue_cond.can_produce); 
            }
        }
    } else {
        printf("A resize request is already in progress or pending (condition var.).\n");
    }
    pthread_mutex_unlock(&queue_cond.mutex);
}

void run_lab_logic(
    void* (*producer_func)(void*),
    void* (*consumer_func)(void*),
    void (*show_status_func)(),
    void (*request_resize_func)(char),
    void (*signal_cleanup_func)(), 
    const char* lab_name)
{
    printf("--- Start: %s ---\n", lab_name);

    keep_running = 1;
    producer_count = 0;
    consumer_count = 0;
    next_producer_id = 0;
    next_consumer_id = 0;
    for (int i = 0; i < MAX_THREADS; ++i) {
        producer_active[i] = 0;
        consumer_active[i] = 0;
    }
    memset(producer_threads, 0, sizeof(producer_threads));
    memset(consumer_threads, 0, sizeof(consumer_threads));

    printf("Control:\n");
    printf(" p: Add producer            c: Add consumer\n");
    printf(" P: Remove producer         C: Remove consumer\n");
    printf(" +: Increase queue by 5     -: Decrease queue by 5\n");
    printf(" s: Show status             q: Exit (%s)\n", lab_name);

    while (keep_running) {
        char cmd = getch_nonblock();
        if (cmd) {
            int target_id = -1;
            switch (cmd) {
                case 'p': 
                    if (producer_count < MAX_THREADS) {
                        int current_id = next_producer_id++;
                        producer_active[current_id] = 1;
                        thread_arg_t *arg = malloc(sizeof(thread_arg_t));
                        if (!arg) { perror("malloc thread arg failed"); next_producer_id--; break; }
                        arg->id = current_id; arg->seed = rand();
                        if (pthread_create(&producer_threads[current_id], NULL, producer_func, arg) != 0) {
                            perror("Error creating producer thread");
                            producer_active[current_id] = 0; next_producer_id--; free(arg);
                        } else {
                             producer_count++; printf("Producer was created %d (%s)\n", current_id, lab_name);
                        }
                    } else printf("Max number of producers reached.\n");
                    break;
                case 'c': 
                     if (consumer_count < MAX_THREADS) {
                        int current_id = next_consumer_id++;
                        consumer_active[current_id] = 1;
                        thread_arg_t *arg = malloc(sizeof(thread_arg_t));
                        if (!arg) { perror("malloc thread arg failed"); next_consumer_id--; break; }
                        arg->id = current_id; arg->seed = rand();
                        if (pthread_create(&consumer_threads[current_id], NULL, consumer_func, arg) != 0) {
                             perror("Error creating consumer thread");
                             consumer_active[current_id] = 0; next_consumer_id--; free(arg);
                        } else {
                             consumer_count++; printf("Consumer was created %d (%s)\n", current_id, lab_name);
                         }
                     } else printf("Max number of consumers reached.\n");
                    break;
                 case 'P':
                    target_id = -1;
                    for(int i = next_producer_id - 1; i >= 0; --i) if(producer_active[i]) { target_id = i; break; }
                    if (target_id != -1) {
                         printf("Main: Signaling Producer %d to terminate (%s)...\n", target_id, lab_name);
                         producer_active[target_id] = 0;
                         producer_count--;
                         signal_cleanup_func();
                     } else printf("There are no active producers to remove.\n");
                     break;
                 case 'C': 
                    target_id = -1;
                    for(int i = next_consumer_id - 1; i >= 0; --i) if(consumer_active[i]) { target_id = i; break; }
                    if (target_id != -1) {
                         printf("Main: Signaling Consumer %d to terminate (%s)...\n", target_id, lab_name);
                         consumer_active[target_id] = 0;
                         consumer_count--;
                         signal_cleanup_func(); 
                     } else printf("There are no active consumers to remove.\n");
                    break;
                 case '+': request_resize_func('+'); break;
                 case '-': request_resize_func('-'); break;
                 case 's': show_status_func(); break;
                 case 'q':
                    printf("Main: Shutdown (%s)...\n", lab_name);
                    keep_running = 0;
                    signal_cleanup_func(); 
                    break;
            }
        }
        struct timespec delay = {0, 50000000L}; 
        nanosleep(&delay, NULL);
    }


    printf("Waiting for threads to complete (%s)...\n", lab_name);

    for (int i = 0; i < next_producer_id; ++i) producer_active[i] = 0;
    for (int i = 0; i < next_consumer_id; ++i) consumer_active[i] = 0;
 
    signal_cleanup_func();
    struct timespec short_delay = {0, 200000000L}; 
    nanosleep(&short_delay, NULL); 
    signal_cleanup_func();

    printf("Attaching Producer (%s)...\n", lab_name);
    for (int i = 0; i < next_producer_id; ++i) if (producer_threads[i] != 0) pthread_join(producer_threads[i], NULL);
    printf("Attaching Consumers (%s)...\n", lab_name);
    for (int i = 0; i < next_consumer_id; ++i) if (consumer_threads[i] != 0) pthread_join(consumer_threads[i], NULL);
    printf("Main: All streams (%s) are complited.\n", lab_name);
}

void signal_cleanup_sem() {
    pthread_mutex_lock(&queue.mutex);
    int cap = queue.capacity;
    pthread_mutex_unlock(&queue.mutex);
    int signals = cap + producer_count + consumer_count + 5;
    for(int i=0; i < signals; ++i) {
        sem_post(&queue.empty_slots);
        sem_post(&queue.filled_slots);
    }
}

void signal_cleanup_cond() {
    pthread_mutex_lock(&queue_cond.mutex);
    pthread_cond_broadcast(&queue_cond.can_produce);
    pthread_cond_broadcast(&queue_cond.can_consume);
    pthread_mutex_unlock(&queue_cond.mutex);
}

void clear_stdin_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

