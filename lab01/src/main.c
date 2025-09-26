/*
 * Программа dirwalk
 *
 * usage: dirwalk [options] [dir] [options]
 *
 * Опции:
 *   -l     вывод только символических ссылок
 *   -d     вывод только каталогов
 *   -f     вывод только обычных файлов
 *   -s     сортировать выход в соответствии с LC_COLLATE
 *
 * Если опции l, d и f опущены, выводятся все типы (каталоги, файлы и ссылки).
 *
 * Формат вывода аналогичен утилите find: выводятся пути к найденным файлам и каталогам.
 *
 * Программа переносима (используются только возможности POSIX).
 */

#define _XOPEN_SOURCE 700  // Для nftw или других POSIX функций (если потребуется)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <locale.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} PathList;

void add_path(PathList *pl, const char *path) {
    if (pl->count == pl->capacity) {
        size_t newCapacity = (pl->capacity == 0) ? 16 : pl->capacity * 2;
        char **tmp = realloc(pl->items, newCapacity * sizeof(char*));
        if (!tmp) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        pl->items = tmp;
        pl->capacity = newCapacity;
    }
    pl->items[pl->count] = strdup(path);
    if (!pl->items[pl->count]) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    pl->count++;
}

void free_pathlist(PathList *pl) {
    for (size_t i = 0; i < pl->count; i++) {
        free(pl->items[i]);
    }
    free(pl->items);
}

int cmp_strcoll(const void *a, const void *b) {
    const char * const *pa = a;
    const char * const *pb = b;
    return strcoll(*pa, *pb);
}

typedef struct {
    int l_only;
    int d_only;
    int f_only;
    int sort;
} Options;

int is_match(const char *path, const struct stat *sb, const Options *opts) {
    int anyFilter = opts->l_only || opts->d_only || opts->f_only;

    if (opts->l_only) { 
        if (S_ISLNK(sb->st_mode))
            return 1;
    }
    if (opts->d_only) {
        if (S_ISDIR(sb->st_mode))
            return 1;
    }
    if (opts->f_only) {
        if (S_ISREG(sb->st_mode))
            return 1;
    }

    if (!anyFilter) {
        return 1;
    }

    return 0;
}

void traverse_dir(const char *basepath, const Options *opts, PathList *plist) {
    DIR *dir = opendir(basepath);
    if (!dir) {
        fprintf(stderr, "Ошибка при открытии каталога '%s': %s\n", basepath, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Пропускаем "." и ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", basepath, entry->d_name);

        struct stat sb;
        if (lstat(path, &sb) < 0) {
            fprintf(stderr, "Ошибка при lstat '%s': %s\n", path, strerror(errno));
            continue;
        }

        if (is_match(path, &sb, opts)) {
            add_path(plist, path);
        }

        if (S_ISDIR(sb.st_mode)) {
            traverse_dir(path, opts, plist);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    
    setlocale(LC_COLLATE, "");

    Options opts = {0, 0, 0, 0};

    int opt;
    opterr = 0;
    while ((opt = getopt(argc, argv, "ldfs")) != -1) {
        switch(opt) {
            case 'l':
                opts.l_only = 1;
                break;
            case 'd':
                opts.d_only = 1;
                break;
            case 'f':
                opts.f_only = 1;
                break;
            case 's':
                opts.sort = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [options] [dir]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Определяем начальный каталог: если остался аргумент, берем его, иначе "./"
    char *start_dir = "./";
    if (optind < argc) {
        start_dir = argv[optind];
    }

    PathList plist = {0};
    plist.items = NULL;
    plist.count = 0;
    plist.capacity = 0;

    // Если начальный каталог удовлетворяет фильтру, добавить его в список
    struct stat sb;
    if (lstat(start_dir, &sb) < 0) {
        fprintf(stderr, "Ошибка при lstat '%s': %s\n", start_dir, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (is_match(start_dir, &sb, &opts)) {
        add_path(&plist, start_dir);
    }

   
    if (S_ISDIR(sb.st_mode)) {
        traverse_dir(start_dir, &opts, &plist);
    }

    if (opts.sort) {
        qsort(plist.items, plist.count, sizeof(char *), cmp_strcoll);
    }

    for (size_t i = 0; i < plist.count; i++) {
        printf("%s\n", plist.items[i]);
    }

    free_pathlist(&plist);

    return 0;
}
