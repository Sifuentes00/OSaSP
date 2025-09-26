#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <signal.h> // Для обработки сигналов
#include <poll.h>   // FIX: Добавлен для struct pollfd, POLLIN, poll()

#define BACKLOG 10
#define BUF_SIZE 4096

#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#define PATHBUF (PATH_MAX + NAME_MAX + 2)

// Глобальный флаг для управления циклом сервера
volatile sig_atomic_t server_running = 1;
// Мьютекс и условная переменная для отслеживания активных потоков
pthread_mutex_t active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t active_threads_cond = PTHREAD_COND_INITIALIZER;
int active_threads_count = 0;

typedef struct {
    int client_fd;
    char root[PATH_MAX];
} client_args_t;

void log_event(const char *fmt, ...) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y.%m.%d-%H:%M:%S", &tm);
    int ms = tv.tv_usec / 1000;
    fprintf(stdout, "%s.%03d ", ts, ms);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

// Обработчик сигналов (Ctrl+C, SIGTERM)
void sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        log_event("Received signal %d, initiating shutdown.", signo);
        server_running = 0; // Устанавливаем флаг для выхода из основного цикла
    }
}

static char *trim(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int build_path(const char *root, const char *cwd, const char *target, char *out) {
    char tmp[PATHBUF];
    if (target[0] == '/') {
        snprintf(tmp, sizeof(tmp), "%s%s", root, target);
    } else {
        snprintf(tmp, sizeof(tmp), "%s/%s/%s", root, cwd, target);
    }
    char realp[PATH_MAX];
    if (!realpath(tmp, realp)) return -1;
    if (strncmp(realp, root, strlen(root)) != 0) return -1;
    strcpy(out, realp);
    return 0;
}
void sendall(int fd, const char *s) {
    size_t len = strlen(s), sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, s + sent, len - sent, 0);
        if (n <= 0) return;
        sent += n;
    }
}

void handle_echo(int fd, const char *arg) {
    sendall(fd, arg);
    sendall(fd, "\n");
}

void handle_info(int fd) {
    sendall(fd, "Hello from 'myserver'\n");
}

void handle_list(int fd, const char *root, const char *cwd) {
    char path[PATHBUF];
    if (build_path(root, cwd, ".", path) < 0) {
        sendall(fd, "Error: Cannot access current directory.\n");
        return;
    }
    DIR *d = opendir(path);
    if (!d) {
        perror("opendir");
        sendall(fd, "Error: Cannot open directory.\n");
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
        char full[PATHBUF];
        int written = snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        if (written < 0 || written >= (int)sizeof(full)) {
            continue;
        }

        struct stat st;
        if (lstat(full, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) {
            sendall(fd, ent->d_name);
            sendall(fd, "/\n");
        } else if (S_ISLNK(st.st_mode)) {
            char linkto[PATHBUF];
            ssize_t r = readlink(full, linkto, sizeof(linkto)-1);
            if (r < 0) continue;
            linkto[r] = '\0';
            char target[PATHBUF];
            if (build_path(root, cwd, linkto, target) == 0) {
                struct stat st2;
                if (lstat(target, &st2) == 0 && S_ISLNK(st2.st_mode)) {
                    char real2[PATHBUF];
                    ssize_t r2 = readlink(target, real2, sizeof(real2)-1);
                    if (r2 > 0) {
                        real2[r2] = '\0';
                        sendall(fd, ent->d_name);
                        sendall(fd, " -->> ");
                        sendall(fd, real2);
                        sendall(fd, "\n");
                    }
                } else {
                    sendall(fd, ent->d_name);
                    sendall(fd, " --> ");
                    sendall(fd, linkto);
                    sendall(fd, "\n");
                }
            }
        } else {
            sendall(fd, ent->d_name);
            sendall(fd, "\n");
        }
    }
    closedir(d);
}

void *client_thread(void *arg) {
    // Увеличиваем счетчик активных потоков
    pthread_mutex_lock(&active_threads_mutex);
    active_threads_count++;
    pthread_mutex_unlock(&active_threads_mutex);

    client_args_t *ca = arg;
    int fd = ca->client_fd;
    char cwd[PATH_MAX] = "";

    handle_info(fd);
    sendall(fd, ">\n");
    log_event("Client %d connected, greeting sent", fd);

    char buf[BUF_SIZE];
    while (server_running) { // Теперь цикл зависит от server_running
        // Используем poll с таймаутом, чтобы recv не блокировал вечно
        // и поток мог проверить server_running.
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN; // Интересуют события чтения

        int poll_ret = poll(&pfd, 1, 100); // Таймаут 100 мс

        if (poll_ret < 0) {
            if (errno == EINTR) { // Прервано сигналом
                continue;
            }
            perror("poll error");
            break; // Другая ошибка poll
        }
        if (poll_ret == 0) { // Таймаут, данных нет
            continue; // Проверяем server_running снова
        }

        // Есть данные для чтения
        ssize_t n = recv(fd, buf, sizeof(buf)-1, 0);
        if (n <= 0) {
            if (n == 0) {
                log_event("Client %d disconnected (gracefully)", fd);
            } else {
                log_event("Client %d disconnected (error: %s)", fd, strerror(errno));
            }
            break;
        }
        buf[n] = '\0';
        char *nl = strchr(buf, '\n'); if (nl) *nl = '\0';
        char *cmd = trim(buf);
        if (*cmd == '\0') {
            if (cwd[0] == '\0') sendall(fd, ">\n");
            else { sendall(fd, cwd); sendall(fd, ">\n"); }
            continue;
        }
        log_event("Client %d sent: %s", fd, cmd);

        if (strncasecmp(cmd, "ECHO ", 5) == 0) {
            handle_echo(fd, cmd + 5);
        } else if (strcasecmp(cmd, "QUIT") == 0) {
            sendall(fd, "BYE\n");
            log_event("Client %d disconnected (QUIT command)", fd);
            break;
        } else if (strcasecmp(cmd, "INFO") == 0) {
            handle_info(fd);
        } else if (strncasecmp(cmd, "CD ", 3) == 0) {
            const char *t = cmd + 3;
            if (strcasecmp(t, "/") == 0) {
                cwd[0] = '\0';
            } else if (strcasecmp(t, "..") == 0) {
                char *p = strrchr(cwd, '/'); if (p) *p = '\0'; else cwd[0] = '\0';
            } else {
                char newp[PATHBUF];
                if (build_path(ca->root, cwd, t, newp) == 0) {
                    const char *rel = newp + strlen(ca->root);
                    if (rel[0] == '/') rel++;
                    size_t rlen = strlen(rel);
                    if (rlen < sizeof(cwd)) {
                        memcpy(cwd, rel, rlen);
                        cwd[rlen] = '\0';
                    } else {
                        sendall(fd, "Error: Path too long to set as current directory.\n");
                    }
                } else {
                    sendall(fd, "Error: Invalid path or permission denied for CD.\n");
                }
            }
        } else if (strcasecmp(cmd, "LIST") == 0) {
            handle_list(fd, ca->root, cwd);
        } else {
            sendall(fd, "Unknown command\n");
        }
        if (cwd[0] == '\0') sendall(fd, ">\n");
        else {
            sendall(fd, cwd); sendall(fd, ">\n");
        }
    }
    close(fd);
    free(ca);

    // Уменьшаем счетчик активных потоков и сигнализируем main
    pthread_mutex_lock(&active_threads_mutex);
    active_threads_count--;
    pthread_cond_signal(&active_threads_cond); // Сообщаем main, что поток завершился
    pthread_mutex_unlock(&active_threads_mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s root_dir port\n", argv[0]);
        return 1;
    }
    char root[PATH_MAX];
    if (!realpath(argv[1], root)) {
        perror("realpath for root_dir");
        return 1;
    }
    struct stat st_root;
    if (stat(root, &st_root) < 0 || !S_ISDIR(st_root.st_mode)) {
        fprintf(stderr, "Error: root_dir '%s' is not a valid directory or inaccessible.\n", argv[1]);
        return 1;
    }

    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number: %d\n", port);
        return 1;
    }

    // Регистрация обработчиков сигналов
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket"); return 1;
    }
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }
    log_event("Server started port=%d, root='%s'", port, root);
    log_event("Enter 'Q' to quit the server.");

    // Для чтения из stdin
    int stdin_fd = fileno(stdin);
    // Для select, max_fd должен быть наибольшим дескриптором + 1
    int max_fd = listen_fd;
    if (stdin_fd > max_fd) max_fd = stdin_fd;

    fd_set read_fds;
    struct timeval timeout;

    while (server_running) {
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        FD_SET(stdin_fd, &read_fds); // Добавляем stdin в набор для select

        timeout.tv_sec = 1; // Проверять каждые 1 секунду
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno == EINTR) { // Прервано сигналом
                continue; // Повторяем select
            }
            perror("select error");
            server_running = 0; // Фатальная ошибка select, выходим
            break;
        }

        // Если пришло новое соединение
        if (FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr_in cli;
            socklen_t len = sizeof(cli);
            int fd = accept(listen_fd, (struct sockaddr*)&cli, &len);
            if (fd < 0) {
                perror("accept");
                // Если accept fail, но server_running уже false, то это нормально при завершении
                if (!server_running) break;
                continue;
            }
            client_args_t *ca = malloc(sizeof(*ca));
            if (ca == NULL) {
                perror("malloc for client_args_t");
                close(fd);
                continue;
            }
            strncpy(ca->root, root, PATH_MAX - 1);
            ca->root[PATH_MAX - 1] = '\0';
            ca->client_fd = fd; // FIX: client_fd должен быть присвоен после strncpy, но до pthread_create

            pthread_t tid;
            int rc = pthread_create(&tid, NULL, client_thread, ca);
            if (rc != 0) {
                fprintf(stderr, "Error creating thread: %s\n", strerror(rc));
                close(fd);
                free(ca);
            } else {
                pthread_detach(tid);
            }
        }

        // Если пришел ввод с консоли (stdin)
        if (FD_ISSET(stdin_fd, &read_fds)) {
            char cmd_line[256];
            if (fgets(cmd_line, sizeof(cmd_line), stdin)) {
                char *nl = strchr(cmd_line, '\n');
                if (nl) *nl = '\0';
                char *cmd = trim(cmd_line);
                if (strcasecmp(cmd, "Q") == 0) {
                    log_event("Server received 'Q' command, initiating shutdown.");
                    server_running = 0; // Устанавливаем флаг завершения
                } else {
                    log_event("Unknown server command: '%s'", cmd);
                }
            } else {
                // stdin закрыт или произошла ошибка (например, EOF)
                log_event("stdin closed or error, initiating shutdown.");
                server_running = 0;
            }
        }
    }

    // Начало процедуры чистого завершения
    log_event("Server shutting down, closing listening socket...");
    close(listen_fd); // Закрываем слушающий сокет

    // Ожидаем завершения всех активных клиентских потоков
    log_event("Waiting for active client threads to finish...");
    pthread_mutex_lock(&active_threads_mutex);
    while (active_threads_count > 0) {
        log_event("Active threads: %d. Waiting...", active_threads_count);
        // pthread_cond_wait может быть прерван сигналом.
        // Если server_running уже 0, мы можем выйти из ожидания.
        // Используем timedwait, чтобы не блокироваться вечно, если что-то пойдет не так.
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5; // Ждем максимум 5 секунд
        int ret = pthread_cond_timedwait(&active_threads_cond, &active_threads_mutex, &ts);
        if (ret == ETIMEDOUT) {
            log_event("Timed out waiting for threads. Forcing shutdown.");
            break; // Выходим из цикла ожидания, если таймаут
        }
        if (!server_running && active_threads_count > 0) {
            // Если сервер уже не работает, но потоки все еще есть,
            // это может быть из-за таймаута или других проблем.
            // Можно выйти, чтобы не зависнуть.
            break;
        }
    }
    pthread_mutex_unlock(&active_threads_mutex);
    log_event("All client threads finished. Server gracefully stopped.");

    return 0;
}
