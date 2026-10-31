#ifndef CHATHUD_CONFIG_H
#define CHATHUD_CONFIG_H

#include <stddef.h>

typedef struct {
    char stream_url[2048];
    char youtube_api_key[256];
    char provider[32];
    char language[8];
    int lines;
    int width;
    int font_size;
    int header_font_size;
    int show_header;
    double opacity;
    int margin;
    int refresh_ms;
    int emotes;
    int emote_size;
    char position[32];
    char monitor[256];
    char text_align[16];
    char message_order[24];
} AppConfig;

void config_defaults(AppConfig *cfg);
void config_load(AppConfig *cfg, const char *path);
void config_default_path(char *out, size_t out_size);

#endif
