#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
#include <signal.h> 
#include <termios.h> 
#include <unistd.h> 
#include <fcntl.h>   
#include <stdio.h>   
#include <stdlib.h>  
#include <stdint.h>  
#include <string.h>  
#include <time.h>    
#include <errno.h>  

#define _POSIX_C_SOURCE 200809L 
#define INITIAL_QUEUE_SIZE 10     
#define MAX_THREADS 100          
#define MAX_DATA_SIZE 255   

extern volatile sig_atomic_t keep_running;

typedef struct {
    int id;         
    unsigned int seed; 
} thread_arg_t;

extern pthread_t producer_threads[MAX_THREADS];
extern pthread_t consumer_threads[MAX_THREADS];

extern volatile sig_atomic_t producer_active[MAX_THREADS];
extern volatile sig_atomic_t consumer_active[MAX_THREADS];

extern int producer_count;
extern int consumer_count;

extern int next_producer_id;
extern int next_consumer_id;

int kbhit(void);        
char getch_nonblock();

#endif 