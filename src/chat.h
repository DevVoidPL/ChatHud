#ifndef CHATHUD_CHAT_H
#define CHATHUD_CHAT_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHATHUD_MAX_MESSAGES 32
#define CHATHUD_AUTHOR_MAX 128
#define CHATHUD_ID_MAX 160
#define CHATHUD_TEXT_MAX 2048
#define CHATHUD_STATUS_MAX 256
#define CHATHUD_TITLE_MAX 256
#define CHATHUD_MAX_RUNS 64
#define CHATHUD_RUN_TEXT_MAX 512
#define CHATHUD_EMOTE_PATH_MAX 1024

typedef enum {
    CHAT_RUN_TEXT = 0,
    CHAT_RUN_EMOTE = 1
} ChatRunType;

typedef struct {
    ChatRunType type;
    char text[CHATHUD_RUN_TEXT_MAX];
    char emote_path[CHATHUD_EMOTE_PATH_MAX];
} ChatRun;

typedef struct {
    char id[CHATHUD_ID_MAX];
    char author[CHATHUD_AUTHOR_MAX];
    char text[CHATHUD_TEXT_MAX];
    ChatRun runs[CHATHUD_MAX_RUNS];
    size_t run_count;
} ChatMessage;

typedef struct {
    pthread_mutex_t mutex;
    ChatMessage messages[CHATHUD_MAX_MESSAGES];
    size_t count;
    char title[CHATHUD_TITLE_MAX];
    char status[CHATHUD_STATUS_MAX];
    int has_error;
    volatile sig_atomic_t stop;
} ChatState;

static inline void chat_message_init(ChatMessage *m, const char *author) {
    memset(m, 0, sizeof(*m));
    snprintf(m->author, sizeof(m->author), "%s", author ? author : "");
}

static inline void chat_message_set_id(ChatMessage *m, const char *id) {
    snprintf(m->id, sizeof(m->id), "%s", id ? id : "");
}

static inline void chat_message_append_fallback(ChatMessage *m, const char *text) {
    if (!text || !*text) return;
    size_t used = strlen(m->text);
    if (used + 1 >= sizeof(m->text)) return;
    snprintf(m->text + used, sizeof(m->text) - used, "%s", text);
}

static inline void chat_message_add_text(ChatMessage *m, const char *text) {
    if (!text || !*text) return;
    chat_message_append_fallback(m, text);
    if (m->run_count >= CHATHUD_MAX_RUNS) return;
    ChatRun *r = &m->runs[m->run_count++];
    r->type = CHAT_RUN_TEXT;
    snprintf(r->text, sizeof(r->text), "%s", text);
}

static inline void chat_message_add_emote(ChatMessage *m, const char *fallback,
                                          const char *path) {
    const char *safe_fallback = (fallback && *fallback) ? fallback : "[emote]";
    chat_message_append_fallback(m, safe_fallback);
    if (!path || !*path || m->run_count >= CHATHUD_MAX_RUNS) {
        if (m->run_count < CHATHUD_MAX_RUNS) {
            ChatRun *r = &m->runs[m->run_count++];
            r->type = CHAT_RUN_TEXT;
            snprintf(r->text, sizeof(r->text), "%s", safe_fallback);
        }
        return;
    }
    ChatRun *r = &m->runs[m->run_count++];
    r->type = CHAT_RUN_EMOTE;
    snprintf(r->text, sizeof(r->text), "%s", safe_fallback);
    snprintf(r->emote_path, sizeof(r->emote_path), "%s", path);
}

static inline void chat_state_init(ChatState *state) {
    pthread_mutex_init(&state->mutex, NULL);
    state->count = 0;
    state->title[0] = '\0';
    state->status[0] = '\0';
    state->has_error = 0;
    state->stop = 0;
}

static inline void chat_state_destroy(ChatState *state) {
    pthread_mutex_destroy(&state->mutex);
}

static inline void chat_state_add_message(ChatState *state, const ChatMessage *src) {
    pthread_mutex_lock(&state->mutex);

    if (src->id[0]) {
        for (size_t i = 0; i < state->count; i++) {
            if (strcmp(state->messages[i].id, src->id) == 0) {
                pthread_mutex_unlock(&state->mutex);
                return;
            }
        }
    } else if (state->count > 0) {
        ChatMessage *last = &state->messages[state->count - 1];
        if (strcmp(last->author, src->author) == 0 && strcmp(last->text, src->text) == 0) {
            pthread_mutex_unlock(&state->mutex);
            return;
        }
    }

    if (state->count < CHATHUD_MAX_MESSAGES) {
        state->messages[state->count++] = *src;
    } else {
        memmove(&state->messages[0], &state->messages[1],
                sizeof(ChatMessage) * (CHATHUD_MAX_MESSAGES - 1));
        state->messages[CHATHUD_MAX_MESSAGES - 1] = *src;
    }

    pthread_mutex_unlock(&state->mutex);
}

static inline void chat_state_add(ChatState *state, const char *author, const char *text) {
    ChatMessage m;
    chat_message_init(&m, author);
    chat_message_add_text(&m, text ? text : "");
    chat_state_add_message(state, &m);
}

static inline void chat_state_set_title(ChatState *state, const char *title) {
    pthread_mutex_lock(&state->mutex);
    snprintf(state->title, sizeof(state->title), "%s", title ? title : "");
    pthread_mutex_unlock(&state->mutex);
}

static inline void chat_state_set_status(ChatState *state, const char *status, int is_error) {
    pthread_mutex_lock(&state->mutex);
    snprintf(state->status, sizeof(state->status), "%s", status ? status : "");
    state->has_error = is_error;
    pthread_mutex_unlock(&state->mutex);
}

static inline void chat_state_request_stop(ChatState *state) { state->stop = 1; }
static inline int chat_state_should_stop(ChatState *state) { return state->stop != 0; }

#endif
