#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

struct index_s {
    double time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t records;
    struct index_s idx[];
};

#define MJD_MIN 15020.0
#define MJD_MAX 60781.0

double rand_mjd() {
    double int_part = (double)(rand() % ((int)(MJD_MAX - MJD_MIN))) + MJD_MIN;
    double frac_part = (double)rand() / (double)RAND_MAX;
    return int_part + frac_part;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <records>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    uint64_t records = atol(argv[2]);

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    struct index_hdr_s* hdr = malloc(sizeof(struct index_hdr_s) + sizeof(struct index_s) * records);
    hdr->records = records;

    srand(time(NULL));
    for (uint64_t i = 0; i < records; ++i) {
        hdr->idx[i].time_mark = rand_mjd();
        hdr->idx[i].recno = i + 1;
    }

    fwrite(hdr, sizeof(struct index_hdr_s) + sizeof(struct index_s) * records, 1, fp);
    fclose(fp);
    free(hdr);
    return 0;
}
