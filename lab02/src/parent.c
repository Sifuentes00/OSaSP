#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_CHILDREN 100

void read_env_file(const char *file_path, char **envp, char *new_env[], int max_vars) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Не удалось открыть файл окружения");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file) && count < max_vars) {
        line[strcspn(line, "\n")] = 0; 
        char *value = getenv(line);
        if (value) {
            size_t len = strlen(line) + strlen(value) + 2;
            new_env[count] = malloc(len);
            snprintf(new_env[count], len, "%s=%s", line, value);
            count++;
        }
    }
    new_env[count] = NULL;
    fclose(file);
}


void spawn_child(const char *child_path, const char *env_path, char *child_name, char *new_env[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Не удалось создать процесс");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { 
        char *argv[] = {child_name, (char *)env_path, NULL};
        if (execve(child_path, argv, new_env) < 0) {
            perror("Не удалось выполнить execve");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[], char *envp[]) {
    char *child_path = getenv("CHILD_PATH");
    if (!child_path) {
        fprintf(stderr, "CHILD_PATH не установлен в окружении\n");
        exit(EXIT_FAILURE);
    }

    char *env_file = "../../env";
    char *new_env[256];
    int child_count = 0;

    char **env;
    for (env = envp; *env != NULL; env++) {
        printf("%s\n", *env);
    }

    read_env_file(env_file, envp, new_env, 256);

    while (1) {
        printf("Введите команду (+, *, q): ");
        char command;
        scanf(" %c", &command);

        if (command == '+') {
            if (child_count >= 100) {
                fprintf(stderr, "Достигнуто максимальное количество дочерних процессов\n");
                continue;
            }

            char child_name[32]; // Имя дочернего процесса
            snprintf(child_name, sizeof(child_name), "child_%02d", child_count++);
            spawn_child(child_path, env_file, child_name, new_env);
        } else if (command == '*') {
            if (child_count >= 100) {
                fprintf(stderr, "Достигнуто максимальное количество дочерних процессов\n");
                continue;
            }

            char child_name[32]; // Имя дочернего процесса
            snprintf(child_name, sizeof(child_name), "child_%02d", child_count++);
            pid_t pid = fork();
            if (pid < 0) {
                perror("Не удалось создать процесс");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) { // Дочерний процесс
                char *argv[] = {child_name, NULL};
                if (execve(child_path, argv, new_env) < 0) {
                    perror("Не удалось выполнить execve");
                    exit(EXIT_FAILURE);
                }
            }
        } else if (command == 'q') {
            printf("Выход из родительского процесса...\n");
            while (wait(NULL) > 0) {} // Ожидание завершения всех дочерних процессов
            break;
        } else {
            printf("Неизвестная команда\n");
        }
    }
    

    for (int i = 0; new_env[i] != NULL; i++) {
        free(new_env[i]);
    }

    return 0;
}

