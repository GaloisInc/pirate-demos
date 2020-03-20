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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef __cplusplus
#include <atomic>
typedef std::atomic_int atomic_int
#define ATOMIC_INC(PTR) std::atomic_fetch_add(PTR, 1)
#elif HAVE_STD_ATOMIC
#include <stdatomic.h>
#define ATOMIC_INC(PTR) atomic_fetch_add(PTR, 1)
#endif

#include "libpirate.h"
#include "device.h"
#include "pipe.h"
#include "unix_socket.h"
#include "tcp_socket.h"
#include "udp_socket.h"
#include "shmem_interface.h"
#include "udp_shmem_interface.h"
#include "uio.h"
#include "serial.h"
#include "mercury.h"
#include "ge_eth.h"
#include "pirate_common.h"

typedef struct {
    union {
        device_ctx         device;
        pipe_ctx           pipe;
        unix_socket_ctx    unix_socket;
        tcp_socket_ctx     tcp_socket;
        udp_socket_ctx     udp_socket;
        shmem_ctx          shmem;
        udp_shmem_ctx      udp_shmem;
        uio_ctx            uio;
        serial_ctx         serial;
        mercury_ctx        mercury;
        ge_eth_ctx         ge_eth;
    } channel;
} pirate_channel_ctx_t;

typedef struct {
    pirate_channel_param_t param;
    pirate_channel_ctx_t ctx;
} pirate_channel_t;

static struct {
    pirate_channel_t reader;
    pirate_channel_t writer;
} gaps_channels[PIRATE_NUM_CHANNELS];

int pirate_close_channel(pirate_channel_t *channel);

static inline pirate_channel_t *pirate_get_channel(int gd, int flags) {
    if ((gd < 0) || (gd >= PIRATE_NUM_CHANNELS)) {
        errno = EBADF;
        return NULL;
    }

    if (flags == O_RDONLY) {
        return &gaps_channels[gd].reader;
    } else if (flags == O_WRONLY) {
        return &gaps_channels[gd].writer;
    }

    errno = EINVAL;
    return NULL;
}


void pirate_init_channel_param(channel_enum_t channel_type, pirate_channel_param_t *param) {
    memset(param, 0, sizeof(*param));
    param->channel_type = channel_type;
}

int pirate_parse_channel_param(const char *str, pirate_channel_param_t *param) {

    // Channel configuration function is allowed to modify the string
    // while braking it into delimiter-separated tokens
    char opt[256];
    strncpy(opt, str, sizeof(opt));

    pirate_init_channel_param(INVALID, param);

    if (strncmp("device", opt, strlen("device")) == 0) {
        param->channel_type = DEVICE;
        return pirate_device_parse_param(opt, &param->channel.device);
    } else if (strncmp("pipe", opt, strlen("pipe")) == 0) {
        param->channel_type = PIPE;
        return pirate_pipe_parse_param(opt, &param->channel.pipe);
    } else if (strncmp("unix_socket", opt, strlen("unix_socket")) == 0) {
        param->channel_type = UNIX_SOCKET;
        return pirate_unix_socket_parse_param(opt, &param->channel.unix_socket);
    } else if (strncmp("tcp_socket", opt, strlen("tcp_socket")) == 0) {
        param->channel_type = TCP_SOCKET;
        return pirate_tcp_socket_parse_param(opt, &param->channel.tcp_socket);
    } else if (strncmp("udp_socket", opt, strlen("udp_socket")) == 0) {
        param->channel_type = UDP_SOCKET;
        return pirate_udp_socket_parse_param(opt, &param->channel.udp_socket);
    } else if (strncmp("shmem", opt, strlen("shmem")) == 0) {
        param->channel_type = SHMEM;
        return pirate_shmem_parse_param(opt, &param->channel.shmem);
    } else if (strncmp("udp_shmem", opt, strlen("udp_shmem")) == 0) {
        param->channel_type = UDP_SHMEM;
        return pirate_udp_shmem_parse_param(opt, &param->channel.udp_shmem);
    } else if (strncmp("uio", opt, strlen("uio")) == 0) {
        param->channel_type = UIO_DEVICE;
        return pirate_uio_parse_param(opt, &param->channel.uio);
    } else if (strncmp("serial", opt, strlen("serial")) == 0) {
        param->channel_type = SERIAL;
        return pirate_serial_parse_param(opt, &param->channel.serial);
    } else if (strncmp("mercury", opt, strlen("mercury")) == 0) {
        param->channel_type = MERCURY;
        return pirate_mercury_parse_param(opt, &param->channel.mercury);
    } else if (strncmp("ge_eth", opt, strlen("ge_eth")) == 0) {
        param->channel_type = GE_ETH;
        return pirate_ge_eth_parse_param(opt, &param->channel.ge_eth);
    }

    errno = EINVAL;
    return -1;
}

int pirate_get_channel_param(int gd, int flags,
                            pirate_channel_param_t *param) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, flags)) == NULL) {
        return -1;
    }
    memcpy(param, &channel->param, sizeof(pirate_channel_param_t));
    return 0;
}

static atomic_int next_gd;

static int pirate_next_gd() {
    int next = ATOMIC_INC(&next_gd);
    if (next >= PIRATE_NUM_CHANNELS) {
        return -1;
    }
    return next;
}

static int pirate_open(pirate_channel_t *channel, int flags) {
    pirate_channel_param_t *param = &channel->param;
    pirate_channel_ctx_t *ctx = &channel->ctx;

    if ((flags != O_RDONLY) && (flags != O_WRONLY)) {
        errno = EINVAL;
        return -1;
    }

    switch (param->channel_type) {
    case DEVICE:
        return pirate_device_open(flags, &param->channel.device, &ctx->channel.device);

    case PIPE:
        return pirate_pipe_open(flags, &param->channel.pipe, &ctx->channel.pipe);

    case UNIX_SOCKET:
        return pirate_unix_socket_open(flags, &param->channel.unix_socket, &ctx->channel.unix_socket);

    case TCP_SOCKET:
        return pirate_tcp_socket_open(flags, &param->channel.tcp_socket, &ctx->channel.tcp_socket);

    case UDP_SOCKET:
        return pirate_udp_socket_open(flags, &param->channel.udp_socket, &ctx->channel.udp_socket);

    case SHMEM:
        return pirate_shmem_open(flags, &param->channel.shmem, &ctx->channel.shmem);

    case UDP_SHMEM:
        return pirate_udp_shmem_open(flags, &param->channel.udp_shmem, &ctx->channel.udp_shmem);

    case UIO_DEVICE:
        return pirate_uio_open(flags, &param->channel.uio, &ctx->channel.uio);

    case SERIAL:
        return pirate_serial_open(flags, &param->channel.serial, &ctx->channel.serial);

    case MERCURY:
        return pirate_mercury_open(flags, &param->channel.mercury, &ctx->channel.mercury);

    case GE_ETH:
        return pirate_ge_eth_open(flags, &param->channel.ge_eth, &ctx->channel.ge_eth);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}

// gaps descriptors must be opened from smallest to largest
int pirate_open_param(pirate_channel_param_t *param, int flags) {
    pirate_channel_t channel;

    if (next_gd >= PIRATE_NUM_CHANNELS) {
        errno = EMFILE;
        return -1;
    }

    memcpy(&channel.param, param, sizeof(pirate_channel_param_t));

    if (pirate_open(&channel, flags) < 0) {
        return -1;
    }

    int gd = pirate_next_gd();
    if (gd < 0) {
        pirate_close_channel(&channel);
        errno = EMFILE;
        return -1;
    }

    pirate_channel_t *dest = pirate_get_channel(gd, flags);
    memcpy(dest, &channel, sizeof(pirate_channel_t));

    return gd;
}

int pirate_open_parse(const char *param, int flags) {
    pirate_channel_param_t vals;

    if (pirate_parse_channel_param(param, &vals) < 0) {
        return -1;
    }

    return pirate_open_param(&vals, flags);
}

static void* pirate_pipe_open_read(void *channel) {
    int rv;
    rv = pirate_open((pirate_channel_t*) channel, O_RDONLY);
    if (rv < 0) {
        return (void*) ((intptr_t) errno);
    }
    return 0;
}

static void* pirate_pipe_open_write(void *channel) {
    int rv;
    rv = pirate_open((pirate_channel_t*) channel, O_WRONLY);
    if (rv < 0) {
        return (void*) ((intptr_t) errno);
    }
    return 0;
}

int pirate_pipe_param(pirate_channel_param_t *param, int flags) {
    pthread_t read_id, write_id;
    pirate_channel_t read_channel, write_channel;
    intptr_t result;
    int rv;

    if (next_gd >= PIRATE_NUM_CHANNELS) {
        errno = EMFILE;
        return -1;
    }

    if (flags != O_RDWR) {
        errno = EINVAL;
        return -1;
    }

    memcpy(&read_channel.param, param, sizeof(pirate_channel_param_t));
    memcpy(&write_channel.param, param, sizeof(pirate_channel_param_t));

    rv = pthread_create(&read_id, NULL, pirate_pipe_open_read, (void*) &read_channel);
    if (rv != 0) {
        errno = rv;
        return -1;
    }

    rv = pthread_create(&write_id, NULL, pirate_pipe_open_write, (void*) &write_channel);
    if (rv != 0) {
        errno = rv;
        return -1;
    }

    rv = pthread_join(read_id, (void*) &result);
    if (rv != 0) {
        errno = rv;
        return -1;
    }
    if (result != 0) {
        errno = result;
        return -1;
    }

    rv = pthread_join(write_id, (void*) &result);
    if (rv != 0) {
        errno = rv;
        return -1;
    }
    if (result != 0) {
        errno = result;
        return -1;
    }

    int gd = pirate_next_gd();
    if (gd < 0) {
        pirate_close_channel(&read_channel);
        pirate_close_channel(&write_channel);
        errno = EMFILE;
        return -1;
    }

    memcpy(&gaps_channels[gd].reader, &read_channel, sizeof(pirate_channel_t));
    memcpy(&gaps_channels[gd].writer, &write_channel, sizeof(pirate_channel_t));
    return gd;
}


int pirate_close(int gd, int flags) {
    pirate_channel_t *channel;

    if ((channel = pirate_get_channel(gd, flags)) == NULL) {
        return -1;
    }
    return pirate_close_channel(channel);
}

int pirate_close_channel(pirate_channel_t *channel) {

    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (channel->param.channel_type) {

    case DEVICE:
        return pirate_device_close(&ctx->channel.device);

    case PIPE:
        return pirate_pipe_close(&ctx->channel.pipe);

    case UNIX_SOCKET:
        return pirate_unix_socket_close(&ctx->channel.unix_socket);

    case TCP_SOCKET:
        return pirate_tcp_socket_close(&ctx->channel.tcp_socket);

    case UDP_SOCKET:
        return pirate_udp_socket_close(&ctx->channel.udp_socket);

    case SHMEM:
        return pirate_shmem_close(&ctx->channel.shmem);

    case UDP_SHMEM:
        return pirate_udp_shmem_close(&ctx->channel.udp_shmem);

    case UIO_DEVICE:
        return pirate_uio_close(&ctx->channel.uio);

    case SERIAL:
        return pirate_serial_close(&ctx->channel.serial);

    case MERCURY:
        return pirate_mercury_close(&ctx->channel.mercury);

    case GE_ETH:
        return pirate_ge_eth_close(&ctx->channel.ge_eth);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}

ssize_t pirate_read(int gd, void *buf, size_t count) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, O_RDONLY)) == NULL) {
        return -1;
    }

    pirate_channel_param_t *param = &channel->param;
    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (channel->param.channel_type) {

    case DEVICE:
        return pirate_device_read(&param->channel.device, &ctx->channel.device, buf, count);

    case PIPE:
        return pirate_pipe_read(&param->channel.pipe, &ctx->channel.pipe, buf, count);

    case UNIX_SOCKET:
        return pirate_unix_socket_read(&param->channel.unix_socket, &ctx->channel.unix_socket, buf, count);

    case TCP_SOCKET:
        return pirate_tcp_socket_read(&param->channel.tcp_socket, &ctx->channel.tcp_socket, buf, count);

    case UDP_SOCKET:
        return pirate_udp_socket_read(&param->channel.udp_socket, &ctx->channel.udp_socket, buf, count);

    case SHMEM:
        return pirate_shmem_read(&param->channel.shmem, &ctx->channel.shmem, buf, count);

    case UDP_SHMEM:
        return pirate_udp_shmem_read(&param->channel.udp_shmem, &ctx->channel.udp_shmem, buf, count);

    case UIO_DEVICE:
        return pirate_uio_read(&param->channel.uio, &ctx->channel.uio, buf, count);

    case SERIAL:
        return pirate_serial_read(&param->channel.serial, &ctx->channel.serial, buf, count);

    case MERCURY:
        return pirate_mercury_read(&param->channel.mercury, &ctx->channel.mercury, buf, count);

    case GE_ETH:
        return pirate_ge_eth_read(&param->channel.ge_eth, &ctx->channel.ge_eth, buf, count);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}

ssize_t pirate_write(int gd, const void *buf, size_t count) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, O_WRONLY)) == NULL) {
        return -1;
    }

    pirate_channel_param_t *param = &channel->param;
    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (param->channel_type) {

    case DEVICE:
        return pirate_device_write(&param->channel.device, &ctx->channel.device, buf, count);

    case PIPE:
        return pirate_pipe_write(&param->channel.pipe, &ctx->channel.pipe, buf, count);

    case UNIX_SOCKET:
        return pirate_unix_socket_write(&param->channel.unix_socket, &ctx->channel.unix_socket, buf, count);

    case TCP_SOCKET:
        return pirate_tcp_socket_write(&param->channel.tcp_socket, &ctx->channel.tcp_socket, buf, count);

    case UDP_SOCKET:
        return pirate_udp_socket_write(&param->channel.udp_socket, &ctx->channel.udp_socket, buf, count);

    case SHMEM:
        return pirate_shmem_write(&param->channel.shmem, &ctx->channel.shmem, buf, count);

    case UDP_SHMEM:
        return pirate_udp_shmem_write(&param->channel.udp_shmem, &ctx->channel.udp_shmem, buf, count);

    case UIO_DEVICE:
        return pirate_uio_write(&param->channel.uio, &ctx->channel.uio, buf, count);

    case SERIAL:
        return pirate_serial_write(&param->channel.serial, &ctx->channel.serial, buf, count);

    case MERCURY:
        return pirate_mercury_write(&param->channel.mercury, &ctx->channel.mercury, buf, count);

    case GE_ETH:
        return pirate_ge_eth_write(&param->channel.ge_eth, &ctx->channel.ge_eth, buf, count);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}
