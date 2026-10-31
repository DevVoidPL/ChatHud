#include "youtube.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *data;
    size_t size;
} HttpBuffer;

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    HttpBuffer *buf = userp;

    char *new_data = realloc(buf->data, buf->size + total + 1);
    if (!new_data) return 0;

    buf->data = new_data;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static char *http_get(const char *url, long *http_code) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HttpBuffer buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ChatHUD/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        free(buf.data);
        curl_easy_cleanup(curl);
        return NULL;
    }

    if (http_code) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    curl_easy_cleanup(curl);
    return buf.data;
}

static void sleep_interruptible(ChatState *state, int ms) {
    const int step = 100;
    int left = ms;
    while (left > 0 && !chat_state_should_stop(state)) {
        int now = left < step ? left : step;
        usleep((useconds_t)now * 1000);
        left -= now;
    }
}

static int is_video_id_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static void copy_id_segment(const char *start, char *out, size_t out_size) {
    size_t i = 0;
    while (start[i] && is_video_id_char(start[i]) && i + 1 < out_size) {
        out[i] = start[i];
        i++;
    }
    out[i] = '\0';
}

int youtube_extract_video_id(const char *input, char *out, size_t out_size) {
    if (!input || !*input || out_size < 2) return 0;

    const char *p = strstr(input, "youtu.be/");
    if (p) {
        copy_id_segment(p + strlen("youtu.be/"), out, out_size);
        return out[0] != '\0';
    }

    p = strstr(input, "v=");
    if (p) {
        copy_id_segment(p + 2, out, out_size);
        return out[0] != '\0';
    }

    p = strstr(input, "/live/");
    if (p) {
        copy_id_segment(p + strlen("/live/"), out, out_size);
        return out[0] != '\0';
    }

    p = strstr(input, "/shorts/");
    if (p) {
        copy_id_segment(p + strlen("/shorts/"), out, out_size);
        return out[0] != '\0';
    }

    size_t len = strlen(input);
    if (len >= 6 && len <= 64) {
        for (size_t i = 0; i < len; i++) {
            if (!is_video_id_char(input[i])) return 0;
        }
        snprintf(out, out_size, "%s", input);
        return 1;
    }

    return 0;
}

static int json_get_string(json_object *obj, const char *key, const char **out) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(obj, key, &value)) return 0;
    if (!json_object_is_type(value, json_type_string)) return 0;
    *out = json_object_get_string(value);
    return *out != NULL;
}

static int fetch_live_chat_id(const char *video_id, const char *api_key,
                              char *out, size_t out_size, char *err, size_t err_size) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(err, err_size, "Nie udało się uruchomić libcurl");
        return 0;
    }

    char *escaped_id = curl_easy_escape(curl, video_id, 0);
    char *escaped_key = curl_easy_escape(curl, api_key, 0);
    char url[4096];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=%s&key=%s",
             escaped_id ? escaped_id : video_id,
             escaped_key ? escaped_key : api_key);
    curl_free(escaped_id);
    curl_free(escaped_key);
    curl_easy_cleanup(curl);

    long code = 0;
    char *body = http_get(url, &code);
    if (!body) {
        snprintf(err, err_size, "Błąd sieci przy pobieraniu danych live");
        return 0;
    }

    json_object *root = json_tokener_parse(body);
    free(body);
    if (!root) {
        snprintf(err, err_size, "Nieprawidłowa odpowiedź YouTube API");
        return 0;
    }

    if (code != 200) {
        snprintf(err, err_size, "YouTube API: HTTP %ld", code);
        json_object_put(root);
        return 0;
    }

    json_object *items = NULL;
    if (!json_object_object_get_ex(root, "items", &items) ||
        !json_object_is_type(items, json_type_array) ||
        json_object_array_length(items) == 0) {
        snprintf(err, err_size, "Nie znaleziono live dla tego linku");
        json_object_put(root);
        return 0;
    }

    json_object *item = json_object_array_get_idx(items, 0);
    json_object *details = NULL;
    const char *chat_id = NULL;
    if (!json_object_object_get_ex(item, "liveStreamingDetails", &details) ||
        !json_get_string(details, "activeLiveChatId", &chat_id)) {
        snprintf(err, err_size, "Live nie jest aktywny albo czat jest wyłączony");
        json_object_put(root);
        return 0;
    }

    snprintf(out, out_size, "%s", chat_id);
    json_object_put(root);
    return 1;
}

static int parse_chat_page(ChatState *state, const char *body,
                           char *next_page, size_t next_page_size,
                           int *poll_ms, char *err, size_t err_size) {
    json_object *root = json_tokener_parse(body);
    if (!root) {
        snprintf(err, err_size, "Nieprawidłowa odpowiedź z live chat");
        return 0;
    }

    json_object *error_obj = NULL;
    if (json_object_object_get_ex(root, "error", &error_obj)) {
        json_object *message_obj = NULL;
        const char *msg = NULL;
        if (json_object_object_get_ex(error_obj, "message", &message_obj) &&
            json_object_is_type(message_obj, json_type_string)) {
            msg = json_object_get_string(message_obj);
        }
        snprintf(err, err_size, "YouTube API: %s", msg ? msg : "chat error");
        json_object_put(root);
        return 0;
    }

    const char *token = NULL;
    json_get_string(root, "nextPageToken", &token);
    if (token) snprintf(next_page, next_page_size, "%s", token);

    json_object *interval = NULL;
    if (json_object_object_get_ex(root, "pollingIntervalMillis", &interval)) {
        int value = json_object_get_int(interval);
        if (value >= 500 && value <= 30000) *poll_ms = value;
    }

    json_object *items = NULL;
    if (json_object_object_get_ex(root, "items", &items) &&
        json_object_is_type(items, json_type_array)) {
        size_t n = json_object_array_length(items);
        for (size_t i = 0; i < n; i++) {
            json_object *item = json_object_array_get_idx(items, i);
            json_object *snippet = NULL;
            json_object *author = NULL;
            const char *text = NULL;
            const char *name = NULL;

            if (json_object_object_get_ex(item, "snippet", &snippet)) {
                json_get_string(snippet, "displayMessage", &text);
            }
            if (json_object_object_get_ex(item, "authorDetails", &author)) {
                json_get_string(author, "displayName", &name);
            }

            if (text && *text) chat_state_add(state, name ? name : "?", text);
        }
    }

    json_object_put(root);
    return 1;
}

void *youtube_thread(void *arg) {
    YouTubeThreadArgs *args = arg;
    ChatState *state = args->state;

    char video_id[128];
    if (!youtube_extract_video_id(args->url, video_id, sizeof(video_id))) {
        chat_state_set_status(state, "Nie rozpoznaję ID filmu z podanego linku", 1);
        return NULL;
    }

    if (args->api_key[0] == '\0') {
        chat_state_set_status(state, "Brak youtube_api_key w ~/.config/chathud/config", 1);
        return NULL;
    }

    char chat_id[512];
    char err[256];

    while (!chat_state_should_stop(state)) {
        err[0] = '\0';
        if (fetch_live_chat_id(video_id, args->api_key, chat_id, sizeof(chat_id), err, sizeof(err))) {
            break;
        }
        chat_state_set_status(state, err, 1);
        sleep_interruptible(state, 10000);
    }

    if (chat_state_should_stop(state)) return NULL;
    chat_state_set_status(state, "Połączono z YouTube Live Chat", 0);

    char page_token[2048] = {0};
    int poll_ms = 3000;

    while (!chat_state_should_stop(state)) {
        CURL *curl = curl_easy_init();
        if (!curl) {
            chat_state_set_status(state, "Nie udało się uruchomić libcurl", 1);
            sleep_interruptible(state, 3000);
            continue;
        }

        char *escaped_chat = curl_easy_escape(curl, chat_id, 0);
        char *escaped_key = curl_easy_escape(curl, args->api_key, 0);
        char *escaped_page = page_token[0] ? curl_easy_escape(curl, page_token, 0) : NULL;

        char url[8192];
        snprintf(url, sizeof(url),
                 "https://www.googleapis.com/youtube/v3/liveChat/messages?liveChatId=%s&part=snippet,authorDetails&maxResults=200&key=%s%s%s",
                 escaped_chat ? escaped_chat : chat_id,
                 escaped_key ? escaped_key : args->api_key,
                 escaped_page ? "&pageToken=" : "",
                 escaped_page ? escaped_page : "");

        curl_free(escaped_chat);
        curl_free(escaped_key);
        curl_free(escaped_page);
        curl_easy_cleanup(curl);

        long code = 0;
        char *body = http_get(url, &code);
        if (!body) {
            chat_state_set_status(state, "Błąd sieci przy pobieraniu wiadomości", 1);
            sleep_interruptible(state, 3000);
            continue;
        }

        if (code != 200) {
            snprintf(err, sizeof(err), "YouTube live chat: HTTP %ld", code);
            chat_state_set_status(state, err, 1);
            free(body);
            sleep_interruptible(state, 5000);
            continue;
        }

        char next_page[2048] = {0};
        err[0] = '\0';
        if (!parse_chat_page(state, body, next_page, sizeof(next_page), &poll_ms, err, sizeof(err))) {
            chat_state_set_status(state, err, 1);
            free(body);
            sleep_interruptible(state, 5000);
            continue;
        }
        free(body);

        if (next_page[0]) snprintf(page_token, sizeof(page_token), "%s", next_page);
        chat_state_set_status(state, "YouTube Live Chat", 0);
        sleep_interruptible(state, poll_ms);
    }

    return NULL;
}
