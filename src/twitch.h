#ifndef CHATHUD_TWITCH_H
#define CHATHUD_TWITCH_H
#include "chat.h"

typedef struct {
    ChatState *state;
    char channel[26];
} TwitchThreadArgs;

int twitch_extract_channel(const char *url, char *out, size_t capacity);
void *twitch_thread(void *arg);
#endif
