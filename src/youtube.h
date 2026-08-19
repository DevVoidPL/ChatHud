#ifndef CHATHUD_YOUTUBE_H
#define CHATHUD_YOUTUBE_H

#include "chat.h"

typedef struct {
    ChatState *state;
    char url[2048];
    char api_key[256];
} YouTubeThreadArgs;

int youtube_extract_video_id(const char *input, char *out, size_t out_size);
void *youtube_thread(void *arg);

#endif
