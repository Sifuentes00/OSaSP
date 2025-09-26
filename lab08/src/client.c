#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h> // Для обработки SIGINT
#include <poll.h>   // Для poll (хотя в итоге используем select, poll здесь просто для полноты)

#define BUF_SIZE 4096

// Глобальный флаг для управления циклом клиента
volatile sig_atomic_t client_running = 1;

// Обработчик сигналов для клиента
void sig_handler(int signo) {
    if (signo == SIGINT) {
        fprintf(stderr, "\nClient received SIGINT, initiating shutdown.\n");
        client_running = 0; // Устанавливаем флаг для выхода из основного цикла
    }
}

// Теперь read_line будет возвращать malloc'ированную строку.
// Ответственность за free лежит на вызывающей стороне.
static char read_buffer[BUF_SIZE];
static int read_buffer_pos = 0;
static int read_buffer_len = 0;

char *read_line(int fd) {
    char *out = NULL;
    int current_out_len = 0;

    while (1) {
        if (read_buffer_pos >= read_buffer_len) {
            read_buffer_len = recv(fd, read_buffer, BUF_SIZE, 0);
            read_buffer_pos = 0;
            if (read_buffer_len <= 0) {
                if (out != NULL) free(out);
                return NULL;
            }
        }

        char c = read_buffer[read_buffer_pos++];

        if (c == '\n') {
            if (out == NULL) {
                out = malloc(1);
                if (out == NULL) { perror("malloc"); return NULL; }
                out[0] = '\0';
            } else {
                out = realloc(out, current_out_len + 1);
                if (out == NULL) { perror("realloc"); return NULL; }
                out[current_out_len] = '\0';
            }
            return out;
        }

        out = realloc(out, current_out_len + 2);
        if (out == NULL) {
            perror("realloc failed in read_line");
            return NULL;
        }
        out[current_out_len++] = c;
    }
}


int is_prompt(const char *s) {
    size_t L = strlen(s);
    return L > 0 && s[L-1] == '>';
}

// handle_server_line теперь возвращает:
// 0: обработана обычная строка (не prompt, не BYE)
// 1: получено BYE или EOF (нужно завершить клиента)
// 2: получен prompt (нужно показать prompt и ждать ввода)
int handle_server_line(int sock, char **prompt) {
    char *line = read_line(sock);
    if (line == NULL) { // Сервер отключился
        if (*prompt != NULL) {
            free(*prompt);
            *prompt = NULL;
        }
        return 1; // Сигнал на выход
    }

    if (is_prompt(line)) {
        if (*prompt != NULL) {
            free(*prompt);
        }
        *prompt = strdup(line); // strdup выделяет память, не забываем free
        free(line); // Освобождаем строку, полученную от read_line
        return 2; // Это prompt
    }

    printf("%s\n", line); // Это обычный ответ от сервера, печатаем его
    if (strcmp(line, "BYE") == 0) {
        free(line);
        if (*prompt != NULL) {
            free(*prompt);
            *prompt = NULL;
        }
        return 1; // Сигнал на выход
    }
    free(line); // Освобождаем строку, полученную от read_line
    return 0; // Обычная строка, продолжаем
}


void send_cmd(int sock, const char *cmd) {
    send(sock, cmd, strlen(cmd), 0);
    send(sock, "\n", 1, 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s server_host port\n", argv[0]);
        return 1;
    }
    const char *host = argv[1];
    int port = atoi(argv[2]);

    // Регистрация обработчика SIGINT для клиента
    signal(SIGINT, sig_handler);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket"); return 1;
    }

    struct hostent *he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "Unknown host\n");
        close(sock);
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);
    servaddr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr))<0) {
        perror("connect");
        close(sock);
        return 1;
    }

    char *prompt = NULL; // Инициализируем prompt
    int prompt_received = 0; // Флаг, который указывает, был ли уже получен prompt

    // Сначала читаем все, пока не получим prompt
    while(client_running) {
        int status = handle_server_line(sock, &prompt);
        if (status == 1) { // Сервер отключился или прислал BYE
            goto end_client;
        }
        if (status == 2) { // Получили prompt
            prompt_received = 1;
            break; // Выходим из этого цикла, готовы к вводу
        }
    }

    // Здесь мы гарантированно получили первый prompt от сервера
    if (prompt_received && prompt != NULL) {
        printf("%s ", prompt);
        fflush(stdout); // Важно для вывода приглашения
    } else {
        // Это может произойти, если сервер сразу отключился или не прислал prompt
        goto end_client;
    }


    char line_buffer[BUF_SIZE]; // Буфер для ввода пользователя
    while (client_running) { // Основной цикл клиента, зависит от client_running

        // Используем select для мониторинга stdin и сокета
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sock, &read_fds);

        int max_fd = (STDIN_FILENO > sock) ? STDIN_FILENO : sock;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 мс

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno == EINTR) {
                continue; // Прервано сигналом, повторяем
            }
            perror("select error");
            client_running = 0; // Фатальная ошибка
            break;
        }
        if (activity == 0) { // Таймаут, нет активности, продолжаем цикл для проверки client_running
            continue;
        }

        // Ввод с клавиатуры
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!fgets(line_buffer, sizeof(line_buffer), stdin)) {
                // EOF или ошибка на stdin (например, Ctrl+D)
                client_running = 0;
                break;
            }
            char *nl = strchr(line_buffer, '\n');
            if (nl) *nl = 0;

            if (strlen(line_buffer) == 0) {
                // Пустая строка, ничего не отправляем, просто ждем prompt снова
                if (prompt != NULL) {
                    printf("%s ", prompt);
                    fflush(stdout);
                }
                continue;
            }

            // *** ИСПРАВЛЕНИЕ: Восстановление логики чтения из файла здесь ***
            if (line_buffer[0] == '@') {
                const char *fn = line_buffer + 1;
                FILE *f = fopen(fn, "r");
                if (!f) {
                    perror("fopen");
                    // Выводим prompt снова, если файл не найден
                    if (prompt != NULL) {
                        printf("%s ", prompt);
                        fflush(stdout);
                    }
                    continue;
                }
                char file_line[BUF_SIZE];
                while (client_running && fgets(file_line, sizeof(file_line), f)) {
                    if ((nl = strchr(file_line, '\n'))) *nl = 0;
                    if (strlen(file_line) == 0) continue;

                    send_cmd(sock, file_line);
                    // После отправки команды из файла, читаем ответы от сервера
                    // Продолжаем читать ответы, пока не получим prompt
                    while(client_running) {
                        int status = handle_server_line(sock, &prompt);
                        if (status == 1) { // Сервер отключился или BYE
                            fclose(f);
                            goto end_client;
                        }
                        if (status == 2) { // Получили prompt
                            // Здесь не нужно печатать prompt, это будет сделано, как только
                            // все команды из файла будут обработаны, или основной цикл
                            // будет готов к вводу пользователя снова.
                            break; // Получили prompt, готовы к следующей команде из файла
                        }
                    }
                }
                fclose(f);
                // После обработки всех команд из файла, выводим prompt снова для ввода пользователя
                if (prompt != NULL) {
                    printf("%s ", prompt);
                    fflush(stdout);
                }
            } else { // Обычная команда (не начинающаяся с '@')
                send_cmd(sock, line_buffer);
                // После отправки команды, читаем ответы от сервера
                // Продолжаем читать ответы, пока не получим prompt
                while(client_running) {
                    int status = handle_server_line(sock, &prompt);
                    if (status == 1) { // Сервер отключился или BYE
                        goto end_client;
                    }
                    if (status == 2) { // Получили prompt
                        // Здесь мы выводим prompt, потому что мы готовы к следующему вводу пользователя
                        if (prompt != NULL) {
                            printf("%s ", prompt);
                            fflush(stdout);
                        }
                        break; // Получили prompt, ждем ввода
                    }
                }
            }
        }

        // Данные от сервера (например, если сервер отправляет что-то без запроса,
        // хотя это маловероятно с этим протоколом)
        if (FD_ISSET(sock, &read_fds)) {
            // Обрабатываем любые входящие данные от сервера.
            // handle_server_line напечатает обычный вывод и обновит prompt, если он получен.
            int status = handle_server_line(sock, &prompt);
            if (status == 1) { // Сервер отключился или BYE
                goto end_client;
            }
            // Если status равен 2 (prompt получен), мы не печатаем его здесь снова.
            // Основной цикл обработает печать prompt, когда он будет готов к вводу пользователя.
        }
    }

end_client:
    if (prompt != NULL) {
        free(prompt);
    }
    close(sock);
    fprintf(stderr, "Client exited.\n");
    return 0;
}
