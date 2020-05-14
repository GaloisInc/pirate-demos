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
 * Copyright 2019 Two Six Labs, LLC.  All rights reserved.
 */

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "pirate_common.h"
#include "tcp_socket.h"

static void pirate_tcp_socket_init_param(pirate_tcp_socket_param_t *param) {
    if (strnlen(param->addr, 1) == 0) {
        snprintf(param->addr, sizeof(param->addr) - 1, DEFAULT_TCP_IP_ADDR);
    }
    if (param->min_tx == 0) {
        param->min_tx = PIRATE_DEFAULT_MIN_TX;
    }
}

int pirate_tcp_socket_parse_param(char *str, pirate_tcp_socket_param_t *param) {
    char *ptr = NULL, *key, *val;
    char *saveptr1, *saveptr2;

    if (((ptr = strtok_r(str, OPT_DELIM, &saveptr1)) == NULL) ||
        (strcmp(ptr, "tcp_socket") != 0)) {
        return -1;
    }

    if ((ptr = strtok_r(NULL, OPT_DELIM, &saveptr1)) == NULL) {
        errno = EINVAL;
        return -1;
    }
    strncpy(param->addr, ptr, sizeof(param->addr) - 1);

    if ((ptr = strtok_r(NULL, OPT_DELIM, &saveptr1)) == NULL) {
        errno = EINVAL;
        return -1;
    }
    param->port = strtol(ptr, NULL, 10);

    while ((ptr = strtok_r(NULL, OPT_DELIM, &saveptr1)) != NULL) {
        int rv = pirate_parse_key_value(&key, &val, ptr, &saveptr2);
        if (rv < 0) {
            return rv;
        } else if (rv == 0) {
            continue;
        }
        if (strncmp("buffer_size", key, strlen("buffer_size")) == 0) {
            param->buffer_size = strtol(val, NULL, 10);
        } else {
            errno = EINVAL;
            return -1;
        }
    }
    return 0;
}

int pirate_tcp_socket_get_channel_description(const pirate_tcp_socket_param_t *param, char *desc, int len) {
    return snprintf(desc, len - 1, "tcp_socket,%s,%u,buffer_size=%u", param->addr,
                    param->port, param->buffer_size);
}

static int tcp_socket_reader_open(pirate_tcp_socket_param_t *param, tcp_socket_ctx *ctx) {
    int err, rv;
    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return server_fd;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(param->addr);
    addr.sin_port = htons(param->port);

    int enable = 1;
    rv = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (rv < 0) {
        err = errno;
        close(server_fd);
        errno = err;
        return rv;
    }

    if (param->buffer_size > 0) {
        rv = setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, &param->buffer_size,
                        sizeof(param->buffer_size));
        if (rv < 0) {
            err = errno;
            close(server_fd);
            errno = err;
            return rv;
        }
    }

    rv = bind(server_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (rv < 0) {
        err = errno;
        close(server_fd);
        errno = err;
        return rv;
    }

    rv = listen(server_fd, 0);
    if (rv < 0) {
        err = errno;
        close(server_fd);
        errno = err;
        return rv;
    }

    ctx->sock = accept(server_fd, NULL, NULL);

    if (ctx->sock < 0) {
        err = errno;
        close(server_fd);
        errno = err;
        return ctx->sock;
    }

    close(server_fd);
    return 0;
}

static int tcp_socket_writer_open(pirate_tcp_socket_param_t *param, tcp_socket_ctx *ctx) {
    int err, rv;
    struct sockaddr_in addr;

    ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sock < 0) {
        return ctx->sock;
    }

    if (param->buffer_size > 0) {
        rv = setsockopt(ctx->sock, SOL_SOCKET, SO_SNDBUF, &param->buffer_size,
                    sizeof(param->buffer_size));
        if (rv < 0) {
            err = errno;
            close(ctx->sock);
            ctx->sock = -1;
            errno = err;
            return rv;
        }
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(param->addr);
    addr.sin_port = htons(param->port);

    for (;;) {
        err = errno;
        rv = connect(ctx->sock, (struct sockaddr *)&addr, sizeof(addr));
        if (rv < 0) {
            if ((errno == ENOENT) || (errno == ECONNREFUSED)) {
                struct timespec req;
                errno = err;
                req.tv_sec = 0;
                req.tv_nsec = 1e8;
                rv = nanosleep(&req, NULL);
                if (rv == 0) {
                    continue;
                }
            }
            err = errno;
            close(ctx->sock);
            ctx->sock = -1;
            errno = err;
            return rv;
        }

        return 0;
    }

    return -1;
}

int pirate_tcp_socket_open(pirate_tcp_socket_param_t *param, tcp_socket_ctx *ctx) {
    int rv = -1;
    int access = ctx->flags & O_ACCMODE;

    pirate_tcp_socket_init_param(param);
    if (param->port <= 0) {
        errno = EINVAL;
        return -1;
    }
    if ((ctx->min_tx_buf = calloc(param->min_tx, 1)) == NULL) {
        return -1;
    }
    if (access == O_RDONLY) {
        rv = tcp_socket_reader_open(param, ctx);
    } else {
        rv = tcp_socket_writer_open(param, ctx);
    }

    return rv;
}

int pirate_tcp_socket_close(tcp_socket_ctx *ctx) {
    int rv = -1;

    if (ctx->min_tx_buf != NULL) {
        free(ctx->min_tx_buf);
        ctx->min_tx_buf = NULL;
    }
    if (ctx->sock <= 0) {
        errno = ENODEV;
        return -1;
    }

    rv = close(ctx->sock);
    ctx->sock = -1;
    return rv;
}

ssize_t pirate_tcp_socket_read(const pirate_tcp_socket_param_t *param, tcp_socket_ctx *ctx, void *buf, size_t count) {
    return pirate_stream_read((common_ctx*) ctx, param->min_tx, buf, count);
}

ssize_t pirate_tcp_socket_write(const pirate_tcp_socket_param_t *param, tcp_socket_ctx *ctx, const void *buf, size_t count) {
    return pirate_stream_write((common_ctx*) ctx, param->min_tx, buf, count);
}
