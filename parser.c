#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void error(const char* msg) {
    fprintf(stderr, "error\n");
    exit(1);
}

int main(int argc, char **argv) {
    struct stat stat_buf;
    if (argc != 2) {
        error("error!\n");
    }

    if (stat(*(argv + 1), &stat_buf) == 0) {
        unsigned long long fs = stat_buf.st_size;
        fprintf(stderr, "file size: %lld\n", fs);
    } else {
        error("file size error!\n");
    }

    return 0;
}
