#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

struct index_s {
    double time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t records;
    struct index_s idx[];
};

typedef struct {
    int id;
    int threads;
    int blocks;
    size_t block_size;
    struct index_s* base;
    pthread_barrier_t* barrier;
    pthread_mutex_t* map_mutex;
    char* block_map;
    char* merge_map;
    size_t records;
} thread_arg_t;

int compare(const void* a, const void* b) {
    const struct index_s* ia = a, * ib = b;
    return (ia->time_mark > ib->time_mark) - (ia->time_mark < ib->time_mark);
}

void merge(struct index_s* dst, struct index_s* a, size_t na, struct index_s* b, size_t nb) {
    size_t i = 0, j = 0, k = 0;
    while (i < na && j < nb) {
        if (a[i].time_mark < b[j].time_mark)
            dst[k++] = a[i++];
        else
            dst[k++] = b[j++];
    }
    while (i < na) dst[k++] = a[i++];
    while (j < nb) dst[k++] = b[j++];
}

void* worker(void* arg) {
    thread_arg_t* targ = arg;
    printf("[Thread %d] started\n", targ->id);

    size_t recs_per_block = targ->records / targ->blocks;
    pthread_barrier_wait(targ->barrier);

    pthread_mutex_lock(targ->map_mutex);
    if (targ->id < targ->blocks && targ->block_map[targ->id] == 0) {
        targ->block_map[targ->id] = 1;
        pthread_mutex_unlock(targ->map_mutex);

        size_t offset = targ->id * recs_per_block;
        size_t count = ((offset + recs_per_block) > targ->records)
            ? (targ->records - offset)
            : recs_per_block;

        qsort(&targ->base[offset], count, sizeof(struct index_s), compare);
        printf("[Thread %d] sorted initial block %d\n", targ->id, targ->id);
    }
    else {
        pthread_mutex_unlock(targ->map_mutex);
    }

    while (1) {
        int block = -1;
        pthread_mutex_lock(targ->map_mutex);
        for (int i = 0; i < targ->blocks; ++i) {
            if (targ->block_map[i] == 0) {
                targ->block_map[i] = 1;
                block = i;
                break;
            }
        }
        pthread_mutex_unlock(targ->map_mutex);

        if (block == -1) break;

        size_t offset = block * recs_per_block;
        size_t count = ((offset + recs_per_block) > targ->records)
            ? (targ->records - offset)
            : recs_per_block;

        qsort(&targ->base[offset], count, sizeof(struct index_s), compare);
        printf("[Thread %d] sorted block %d\n", targ->id, block);
    }

    pthread_barrier_wait(targ->barrier);

    int step = 1;
    int step_num = 1;
    while (step < targ->blocks) {
        pthread_barrier_wait(targ->barrier);
        if (targ->id == 0) {
            memset(targ->merge_map, 0, targ->blocks);
        }
        pthread_barrier_wait(targ->barrier);

        while (1) {
            int block = -1;

            pthread_mutex_lock(targ->map_mutex);
            for (int i = 0; i + step < targ->blocks; i += step * 2) {
                if (!targ->merge_map[i]) {
                    targ->merge_map[i] = 1;
                    block = i;
                    break;
                }
            }
            pthread_mutex_unlock(targ->map_mutex);

            if (block == -1)
                break;

            size_t left = block * recs_per_block;
            size_t mid = (block + step) * recs_per_block;
            size_t right = (block + step * 2) * recs_per_block;

            if (mid > targ->records) mid = targ->records;
            if (right > targ->records) right = targ->records;

            size_t n1 = mid - left;
            size_t n2 = right - mid;

            if (n1 > 0 && n2 > 0) {
                printf("[Thread %d] merging blocks %d and %d (step %d)\n",
                    targ->id, block, block + step, step_num);

                struct index_s* tmp = malloc((n1 + n2) * sizeof(struct index_s));
                if (!tmp) {
                    fprintf(stderr, "malloc failed in merge\n");
                    exit(1);
                }

                merge(tmp, &targ->base[left], n1, &targ->base[mid], n2);
                memcpy(&targ->base[left], tmp, (n1 + n2) * sizeof(struct index_s));
                free(tmp);
            }
        }

        pthread_barrier_wait(targ->barrier);
        step_num++;
        step *= 2;
    }

    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s memsize blocks threads filename\n", argv[0]);
        return 1;
    }

    size_t memsize = atol(argv[1]);
    int blocks = atoi(argv[2]);
    int threads = atoi(argv[3]);
    const char* filename = argv[4];

    long page_size = sysconf(_SC_PAGESIZE);
    if (memsize % page_size != 0 || (blocks & (blocks - 1)) != 0 || blocks < threads * 4) {
        fprintf(stderr, "Invalid arguments.\n");
        return 1;
    }

    int fd = open(filename, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    struct stat st;
    fstat(fd, &st);

    size_t offset = 0;
    while (offset < (size_t)st.st_size) {        
        size_t size = (offset + memsize > (size_t)st.st_size) ? (st.st_size - offset) : memsize;
        void* map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
        if (map == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

        struct index_hdr_s* hdr = (struct index_hdr_s*)map;
        struct index_s* base = hdr->idx;
        size_t total = hdr->records;

        size_t needed = sizeof(struct index_hdr_s) + total * sizeof(struct index_s);
        if (needed > size) {
            fprintf(stderr,"Mapped size too small for block layout (needed %zu bytes, got %zu bytes)\n",needed, size);            
            munmap(map, size);
            close(fd);
            return 1;
        }

        printf("[Main] records = %lu\n", total);

        pthread_t tid[threads];
        thread_arg_t args[threads];
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, threads);
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        char* block_map = calloc(blocks, 1);
        char* merge_map = calloc(blocks, 1);

        for (int i = 0; i < threads; ++i) {
            args[i] = (thread_arg_t){
                .id = i,
                .threads = threads,
                .blocks = blocks,
                .block_size = memsize / blocks,
                .base = base,
                .barrier = &barrier,
                .map_mutex = &mutex,
                .block_map = block_map,
                .merge_map = merge_map,
                .records = total
            };
            pthread_create(&tid[i], NULL, worker, &args[i]);
        }

        for (int i = 0; i < threads; ++i)
            pthread_join(tid[i], NULL);

        if (threads > 1 && blocks > 1) {
            size_t recs_per_block = total / blocks;
            size_t left = 0;
            size_t mid = total / 2;
            size_t right = total;
            
            size_t n1 = mid - left;
            size_t n2 = right - mid;
            
            if (n1 > 0 && n2 > 0) {
                printf("[Main] final merge of blocks %lu to %lu and %lu to %lu\n", left, mid, mid, right);
                struct index_s* tmp = malloc((n1 + n2) * sizeof(struct index_s));
                if (!tmp) {
                    fprintf(stderr, "malloc failed in final merge\n");
                    munmap(map, size);
                    close(fd);
                    return 1;
                }
                merge(tmp, &base[left], n1, &base[mid], n2);
                memcpy(&base[left], tmp, (n1 + n2) * sizeof(struct index_s));
                    free(tmp);
            }
        }

        pthread_barrier_destroy(&barrier);
        pthread_mutex_destroy(&mutex);
        free(block_map);
        free(merge_map);
        munmap(map, size);
        offset += size;
    }

    close(fd);
    return 0;
} 
