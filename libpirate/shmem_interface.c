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

#include <errno.h>
#include <string.h>
#include "shmem_interface.h"
#include "shmem.h"

int pirate_shmem_parse_param(char *str, pirate_shmem_param_t *param) {
#if PIRATE_SHMEM_FEATURE
    return  shmem_buffer_parse_param(str, param);
#else
    (void) str, (void) param;
    errno = ESOCKTNOSUPPORT;
    return -1;
#endif
}

int pirate_shmem_open(int gd, int flags, pirate_shmem_param_t *param, shmem_ctx *ctx) {
#ifdef PIRATE_SHMEM_FEATURE
    return shmem_buffer_open(gd, flags, param, ctx);
#else
    (void) gd, (void) flags, (void) param, (void) ctx;
    errno = ESOCKTNOSUPPORT;
    return -1;
#endif
}

int pirate_shmem_close(shmem_ctx *ctx) {
#ifdef PIRATE_SHMEM_FEATURE
    return shmem_buffer_close(ctx);
#else
    (void) ctx;
    errno = ESOCKTNOSUPPORT;
    return -1;
#endif
}

ssize_t pirate_shmem_read(pirate_shmem_param_t *param, shmem_ctx *ctx, void *buf, size_t count) {
#ifdef PIRATE_SHMEM_FEATURE
    return shmem_buffer_read(param, ctx, buf, count);
#else
    (void) param, (void) ctx, (void) buf, (void) count;
    errno = ESOCKTNOSUPPORT;
    return -1;
#endif
}

ssize_t pirate_shmem_write(pirate_shmem_param_t *param, shmem_ctx *ctx, const void *buf,
                            size_t count) {
#ifdef PIRATE_SHMEM_FEATURE
    return shmem_buffer_write(param, ctx, buf, count);
#else
    (void) param, (void) ctx, (void) buf, (void) count;
    errno = ESOCKTNOSUPPORT;
    return -1;
#endif
}
