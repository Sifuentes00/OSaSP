#define _GNU_SOURCE

#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct
{
    int first;
    int second;
} pair;

void init_signals_handling();
void term_signal_handler(int signo);

pair occurrence;
size_t c00 = 0;
size_t c01 = 0;
size_t c10 = 0;
size_t c11 = 0;

void update_stats()
{
    static int counter = 0;
    switch (counter)
    {
        case 0:
            occurrence.first = 0;
            occurrence.second = 0;
            break;
        case 1:
            occurrence.first = 1;
            occurrence.second = 0;
            break;
        case 2:
            occurrence.first = 0;
            occurrence.second = 1;
            break;
        case 3:
            occurrence.first = 1;
            occurrence.second = 1;
            break;
    }
    counter = (counter + 1) % 4;
}

int main(int argc, char *argv[])
{
    srand(time(NULL) ^ getpid());
    init_signals_handling();

    for(;;)
    {
        usleep(30000);

        update_stats();

        if (occurrence.first == 0 && occurrence.second == 0) c00++;
        else if (occurrence.first == 1 && occurrence.second == 0) c01++;
        else if (occurrence.first == 0 && occurrence.second == 1) c10++;
        else if (occurrence.first == 1 && occurrence.second == 1) c11++;


        if ((c00 + c01 + c10 + c11) >= 101)
        {
            union sigval info;
            info.sival_int = getpid();

            if (sigqueue(getppid(), SIGUSR1, info) == -1) {
                 perror("Child: Failed sending SIGUSR1, exiting");
                 exit(1);
            }

            printf("-------------------------------------------\n");
            printf("Child PID: %5d | Parent PID: %5d | Stats: ", (int) getpid(), (int) getppid());
            printf("00=%zu; 01=%zu; 10=%zu; 11=%zu\n", c00, c01, c10, c11);
            fflush(stdout);

            if (sigqueue(getppid(), SIGUSR2, info) == -1) {
                 perror("Child: Failed sending SIGUSR2, exiting");
                 exit(1);
            }

            c00 = 0;
            c01 = 0;
            c10 = 0;
            c11 = 0;
        }
    }
    return 0;
}

void init_signals_handling()
{
    struct sigaction action_term = {0};
    sigset_t set_term;

    sigemptyset(&set_term);
    action_term.sa_flags = 0;
    action_term.sa_mask = set_term;
    action_term.sa_handler = term_signal_handler;
    if (sigaction(SIGTERM, &action_term, NULL) == -1) {
        perror("sigaction SIGTERM failed");
        exit(errno);
    }
    if (sigaction(SIGINT, &action_term, NULL) == -1) {
        perror("sigaction SIGINT failed");
        exit(errno);
    }
}

void term_signal_handler(int signo)
{
    printf("Child %d received signal %d, exiting.\n", getpid(), signo);
    fflush(stdout);
    exit(0);
}
