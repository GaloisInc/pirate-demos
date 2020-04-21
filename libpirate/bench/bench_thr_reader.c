/*
 * This work was authored by Two Six Labs, LLC and is sponsored by a subcontract
 * agreement with Galois, Inc.  This material is based upon work supported by
 * the Defense Advanced Research Projects Agency (DARPA) under Contract No.
 * HR0011-19-C-0103.
 *
 * The Government has unlimited rights to use, modify, reproduce, release,
 * perform, display, or disclose computer software or computer software
 * documentation marked with this legend. Any reproduction of technical data,
 * computer software, or portions thereof marked with this legend must also
 * reproduce this marking.
 *
 * Copyright 2020 Two Six Labs, LLC.  All rights reserved.
 */

#define _GNU_SOURCE

#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libpirate.h"

int test_gd = -1, sync_gd = -1;
uint64_t nbytes;
size_t message_len;
char message[80];
unsigned char* buffer;

int bench_thr_setup(char *argv[], int test_flags, int sync_flags);
void bench_thr_close(char *argv[]);

int run(int argc, char *argv[]) {
    unsigned char signal = 1;
    ssize_t rv;
    uint64_t readcount = 0, iter, delta;
    struct timespec start, stop;

    if (argc != 5) {
        printf("./bench_thr_reader [test channel] [sync channel] [message length] [nbytes]\n\n");
        return 1;
    }

    if (bench_thr_setup(argv, O_RDONLY, O_WRONLY)) {
        return 1;
    }

    memset(buffer, 0, nbytes);

    rv = pirate_write(sync_gd, &signal, sizeof(signal));
    if (rv < 0) {
        perror("Sync channel initial write error");
        return 1;
    }
    if (((size_t) rv) != sizeof(signal)) {
        fprintf(stderr, "Sync channel initial expected 1 byte and sent %zd bytes\n", rv);
        return 1;
    }

    rv = pirate_read(test_gd, &signal, sizeof(signal));
    if (rv < 0) {
        perror("Test channel initial read error");
        return 1;
    }
    iter = nbytes / message_len;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
      perror("clock_gettime start");
      return 1;
    }
    for (uint64_t i = 0; i < iter; i++) {
        size_t count;

        count = message_len;
        while (count > 0) {
            rv = pirate_read(test_gd, buffer + readcount, count);
            if (rv < 0) {
                perror("Test channel read error");
                return 1;
            }
            readcount += rv;
            count -= rv;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &stop) < 0) {
      perror("clock_gettime stop");
      return 1;
    }

    rv = pirate_write(sync_gd, &signal, sizeof(signal));
    if (rv < 0) {
        perror("Sync channel terminating write error");
        return 1;
    }
    if (((size_t) rv) != sizeof(signal)) {
        fprintf(stderr, "Sync channel terminating expected 1 byte and sent %zd bytes\n", rv);
        return 1;
    }

    for (uint64_t i = 0; i < nbytes; i++) {
        if (buffer[i] != (unsigned char) (i % UCHAR_MAX)) {
            fprintf(stderr, "At position %zu expected %zu and read character %d\n",
                i, (i % UCHAR_MAX), (int) buffer[i]);
            break;
        }
    }
    delta = ((stop.tv_sec - start.tv_sec) * 1000000000ll +
             (stop.tv_nsec - start.tv_nsec));
    // 1e9 nanoseconds per second
    // 1e6 bytes per megabytes
    printf("average throughput: %f MB/s\n",
           ((1e9 / 1e6) * nbytes) / delta);
    
    return 0;
}

int main(int argc, char *argv[]) {
    int rv = run(argc, argv);
    bench_thr_close(argv);
    return rv;
}
