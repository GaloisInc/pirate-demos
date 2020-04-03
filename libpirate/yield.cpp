#include "libpirate.h"
#include "libpirate_internal.h"
#include "libpirate.hpp"

#include <limits.h>
#include <poll.h>

#include <vector>

typedef struct {
    void* func;
    size_t len;
} pirate_listener_t;

typedef std::vector<pirate_listener_t> pirate_listeners_t;

pirate_listeners_t gaps_listeners[PIRATE_NUM_CHANNELS];
struct pollfd gaps_pollfd[PIRATE_NUM_CHANNELS];
nfds_t gaps_nfds = 0;

extern int gaps_reader_gds[PIRATE_NUM_CHANNELS];
extern int gaps_reader_gds_num;
extern int gaps_writer_control_gd;

int pirate::internal::cooperative_register(int gd, void* func, size_t len) {
    pirate_listener_t listener;
    listener.func = func;
    listener.len = len;
    if ((gd < 0) || (gd >= PIRATE_NUM_CHANNELS)) {
        errno = EBADF;
        return -1;
    }
    if (gaps_listeners[gd].size() > 0) {
        pirate_listener_t first = gaps_listeners[gd][0];
        if (len != first.len) {
            errno = EINVAL;
            return -1;
        }
    }
    gaps_listeners[gd].push_back(listener);
    return 0;
}

static int pirate_cooperative_listen_setup() {
    for (int i = 0; i < gaps_reader_gds_num; i++) {
        int fd = pirate_get_fd(gaps_reader_gds[i]);
        if (fd < 0) {
            return fd;
        }
        gaps_pollfd[i].fd = fd;
        gaps_pollfd[i].events = POLLIN;
    }
    gaps_nfds = gaps_reader_gds_num;
    return 0;
}

int pirate_listen() {
    int rv;
    nfds_t ready;
    pirate_channel_param_t *param;
    unsigned char stackbuf[64];

    if (gaps_nfds == 0) {
        rv = pirate_cooperative_listen_setup();
        if (rv < 0) {
            return rv;
        }
    }
    if (gaps_nfds == 0) {
        return 0;
    }
    for (;;) {
        rv = poll(gaps_pollfd, gaps_nfds, -1);
        if (rv < 0) {
            return rv;
        }
        ready = gaps_nfds;
        for (nfds_t i = 0; i < gaps_nfds; i++) {
            if (gaps_pollfd[i].revents == POLLIN) {
                ready = i;
                break;
            }
        }
        if (ready == gaps_nfds) {
            continue;
        }
        unsigned char *buf = stackbuf;
        int gd = gaps_reader_gds[ready];
        param = pirate_get_channel_param_ref(gd);
        if (param == NULL) {
            errno = EBADF;
            return -1;
        }
        if (param->control) {
            unsigned char ctrl = 0;
            // consume the control message and resume execution
            pirate_read(gd, &ctrl, sizeof(ctrl));
            return 0;
        }
        pirate_listeners_t listeners = gaps_listeners[gd];
        pirate_listener_t first = listeners[0];
        size_t count = first.len;
        if (count > sizeof(stackbuf)) {
            buf = (unsigned char*) malloc(count);
        }
        ssize_t len = pirate_read(gd, buf, count);
        if (len < 0) {
            return len;
        }
        if (((size_t) len) != count) {
            if (count > sizeof(stackbuf)) {
                free(buf);
            }
            errno = ENOMSG;
            return -1;
        }
        for (pirate_listener_t listener : listeners) {
            pirate::internal::listener_union_hack<unsigned char> lu;
            lu.ptr = listener.func;
            (*lu.listener)(*buf);
        }
        if (count > sizeof(stackbuf)) {
            free(buf);
        }
        // TODO YIELD: if (param->src_enclave == param->dst_enclave)
        if (param->pipe) {
            return 0;
        } else {
            // TODO YIELD: pirate_yield(param->src_enclave)
            pirate_yield(-1);
        }
    }
}

int pirate_yield(int enclave_id) {
    (void) enclave_id;
    unsigned char msg = 0;
    ssize_t rv = pirate_write(gaps_writer_control_gd, &msg, sizeof(msg));
    if (rv != sizeof(msg)) {
        return -1;
    }
    return 0;
}
