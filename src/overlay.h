#ifndef CHATHUD_OVERLAY_H
#define CHATHUD_OVERLAY_H

#include "chat.h"
#include "config.h"

#include <sys/types.h>

typedef struct {
    ChatState *state;
    const AppConfig *config;
    /* 0 = overlay działa aż do SIGTERM/SIGINT.
       >0 = overlay kończy się, gdy ten PID znika. */
    pid_t watch_pid;
} OverlayArgs;

int overlay_run(const OverlayArgs *args);

#endif
