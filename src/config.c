#include "config.h"
#include "chat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static int parse_bool(const char *value, int fallback) {
    if (!value) return fallback;
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) return 1;
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0) return 0;
    return fallback;
}

void config_defaults(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->provider, sizeof(cfg->provider), "auto");
    snprintf(cfg->language, sizeof(cfg->language), "en");
    cfg->lines = 6;
    cfg->width = 700;
    cfg->font_size = 18;
    cfg->header_font_size = 16;
    cfg->show_header = 1;
    cfg->opacity = 0.62;
    cfg->margin = 24;
    cfg->refresh_ms = 100;
    cfg->emotes = 1;
    cfg->emote_size = 24;
    snprintf(cfg->position, sizeof(cfg->position), "top-right");
    snprintf(cfg->monitor, sizeof(cfg->monitor), "primary");
    snprintf(cfg->text_align, sizeof(cfg->text_align), "left");
    snprintf(cfg->message_order, sizeof(cfg->message_order), "oldest-first");
}

void config_default_path(char *out, size_t out_size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(out, out_size, "%s/chathud/config", xdg);
        return;
    }

    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(out, out_size, "%s/.config/chathud/config", home);
}

void config_load(AppConfig *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[3072];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '\0' || *p == '#') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';

        char *key = trim(p);
        char *value = trim(eq + 1);

        if (strcmp(key, "stream_url") == 0) {
            snprintf(cfg->stream_url, sizeof(cfg->stream_url), "%s", value);
        } else if (strcmp(key, "language") == 0) {
            snprintf(cfg->language, sizeof(cfg->language), "%s", value);
        } else if (strcmp(key, "provider") == 0) {
            snprintf(cfg->provider, sizeof(cfg->provider), "%s", value);
        } else if (strcmp(key, "youtube_api_key") == 0) {
            snprintf(cfg->youtube_api_key, sizeof(cfg->youtube_api_key), "%s", value);
        } else if (strcmp(key, "lines") == 0) {
            cfg->lines = atoi(value);
        } else if (strcmp(key, "width") == 0) {
            cfg->width = atoi(value);
        } else if (strcmp(key, "font_size") == 0) {
            cfg->font_size = atoi(value);
        } else if (strcmp(key, "header_font_size") == 0) {
            cfg->header_font_size = atoi(value);
        } else if (strcmp(key, "show_header") == 0) {
            cfg->show_header = parse_bool(value, cfg->show_header);
        } else if (strcmp(key, "opacity") == 0 || strcmp(key, "background_opacity") == 0) {
            cfg->opacity = atof(value);
        } else if (strcmp(key, "margin") == 0) {
            cfg->margin = atoi(value);
        } else if (strcmp(key, "refresh_ms") == 0) {
            cfg->refresh_ms = atoi(value);
        } else if (strcmp(key, "emotes") == 0) {
            cfg->emotes = parse_bool(value, cfg->emotes);
        } else if (strcmp(key, "emote_size") == 0) {
            cfg->emote_size = atoi(value);
        } else if (strcmp(key, "monitor") == 0) {
            snprintf(cfg->monitor, sizeof(cfg->monitor), "%s", value);
        } else if (strcmp(key, "position") == 0) {
            snprintf(cfg->position, sizeof(cfg->position), "%s", value);
        } else if (strcmp(key, "text_align") == 0) {
            snprintf(cfg->text_align, sizeof(cfg->text_align), "%s", value);
        } else if (strcmp(key, "message_order") == 0) {
            snprintf(cfg->message_order, sizeof(cfg->message_order), "%s", value);
        }
    }

    fclose(f);

    if (cfg->lines < 1) cfg->lines = 1;
    if (cfg->lines > CHATHUD_MAX_MESSAGES) cfg->lines = CHATHUD_MAX_MESSAGES;
    if (cfg->width < 220) cfg->width = 220;
    if (cfg->width > 1800) cfg->width = 1800;
    if (cfg->font_size < 8) cfg->font_size = 8;
    if (cfg->font_size > 64) cfg->font_size = 64;
    if (cfg->header_font_size < 8) cfg->header_font_size = 8;
    if (cfg->header_font_size > 64) cfg->header_font_size = 64;
    if (cfg->opacity < 0.0) cfg->opacity = 0.0;
    if (cfg->opacity > 1.0) cfg->opacity = 1.0;
    if (cfg->margin < 0) cfg->margin = 0;
    if (cfg->refresh_ms < 30) cfg->refresh_ms = 30;
    if (cfg->refresh_ms > 1000) cfg->refresh_ms = 1000;
    if (cfg->emote_size < 12) cfg->emote_size = 12;
    if (cfg->emote_size > 64) cfg->emote_size = 64;
}
