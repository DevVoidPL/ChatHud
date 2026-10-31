#include "twitch.h"

/* Protocol: https://dev.twitch.tv/docs/chat/irc/ . No PRIVMSG is ever sent. */

#include <curl/curl.h>
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static double now_seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

int twitch_extract_channel(const char *url, char *out, size_t capacity) {
    if (!url || !out || capacity < 2) return 0;
    out[0] = '\0';
    CURLU *parsed = curl_url();
    if (!parsed) return 0;
    char *scheme = NULL, *host = NULL, *path = NULL, *user = NULL, *port = NULL;
    int valid = 0;
    if (curl_url_set(parsed, CURLUPART_URL, url, 0) != CURLUE_OK) goto done;
    if (curl_url_get(parsed, CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK ||
        (strcmp(scheme, "https") != 0 && strcmp(scheme, "http") != 0)) goto done;
    if (curl_url_get(parsed, CURLUPART_HOST, &host, 0) != CURLUE_OK ||
        (strcasecmp(host, "twitch.tv") != 0 && strcasecmp(host, "www.twitch.tv") != 0 &&
         strcasecmp(host, "m.twitch.tv") != 0)) goto done;
    if (curl_url_get(parsed, CURLUPART_USER, &user, 0) == CURLUE_OK) goto done;
    if (curl_url_get(parsed, CURLUPART_PORT, &port, 0) == CURLUE_OK &&
        strcmp(port, "443") != 0 && strcmp(port, "80") != 0) goto done;
    if (curl_url_get(parsed, CURLUPART_PATH, &path, 0) != CURLUE_OK || path[0] != '/') goto done;
    size_t length = strlen(path + 1);
    if (length && path[length] == '/') length--;
    if (length < 1 || length > 25 || length >= capacity) goto done;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)path[i + 1];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) goto done;
        out[i] = (char)tolower(c);
    }
    out[length] = '\0';
    static const char *reserved[] = {"directory", "videos", "settings", "downloads", "inventory",
        "subscriptions", "wallet", "jobs", "p", "search", "turbo", "login", "signup", "moderator", "dashboard", NULL};
    for (int i = 0; reserved[i]; i++) if (strcmp(out, reserved[i]) == 0) goto done;
    valid = 1;
done:
    if (!valid) out[0] = '\0';
    curl_free(scheme); curl_free(host); curl_free(path); curl_free(user); curl_free(port);
    curl_url_cleanup(parsed);
    return valid;
}

static void wait_retry(ChatState *state, int seconds) {
    for (int i = 0; i < seconds * 10 && !chat_state_should_stop(state); i++) usleep(100000);
}

static int connect_chat(ChatState *state, CURL *curl, CURLM *multi) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://irc.chat.twitch.tv:6697/");
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
    if (curl_multi_add_handle(multi, curl) != CURLM_OK) return 0;
    int running = 0;
    CURLMcode result = curl_multi_perform(multi, &running);
    while (result == CURLM_OK && running && !chat_state_should_stop(state)) {
        result = curl_multi_poll(multi, NULL, 0, 100, NULL);
        if (result == CURLM_OK) result = curl_multi_perform(multi, &running);
    }
    if (result != CURLM_OK || chat_state_should_stop(state)) return 0;
    int remaining;
    CURLMsg *message;
    while ((message = curl_multi_info_read(multi, &remaining))) {
        if (message->msg == CURLMSG_DONE && message->easy_handle == curl)
            return message->data.result == CURLE_OK;
    }
    return 0;
}

static int send_irc(ChatState *state, CURL *curl, curl_socket_t socket, const char *line) {
    size_t length = strlen(line), sent = 0;
    double deadline = now_seconds() + 5;
    while (sent < length && !chat_state_should_stop(state) && now_seconds() < deadline) {
        size_t written = 0;
        CURLcode result = curl_easy_send(curl, line + sent, length - sent, &written);
        if (result == CURLE_OK && written > 0) sent += written;
        else if (result == CURLE_AGAIN || (result == CURLE_OK && written == 0)) {
            struct pollfd ready = {.fd = socket, .events = POLLOUT};
            if (poll(&ready, 1, 100) < 0 && errno != EINTR) return 0;
        } else return 0;
    }
    return sent == length;
}

/* IRCv3 tag escaping is decoded only into text, never executable commands. */
static void tag_value(const char *tags, const char *key, char *out, size_t capacity) {
    if (!capacity) return;
    out[0] = '\0';
    if (!tags) return;
    size_t key_length = strlen(key);
    for (const char *p = tags; *p;) {
        const char *end = strchr(p, ';');
        if (!end) end = p + strlen(p);
        if ((size_t)(end - p) > key_length && strncmp(p, key, key_length) == 0 && p[key_length] == '=') {
            p += key_length + 1;
            size_t used = 0;
            while (p < end && used + 1 < capacity) {
                char c = *p++;
                if (c == '\\' && p < end) {
                    c = *p++;
                    if (c == 's' || c == 'r' || c == 'n') c = ' ';
                    else if (c == ':') c = ';';
                }
                out[used++] = c;
            }
            out[used] = '\0';
            return;
        }
        p = *end ? end + 1 : end;
    }
}

static void remove_messages(ChatState *state, const char *id, const char *login) {
    pthread_mutex_lock(&state->mutex);
    size_t kept = 0;
    for (size_t i = 0; i < state->count; i++) {
        ChatMessage *message = &state->messages[i];
        int remove = id ? strcmp(message->id, id) == 0 :
                     login ? strcasecmp(message->login, login) == 0 : 1;
        if (!remove) {
            if (kept != i) state->messages[kept] = *message;
            kept++;
        }
    }
    state->count = kept;
    pthread_mutex_unlock(&state->mutex);
}

/* Returns 0 for normal traffic, -1 for reconnect, -2 for a rejected login. */
static int handle_line(TwitchThreadArgs *args, CURL *curl, curl_socket_t socket,
                       char *line, int *joined) {
    ChatState *state = args->state;
    char *tags = NULL, *prefix = NULL, *cursor = line, *end;
    if (*cursor == '@') {
        tags = ++cursor;
        end = strchr(cursor, ' ');
        if (!end) return 0;
        *end = '\0'; cursor = end + 1;
    }
    if (*cursor == ':') {
        prefix = ++cursor;
        end = strchr(cursor, ' ');
        if (!end) return 0;
        *end = '\0'; cursor = end + 1;
    }
    while (*cursor == ' ') cursor++;
    char *command = cursor;
    end = strchr(cursor, ' ');
    if (end) { *end = '\0'; cursor = end + 1; }
    else cursor += strlen(cursor);
    while (*cursor == ' ') cursor++;
    if (strcmp(command, "PING") == 0) {
        char reply[1024];
        if (strlen(cursor) > sizeof(reply) - 10) return -1;
        snprintf(reply, sizeof(reply), "PONG %s\r\n", cursor);
        return send_irc(state, curl, socket, reply) ? 0 : -1;
    }
    if (strcmp(command, "RECONNECT") == 0) return -1;
    if (strcmp(command, "001") == 0) {
        char join[64];
        snprintf(join, sizeof(join), "JOIN #%s\r\n", args->channel);
        return send_irc(state, curl, socket, join) ? 0 : -1;
    }
    char *target = cursor, *text = strstr(cursor, " :");
    if (text) { *text = '\0'; text += 2; }
    end = strchr(target, ' ');
    if (end) *end = '\0';
    if (strcmp(command, "NOTICE") == 0) {
        if (text && (strstr(text, "authentication failed") || strstr(text, "Improperly formatted auth"))) {
            chat_state_set_status(state, "Twitch: anonymous chat access was rejected", 1);
            return -2;
        }
        if (text && (strcmp(target, "*") == 0 || (target[0] == '#' && strcasecmp(target + 1, args->channel) == 0)))
            chat_state_set_status(state, text, 1);
        return 0;
    }
    if (*target != '#' || strcasecmp(target + 1, args->channel) != 0) return 0;
    if (strcmp(command, "ROOMSTATE") == 0 || strcmp(command, "366") == 0) {
        *joined = 1;
        chat_state_set_status(state, "", 0);
        return 0;
    }
    if (strcmp(command, "CLEARMSG") == 0) {
        char id[CHATHUD_ID_MAX];
        tag_value(tags, "target-msg-id", id, sizeof(id));
        if (*id) remove_messages(state, id, NULL);
        return 0;
    }
    if (strcmp(command, "CLEARCHAT") == 0) {
        remove_messages(state, NULL, text && *text ? text : NULL);
        return 0;
    }
    if (strcmp(command, "PRIVMSG") != 0 || !text || !prefix) return 0;
    char author[CHATHUD_AUTHOR_MAX], id[CHATHUD_ID_MAX];
    tag_value(tags, "display-name", author, sizeof(author));
    tag_value(tags, "id", id, sizeof(id));
    end = strchr(prefix, '!');
    if (end) *end = '\0';
    if (!*author) snprintf(author, sizeof(author), "%s", prefix);
    if (strncmp(text, "\001ACTION ", 8) == 0) {
        text += 8;
        size_t length = strlen(text);
        if (length && text[length - 1] == '\001') text[length - 1] = '\0';
    }
    ChatMessage message;
    chat_message_init(&message, author);
    snprintf(message.login, sizeof(message.login), "%s", prefix);
    chat_message_set_id(&message, id);
    chat_message_add_text(&message, text);
    chat_state_add_message(state, &message);
    *joined = 1;
    chat_state_set_status(state, "", 0);
    return 0;
}

void *twitch_thread(void *opaque) {
    TwitchThreadArgs *args = opaque;
    ChatState *state = args->state;
    char title[CHATHUD_TITLE_MAX];
    snprintf(title, sizeof(title), "Twitch · %s", args->channel);
    chat_state_set_title(state, title);
    unsigned int retry = 2;
    while (!chat_state_should_stop(state)) {
        chat_state_set_status(state, "Connecting to Twitch…", 0);
        CURL *curl = curl_easy_init();
        CURLM *multi = curl_multi_init();
        int result = -1, joined = 0;
        if (!curl || !multi) goto cleanup;
        if (!connect_chat(state, curl, multi)) goto cleanup;
        curl_socket_t socket = CURL_SOCKET_BAD;
        if (curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &socket) != CURLE_OK || socket == CURL_SOCKET_BAD) goto cleanup;
        struct timespec seed;
        clock_gettime(CLOCK_REALTIME, &seed);
        unsigned long suffix = ((unsigned long)seed.tv_nsec ^ (unsigned long)getpid()) % 900000000UL + 100000000UL;
        char login[192];
        snprintf(login, sizeof(login), "PASS SCHMOOPIIE\r\nNICK justinfan%lu\r\nCAP REQ :twitch.tv/tags twitch.tv/commands\r\n", suffix);
        if (!send_irc(state, curl, socket, login)) goto cleanup;
        result = 0;
        char pending[16384];
        size_t used = 0;
        int dropping = 0;
        double connected_at = now_seconds(), last_received = connected_at;
        while (!chat_state_should_stop(state)) {
            char bytes[8192];
            size_t received = 0;
            CURLcode code = curl_easy_recv(curl, bytes, sizeof(bytes), &received);
            if (code == CURLE_AGAIN) {
                double now = now_seconds();
                if ((!joined && now - connected_at > 20) || now - last_received > 360) break;
                struct pollfd ready = {.fd = socket, .events = POLLIN};
                if (poll(&ready, 1, 100) < 0 && errno != EINTR) break;
                continue;
            }
            if (code != CURLE_OK || !received) break;
            last_received = now_seconds();
            for (size_t i = 0; i < received; i++) {
                char byte = bytes[i];
                if (byte == '\n') {
                    if (!dropping) {
                        if (used && pending[used - 1] == '\r') used--;
                        pending[used] = '\0';
                        result = handle_line(args, curl, socket, pending, &joined);
                        if (result < 0) break;
                    }
                    used = 0; dropping = 0;
                } else if (byte == '\0' || used + 1 >= sizeof(pending)) dropping = 1;
                else if (!dropping) pending[used++] = byte;
            }
            if (result < 0) break;
            if (joined) retry = 2;
        }
cleanup:
        if (multi && curl) curl_multi_remove_handle(multi, curl);
        if (curl) curl_easy_cleanup(curl);
        if (multi) curl_multi_cleanup(multi);
        if (chat_state_should_stop(state) || result == -2) break;
        chat_state_set_status(state, "Twitch connection lost; reconnecting…", 1);
        wait_retry(state, (int)retry);
        if (retry < 30) retry *= 2;
        if (retry > 30) retry = 30;
    }
    return NULL;
}
