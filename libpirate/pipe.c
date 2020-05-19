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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pirate_common.h"
#include "pipe.h"

static void pirate_pipe_init_param(pirate_pipe_param_t *param) {
    if (param->min_tx == 0) {
        param->min_tx = PIRATE_DEFAULT_MIN_TX;
    }
}

int pirate_pipe_parse_param(char *str, pirate_pipe_param_t *param) {
    char *ptr = NULL, *key, *val;
    char *saveptr1, *saveptr2;

    if (((ptr = strtok_r(str, OPT_DELIM, &saveptr1)) == NULL) ||
        (strcmp(ptr, "pipe") != 0)) {
        return -1;
    }

    if ((ptr = strtok_r(NULL, OPT_DELIM, &saveptr1)) == NULL) {
        errno = EINVAL;
        return -1;
    }
    strncpy(param->path, ptr, sizeof(param->path) - 1);

    while ((ptr = strtok_r(NULL, OPT_DELIM, &saveptr1)) != NULL) {
        int rv = pirate_parse_key_value(&key, &val, ptr, &saveptr2);
        if (rv < 0) {
            return rv;
        } else if (rv == 0) {
            continue;
        }
        if (strncmp("min_tx_size", key, strlen("min_tx_size")) == 0) {
            param->min_tx = strtol(val, NULL, 10);
        } else if (strncmp("mtu", key, strlen("mtu")) == 0) {
            param->mtu = strtol(val, NULL, 10);
        } else {
            errno = EINVAL;
            return -1;
        }
    }
    return 0;
}

int pirate_pipe_get_channel_description(const pirate_pipe_param_t *param, char *desc, int len) {
    int min_tx = (param->min_tx != 0) && (param->min_tx != PIRATE_DEFAULT_MIN_TX);
    int mtu = (param->mtu != 0);
    if (mtu && min_tx) {
        return snprintf(desc, len, "pipe,%s,mtu=%u,min_tx_size=%u", param->path,
            param->mtu, param->min_tx);
    } else if (mtu) {
        return snprintf(desc, len, "pipe,%s,mtu=%u", param->path, param->mtu);
    } else if (min_tx) {
        return snprintf(desc, len, "pipe,%s,min_tx_size=%u", param->path, param->min_tx);
    } else {
        return snprintf(desc, len, "pipe,%s", param->path);
    }
}

int pirate_pipe_open(pirate_pipe_param_t *param, pipe_ctx *ctx) {
    int err;

    pirate_pipe_init_param(param);
    if (strnlen(param->path, 1) == 0) {
        errno = EINVAL;
        return -1;
    }
    err = errno;
    if (mkfifo(param->path, 0660) == -1) {
        if (errno == EEXIST) {
            errno = err;
        } else {
            return -1;
        }
    }

    if ((ctx->fd = open(param->path, ctx->flags)) < 0) {
        return -1;
    }

    if ((ctx->min_tx_buf = calloc(param->min_tx, 1)) == NULL) {
        return -1;
    }

    return 0;
}

int pirate_pipe_pipe(pirate_pipe_param_t *param, pipe_ctx *read_ctx, pipe_ctx *write_ctx) {
    (void) param;
    int rv, fd[2];

    pirate_pipe_init_param(param);
    rv = pipe(fd);
    if (rv < 0) {
        return rv;
    }

    if ((read_ctx->min_tx_buf = calloc(param->min_tx, 1)) == NULL) {
        return -1;
    }
    if ((write_ctx->min_tx_buf = calloc(param->min_tx, 1)) == NULL) {
        free(read_ctx->min_tx_buf);
        return -1;
    }

    read_ctx->fd = fd[0];
    write_ctx->fd = fd[1];
    return 0;
}

int pirate_pipe_close(pipe_ctx *ctx) {
    int rv = -1;

    if (ctx->min_tx_buf != NULL) {
        free(ctx->min_tx_buf);
        ctx->min_tx_buf = NULL;
    }

    if (ctx->fd <= 0) {
        errno = ENODEV;
        return -1;
    }

    rv = close(ctx->fd);
    ctx->fd = -1;
    return rv;
}

ssize_t pirate_pipe_read(const pirate_pipe_param_t *param, pipe_ctx *ctx, void *buf, size_t count) {
    return pirate_stream_read((common_ctx*) ctx, param->min_tx, buf, count);
}

ssize_t pirate_pipe_write(const pirate_pipe_param_t *param, pipe_ctx *ctx, const void *buf, size_t count) {
    return pirate_stream_write((common_ctx*) ctx, param->min_tx, param->mtu, buf, count);
}
