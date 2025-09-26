#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct index_s {
    double time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t records;
    struct index_s idx[];
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    rewind(fp);

    struct index_hdr_s* hdr = malloc(filesize);
    fread(hdr, filesize, 1, fp);
    fclose(fp);

    printf("Records: %lu\n", hdr->records);
    for (uint64_t i = 0; i < hdr->records; ++i) {
        printf("%12.5f  %lu\n", hdr->idx[i].time_mark, hdr->idx[i].recno);
    }

    free(hdr);
    return 0;
}