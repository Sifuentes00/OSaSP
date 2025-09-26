#define _GNU_SOURCE
#define CAPACITY 8

#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct process_info
{
    pid_t pid;
    char name[CAPACITY * 2];
} process_info;

size_t child_processes_size = 0;
size_t child_processes_capacity = CAPACITY;
process_info *child_processes = NULL;
char child_name[CAPACITY] = "./child";

int input_option(char *option, size_t *option_index);
int str_to_int(char* str);
void allocate_child_processes();
void init_child_process(pid_t pid, size_t index);
void case_plus();
void case_minus();
void case_l();
void delete_all_children();
void complete_the_program();
void delete_child_process_by_index(size_t index);
void delete_all_child_processes();
void init_signals_handling();
void signal_handler(int signo, siginfo_t *info, void *context);
char *find_process_name_by_pid(pid_t pid);
int find_process_index_by_pid(pid_t pid);
void print_menu();
void cleanup_child_processes();


int main(int argc, char *argv[])
{
    init_signals_handling();
    allocate_child_processes();
    print_menu();

    while(true)
    {
        char option = 0;
        size_t option_index = (size_t)-1;

        printf("\nEnter command (m for menu): ");
        fflush(stdout);

        if(input_option(&option, &option_index) == -1) {
            fprintf(stderr, "Invalid command or index. Try again.\n");
            continue;
        }

        switch (option)
        {
            case 'm':
                print_menu();
                break;
            case '+':
                case_plus();
                break;
            case '-':
                case_minus();
                break;
            case 'l':
                case_l();
                break;
            case 'k':
                delete_all_children();
                break;
            case 'q':
                complete_the_program();
                break;
            default:
                fprintf(stderr, "Unknown command '%c'.\n", option);
                break;
        }
        int status;
        pid_t terminated_pid;
        while ((terminated_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            int idx = find_process_index_by_pid(terminated_pid);
            if (idx != -1) {
                printf("Child %s (PID %d) terminated unexpectedly or was waited for.\n", child_processes[idx].name, terminated_pid);
                 for (size_t j = idx; j < child_processes_size - 1; j++) {
                      child_processes[j] = child_processes[j + 1];
                 }
                 child_processes_size--;
            }
        }
    }
    cleanup_child_processes();
    return 0;
}

void print_menu()
{
    printf("\n--- Process Manager Menu ---\n");
    printf("  + : Create a new child process\n");
    printf("  - : Delete the last created child process\n");
    printf("  l : List all child processes\n");
    printf("  k : Kill (delete) all child processes\n");
    printf("  m : Show this menu\n");
    printf("  q : Quit the program (kills all children)\n");
    printf("----------------------------\n");
}

void init_signals_handling()
{
    struct sigaction action = {0};
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGCHLD);

    action.sa_flags = SA_SIGINFO | SA_RESTART;
    action.sa_mask = set;
    action.sa_sigaction = signal_handler;

    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        perror("sigaction SIGUSR1 failed");
        exit(errno);
    }

    if (sigaction(SIGUSR2, &action, NULL) == -1) {
        perror("sigaction SIGUSR2 failed");
        exit(errno);
    }

    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        perror("sigaction SIGCHLD failed");
        exit(errno);
    }
}

void signal_handler(int signo, siginfo_t *info, void *context)
{
    pid_t child_pid = info->si_pid;

    if (signo == SIGUSR1)
    {
        char *name = find_process_name_by_pid(child_pid);
        if (name) {
             printf("INFO: Child %s (PID %d) reporting stats.\n", name, child_pid);
        } else {
             printf("INFO: Received SIGUSR1 from unknown child PID %d.\n", child_pid);
        }
         fflush(stdout);
    }
    else if (signo == SIGUSR2)
    {
        char *name = find_process_name_by_pid(child_pid);
        if (name) {
             printf("INFO: Child %s (PID %d) finished reporting stats.\n", name, child_pid);
        } else {
             printf("INFO: Received SIGUSR2 from unknown child PID %d.\n", child_pid);
        }
         fflush(stdout);
    }
     else if (signo == SIGCHLD)
     {
         int status;
         pid_t terminated_pid;

         while ((terminated_pid = waitpid(-1, &status, WNOHANG)) > 0) {
              int idx = find_process_index_by_pid(terminated_pid);
              if (idx != -1) {
                  printf("INFO: Child %s (PID %d) terminated ", child_processes[idx].name, terminated_pid);
                  if (WIFEXITED(status)) {
                      printf("with status %d.\n", WEXITSTATUS(status));
                  } else if (WIFSIGNALED(status)) {
                      printf("by signal %d.\n", WTERMSIG(status));
                  } else {
                      printf("(unknown status).\n");
                  }
                  fflush(stdout);

                  for (size_t j = idx; j < child_processes_size - 1; j++) {
                      child_processes[j] = child_processes[j + 1];
                  }
                  child_processes_size--;

              } else {
                  printf("INFO: Reaped unknown child PID %d.\n", terminated_pid);
                  fflush(stdout);
              }
         }
         if (terminated_pid == -1 && errno != ECHILD) {
         }
     }
}


int find_process_index_by_pid(pid_t pid)
{
    for (size_t i = 0; i < child_processes_size; i++)
    {
        if (child_processes[i].pid == pid) return (int)i;
    }
    return -1;
}

char *find_process_name_by_pid(pid_t pid)
{
    int index = find_process_index_by_pid(pid);
    if (index != -1) {
        return child_processes[index].name;
    }
    return NULL;
}

void allocate_child_processes()
{
    child_processes = (process_info *)calloc(child_processes_capacity, sizeof(process_info));
    if (!child_processes)
    {
        perror("Failed to allocate memory for child processes");
        exit(errno);
    }
}

void init_child_process(pid_t pid, size_t index)
{
    snprintf(child_processes[index].name, sizeof(child_processes[index].name), "C_%02zu", index);
    child_processes[index].pid = pid;
}


void case_plus()
{
    if (child_processes_size >= child_processes_capacity)
    {
        size_t new_capacity = child_processes_capacity * 2;
        process_info *tmp = (process_info *)realloc(child_processes, new_capacity * sizeof(process_info));
        if (!tmp)
        {
            perror("Failed to reallocate memory for child processes");
            return;
        }
        child_processes = tmp;
        child_processes_capacity = new_capacity;
        printf("Increased child process array capacity to %zu.\n", new_capacity);
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
    }
    else if (pid == 0)
    {
        execl(child_name, child_name, (char *)NULL);
        perror("execl failed");
        exit(errno);
    }
    else
    {
        init_child_process(pid, child_processes_size);
        printf("Created child %s with PID %d.\n", child_processes[child_processes_size].name, pid);
        child_processes_size++;
        printf("Total child processes: %zu\n", child_processes_size);
    }
}

void case_minus()
{
    if (child_processes_size == 0)
    {
        printf("No child processes to delete.\n");
        return;
    }
    delete_child_process_by_index(child_processes_size - 1);
    printf("Total child processes remaining: %zu\n", child_processes_size);
}


void delete_child_process_by_index(size_t index) {
     if (index >= child_processes_size) {
         fprintf(stderr, "Error: Invalid index %zu for deletion.\n", index);
         return;
     }

     pid_t pid_to_delete = child_processes[index].pid;
     char name_buffer[CAPACITY * 2];
     strncpy(name_buffer, child_processes[index].name, sizeof(name_buffer) -1);
     name_buffer[sizeof(name_buffer) - 1] = '\0';

     printf("Sending SIGTERM to child %s (PID %d)...\n", name_buffer, pid_to_delete);
     if (kill(pid_to_delete, SIGTERM) == -1) {
         if (errno == ESRCH) {
             printf("Child %s (PID %d) already terminated.\n", name_buffer, pid_to_delete);
         } else {
             perror("Failed to send SIGTERM");
         }
     } else {
         printf("SIGTERM sent to %s (PID %d).\n", name_buffer, pid_to_delete);
     }

     for (size_t i = index; i < child_processes_size - 1; i++) {
         child_processes[i] = child_processes[i + 1];
     }
     child_processes_size--;
}

void case_l()
{
    printf("\n--- Process List ---\n");
    printf("Parent PID: %d\n", (int) getpid());
    if (child_processes_size == 0)
    {
        printf("No child processes running.\n");
    } else {
        printf("Children (%zu):\n", child_processes_size);
        for (size_t i = 0; i < child_processes_size; i++)
        {
            printf("  [%zu] %s (PID: %d)\n", i, child_processes[i].name, child_processes[i].pid);
        }
    }
     printf("--------------------\n");
}

void delete_all_children()
{
    if (child_processes_size == 0)
    {
        printf("No child processes to delete.\n");
        return;
    }
    printf("Deleting all %zu child processes...\n", child_processes_size);
    while (child_processes_size > 0) {
         delete_child_process_by_index(child_processes_size - 1);
    }
    printf("Sent termination signals to all children.\n");
}

void complete_the_program()
{
    printf("Exiting program...\n");
    delete_all_children();
    printf("Waiting briefly for children to terminate...\n");
    sleep(1);

    for(size_t i=0; i < child_processes_size; ++i) {
         if (kill(child_processes[i].pid, 0) == 0 || errno == EPERM) {
            printf("Sending SIGKILL to remaining child %s (PID %d)\n", child_processes[i].name, child_processes[i].pid);
            kill(child_processes[i].pid, SIGKILL);
         }
    }
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);

    cleanup_child_processes();
    printf("Exiting parent process.\n");
    exit(0);
}

void cleanup_child_processes() {
    if (child_processes) {
        free(child_processes);
        child_processes = NULL;
        child_processes_size = 0;
        child_processes_capacity = 0;
    }
}


int input_option(char *option, size_t *option_index)
{
    char buf[32];
    *option_index = (size_t)-1;

    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        if (feof(stdin)) {
            *option = 'q';
            return 0;
        }
        perror("fgets error");
        return -1;
    }

    buf[strcspn(buf, "\n")] = 0;

    if (strlen(buf) == 1) {
        char cmd = buf[0];
        if (strchr("+-lmkq", cmd)) {
            *option = cmd;
            return 0;
        } else {
            fprintf(stderr,"Invalid command: %s\n", buf);
            return -1;
        }
    }

    fprintf(stderr,"Invalid command format: %s\n", buf);
    return -1;
}

int str_to_int(char *str)
{
    char num_str[16];
    size_t num_idx = 0; // Changed from int to size_t
    bool digit_found = false;

    for (size_t i = 0; str[i] != '\0' && num_idx < sizeof(num_str) - 1; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            num_str[num_idx++] = str[i];
            digit_found = true;
        } else if (digit_found) {
            break;
        }
    }

    if (num_idx > 0) {
        num_str[num_idx] = '\0';
        return atoi(num_str);
    }

    return -1;
}
