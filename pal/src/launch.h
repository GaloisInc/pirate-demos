#ifndef _PIRATE_PAL_LAUNCH_H
#define _PIRATE_PAL_LAUNCH_H

#include <stdbool.h>
#include <sys/types.h>

#include "yaml.h"

struct app {
    char *name;
    int pipe_fd;
    bool hangup;
    pid_t pid;
};

/* Launch and enclave and save information about it in `app`.
 *
 * The path launched is the one specified in `enc`. This may be absolute
 * or relative to the directory part of `cfg_path`.
 *
 * Environment variables are inherited from envp, with any additional variables
 * in `enc` prepended to the front.
 *
 * Return 0 on success. On error, return -1 and print an error message.
 */
int launch(struct app *app, char *cfg_path, struct enclave *enc, char **envp);

#endif // _PIRATE_PAL_LAUNCH_H
