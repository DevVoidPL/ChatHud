#ifndef CHATHUD_YTDLP_H
#define CHATHUD_YTDLP_H

#include "chat.h"

typedef struct {
    ChatState *state;
    char url[2048];
    int emotes;
} YtDlpThreadArgs;

int ytdlp_available(void);
void *ytdlp_thread(void *arg);

#endif
