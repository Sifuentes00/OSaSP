#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void read_env_from_file(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Не удалось открыть файл окружения");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0; 
        char *value = getenv(line);
        if (value) {
            printf("%s=%s\n", line, value);
        }
    }

    fclose(file);
}

void read_env_from_array(char *envp[]) {
    for (char **env = envp; *env != NULL; env++) {
        printf("%s\n", *env);
    }
}

int main(int argc, char *argv[], char *envp[]) {
    printf("Дочерний процесс запущен\n");
    printf("Имя: %s, PID: %d, PPID: %d\n", argv[0], getpid(), getppid());

    if (argc > 1) {
        printf("Чтение окружения из файла: %s\n", argv[1]);
        read_env_from_file(argv[1]);
    } else {
        printf("Чтение окружения из массива envp:\n");
        read_env_from_array(envp);
    }

    return 0;
}

