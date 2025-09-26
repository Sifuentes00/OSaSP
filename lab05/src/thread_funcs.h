#ifndef THREAD_FUNCS_H
#define THREAD_FUNCS_H

#include "utils.h" 

void* producer_thread_sem(void* arg);
void* producer_thread_cond(void* arg);
void* consumer_thread_sem(void* arg);
void* consumer_thread_cond(void* arg);

#endif