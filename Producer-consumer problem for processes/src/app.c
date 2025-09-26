#include "system_management.h"
#include "producer_functions.h"
#include "consumer_functions.h"
#include "shared_buffer_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <limits.h>


QueueBuffer *shared_queue_ptr = NULL;
int semaphore_set_id = -1;
pid_t producer_pids[MAX_CHILD_PROCESSES] = {0};
pid_t consumer_pids[MAX_CHILD_PROCESSES] = {0};
size_t active_producers_count = 0;
size_t active_consumers_count = 0;


static const char command_options_text[] = {
    "Available Commands:\n"
    "[P] - Create Producer Process\n"
    "[p] - Terminate Last Producer Process\n"
    "[C] - Create Consumer Process\n"
    "[c] - Terminate Last Consumer Process\n"
    "[s] - Show Current Status\n"
    "[q] - Quit Program"
};

void display_current_status() {
    semaphore_wait(QUEUE_MUTEX_IDX);

    printf("\n--- System Status Report ---\n");
    printf("Active Producers: %zu\n", active_producers_count);
    printf("Active Consumers: %zu\n", active_consumers_count);
    printf("Messages in Buffer: %zu/%d\n", shared_queue_ptr->current_msg_count, BUFFER_CAPACITY);
    printf("Total Messages Added: %zu\n", shared_queue_ptr->total_added_count);
    printf("Total Messages Extracted: %zu\n", shared_queue_ptr->total_extracted_count);
    printf("---------------------------\n");

    semaphore_signal(QUEUE_MUTEX_IDX);
}

void configure_terminal_input(int enable) {
    static struct termios original_termios_settings;
    static struct termios new_termios_settings;

    if (enable) {
        tcgetattr(STDIN_FILENO, &original_termios_settings);
        new_termios_settings = original_termios_settings;
        new_termios_settings.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios_settings);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios_settings);
    }
}

int main(void) {
    system_init();

    configure_terminal_input(1);

    printf("%s\n", command_options_text);

    while (1) {
        int input_char = getchar();

        if (input_char != EOF) {
            switch (input_char) {
                case 'P':
                    if (active_producers_count < MAX_CHILD_PROCESSES - 1) {
                        spawn_producer_process();
                    } else {
                        printf("Cannot create more producers. Max reached (%d).\n", MAX_CHILD_PROCESSES);
                    }
                    break;

                case 'p':
                    if (active_producers_count > 0) {
                        terminate_last_producer();
                    } else {
                        printf("No producers to terminate.\n");
                    }
                    break;

                case 'C':
                     if (active_consumers_count < MAX_CHILD_PROCESSES - 1) {
                        spawn_consumer_process();
                    } else {
                        printf("Cannot create more consumers. Max reached (%d).\n", MAX_CHILD_PROCESSES);
                    }
                    break;

                case 'c':
                    if (active_consumers_count > 0) {
                        terminate_last_consumer();
                    } else {
                        printf("No consumers to terminate.\n");
                    }
                    break;

                case 's':
                    display_current_status();
                    break;

                case 'q':
                    system_cleanup();
                    configure_terminal_input(0);
                    exit(EXIT_SUCCESS);

                case '\n':
                    break;

                default:
                    printf("Invalid command. %s\n", command_options_text);
                    break;
            }
        }

        usleep(700000);
    }

    return 0;
}
