#include "utils.h"

volatile sig_atomic_t keep_running = 1;

pthread_t producer_threads[MAX_THREADS];
pthread_t consumer_threads[MAX_THREADS];
volatile sig_atomic_t producer_active[MAX_THREADS] = {0};
volatile sig_atomic_t consumer_active[MAX_THREADS] = {0};
int producer_count = 0;
int consumer_count = 0;
int next_producer_id = 0;
int next_consumer_id = 0;

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar(); 

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); 
    fcntl(STDIN_FILENO, F_SETFL, oldf);      
    if (ch != EOF) {
        ungetc(ch, stdin); 
        return 1; 
    }

    return 0; 
}

char getch_nonblock() {
     if (kbhit()) {
         return getchar(); 
     }
     return 0;
}