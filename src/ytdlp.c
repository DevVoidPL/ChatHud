#include "ytdlp.h"
#include "emote.h"

#include <json-c/json.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int path_is_executable(const char *path) {
    return path && access(path, X_OK) == 0;
}

int ytdlp_available(void) {
    const char *path = getenv("PATH");
    if (!path) return 0;
    char *copy = strdup(path);
    if (!copy) return 0;
    int found = 0;
    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        char candidate[PATH_MAX];
        snprintf(candidate, sizeof(candidate), "%s/yt-dlp", *dir ? dir : ".");
        if (path_is_executable(candidate)) { found = 1; break; }
    }
    free(copy);
    return found;
}

static int capture_channel_name(const char *url, char *out, size_t out_size) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return 0;

    char *argv[] = {
        "yt-dlp", "--ignore-config", "--skip-download", "--ignore-no-formats-error",
        "--no-warnings", "--print", "%(channel)s", (char *)url, NULL
    };

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        close(pipefd[0]); close(pipefd[1]); return 0;
    }
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    pid_t pid = -1;
    int rc = posix_spawnp(&pid, "yt-dlp", &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    if (rc != 0) { close(pipefd[0]); return 0; }

    char buf[512];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    if (n <= 0) return 0;
    buf[n] = '\0';
    char *eol = strpbrk(buf, "\r\n");
    if (eol) *eol = '\0';
    if (!buf[0] || strcmp(buf, "NA") == 0) return 0;
    snprintf(out, out_size, "%s", buf);
    return 1;
}

static pid_t spawn_ytdlp(const char *dir, const char *url) {
    char output[PATH_MAX];
    snprintf(output, sizeof(output), "%s/stream.%%(ext)s", dir);
    char *argv[] = {
        "yt-dlp", "--ignore-config", "--skip-download", "--ignore-no-formats-error",
        "--write-subs", "--sub-langs", "live_chat", "--no-part", "--no-progress",
        "--output", output, (char *)url, NULL
    };
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return -1;
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = -1;
    int rc = posix_spawnp(&pid, "yt-dlp", &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    return rc == 0 ? pid : -1;
}

static int filename_has_suffix(const char *name, const char *suffix) {
    size_t a = strlen(name), b = strlen(suffix);
    return a >= b && strcmp(name + a - b, suffix) == 0;
}

static int find_chat_file(const char *dir, char *out, size_t out_size) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d))) {
        if (filename_has_suffix(ent->d_name, ".live_chat.json")) {
            snprintf(out, out_size, "%s/%s", dir, ent->d_name);
            found = 1; break;
        }
    }
    closedir(d);
    return found;
}

static const char *obj_string(json_object *obj, const char *key) {
    json_object *v = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &v) ||
        !json_object_is_type(v, json_type_string)) return NULL;
    return json_object_get_string(v);
}

static const char *emoji_shortcut(json_object *emoji) {
    json_object *shortcuts = NULL;
    if (emoji && json_object_object_get_ex(emoji, "shortcuts", &shortcuts) &&
        json_object_is_type(shortcuts, json_type_array) && json_object_array_length(shortcuts) > 0) {
        json_object *first = json_object_array_get_idx(shortcuts, 0);
        if (json_object_is_type(first, json_type_string)) return json_object_get_string(first);
    }
    return NULL;
}

static const char *emoji_image_url(json_object *emoji) {
    json_object *image = NULL, *thumbs = NULL;
    if (!emoji || !json_object_object_get_ex(emoji, "image", &image) ||
        !json_object_object_get_ex(image, "thumbnails", &thumbs) ||
        !json_object_is_type(thumbs, json_type_array)) return NULL;

    const char *best = NULL;
    int best_width = -1;
    size_t n = json_object_array_length(thumbs);
    for (size_t i = 0; i < n; i++) {
        json_object *thumb = json_object_array_get_idx(thumbs, i);
        const char *url = obj_string(thumb, "url");
        if (!url) continue;
        json_object *wobj = NULL;
        int width = 0;
        if (json_object_object_get_ex(thumb, "width", &wobj)) width = json_object_get_int(wobj);
        if (!best || width >= best_width) { best = url; best_width = width; }
    }
    return best;
}

static void rich_text_to_message(json_object *rich_text, ChatMessage *msg, int want_emotes) {
    if (!rich_text) return;
    const char *simple = obj_string(rich_text, "simpleText");
    if (simple) { chat_message_add_text(msg, simple); return; }

    json_object *runs = NULL;
    if (!json_object_object_get_ex(rich_text, "runs", &runs) ||
        !json_object_is_type(runs, json_type_array)) return;

    size_t n = json_object_array_length(runs);
    for (size_t i = 0; i < n; i++) {
        json_object *run = json_object_array_get_idx(runs, i);
        const char *text = obj_string(run, "text");
        if (text) { chat_message_add_text(msg, text); continue; }

        json_object *emoji = NULL;
        if (!json_object_object_get_ex(run, "emoji", &emoji)) continue;
        const char *shortcut = emoji_shortcut(emoji);
        const char *url = emoji_image_url(emoji);
        char cached[CHATHUD_EMOTE_PATH_MAX] = {0};
        if (want_emotes && url && emote_cache_fetch(url, cached, sizeof(cached)))
            chat_message_add_emote(msg, shortcut, cached);
        else
            chat_message_add_text(msg, shortcut ? shortcut : "[emote]");
    }
}

static json_object *find_message_renderer(json_object *item, const char **kind) {
    static const char *keys[] = {
        "liveChatTextMessageRenderer", "liveChatPaidMessageRenderer",
        "liveChatMembershipItemRenderer", "liveChatPaidStickerRenderer"
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_object *renderer = NULL;
        if (json_object_object_get_ex(item, keys[i], &renderer)) {
            if (kind) *kind = keys[i];
            return renderer;
        }
    }
    return NULL;
}

static void parse_add_chat_item(ChatState *state, json_object *action, int want_emotes) {
    json_object *add = NULL, *item = NULL;
    if (!json_object_object_get_ex(action, "addChatItemAction", &add) ||
        !json_object_object_get_ex(add, "item", &item)) return;

    const char *kind = NULL;
    json_object *renderer = find_message_renderer(item, &kind);
    if (!renderer) return;

    char author[CHATHUD_AUTHOR_MAX] = "?";
    json_object *author_name = NULL;
    if (json_object_object_get_ex(renderer, "authorName", &author_name)) {
        const char *name = obj_string(author_name, "simpleText");
        if (name && *name) snprintf(author, sizeof(author), "%s", name);
    }

    ChatMessage msg;
    chat_message_init(&msg, author);
    const char *message_id = obj_string(renderer, "id");
    if (message_id) chat_message_set_id(&msg, message_id);
    json_object *rich = NULL;
    if (json_object_object_get_ex(renderer, "message", &rich))
        rich_text_to_message(rich, &msg, want_emotes);
    if (!msg.text[0] && json_object_object_get_ex(renderer, "headerSubtext", &rich))
        rich_text_to_message(rich, &msg, want_emotes);
    if (!msg.text[0] && kind && strcmp(kind, "liveChatPaidStickerRenderer") == 0)
        chat_message_add_text(&msg, "[Super Sticker]");
    if (!msg.text[0] && kind && strcmp(kind, "liveChatMembershipItemRenderer") == 0)
        chat_message_add_text(&msg, "[Członkostwo]");
    if (msg.text[0]) chat_state_add_message(state, &msg);
}

static void parse_json_line(ChatState *state, const char *line, int want_emotes) {
    json_object *root = json_tokener_parse(line);
    if (!root) return;
    json_object *replay = NULL;
    if (!json_object_object_get_ex(root, "replayChatItemAction", &replay)) {
        json_object_put(root); return;
    }
    json_object *actions = NULL;
    if (!json_object_object_get_ex(replay, "actions", &actions) ||
        !json_object_is_type(actions, json_type_array)) {
        json_object_put(root); return;
    }
    size_t n = json_object_array_length(actions);
    for (size_t i = 0; i < n; i++) parse_add_chat_item(state, json_object_array_get_idx(actions, i), want_emotes);
    json_object_put(root);
}

static int process_new_bytes(ChatState *state, int fd, char **pending,
                             size_t *pending_len, size_t *pending_cap, int want_emotes) {
    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) { if (errno == EINTR) return 1; return 0; }
    if (n == 0) return 1;

    size_t needed = *pending_len + (size_t)n + 1;
    if (needed > *pending_cap) {
        size_t new_cap = *pending_cap ? *pending_cap : 16384;
        while (new_cap < needed) new_cap *= 2;
        char *new_buf = realloc(*pending, new_cap);
        if (!new_buf) return 0;
        *pending = new_buf; *pending_cap = new_cap;
    }
    memcpy(*pending + *pending_len, buf, (size_t)n);
    *pending_len += (size_t)n; (*pending)[*pending_len] = '\0';

    size_t consumed = 0;
    for (size_t i = 0; i < *pending_len; i++) {
        if ((*pending)[i] == '\n') {
            (*pending)[i] = '\0';
            if (i > consumed) parse_json_line(state, *pending + consumed, want_emotes);
            consumed = i + 1;
        }
    }
    if (consumed > 0) {
        memmove(*pending, *pending + consumed, *pending_len - consumed);
        *pending_len -= consumed; (*pending)[*pending_len] = '\0';
    }
    return 1;
}

static void cleanup_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
            unlink(path);
        }
        closedir(d);
    }
    rmdir(dir);
}

void *ytdlp_thread(void *arg) {
    YtDlpThreadArgs *args = arg;
    ChatState *state = args->state;

    chat_state_set_status(state, "Łączę z YouTube…", 0);
    char channel[CHATHUD_TITLE_MAX];
    if (capture_channel_name(args->url, channel, sizeof(channel)))
        chat_state_set_title(state, channel);

    char tmp_template[] = "/tmp/chathud-yt-XXXXXX";
    char *dir = mkdtemp(tmp_template);
    if (!dir) { chat_state_set_status(state, "Nie udało się utworzyć katalogu /tmp", 1); return NULL; }

    pid_t child = spawn_ytdlp(dir, args->url);
    if (child < 0) {
        chat_state_set_status(state, "Nie udało się uruchomić yt-dlp", 1);
        cleanup_dir(dir); return NULL;
    }

    char chat_file[PATH_MAX] = {0};
    int child_status = 0;
    while (!chat_state_should_stop(state) && !chat_file[0]) {
        if (find_chat_file(dir, chat_file, sizeof(chat_file))) break;
        pid_t rc = waitpid(child, &child_status, WNOHANG);
        if (rc == child) {
            chat_state_set_status(state, "yt-dlp zakończył się zanim pojawił się live chat", 1);
            cleanup_dir(dir); return NULL;
        }
        usleep(100000);
    }
    if (chat_state_should_stop(state)) {
        kill(child, SIGTERM); waitpid(child, NULL, 0); cleanup_dir(dir); return NULL;
    }

    int fd = open(chat_file, O_RDONLY);
    if (fd < 0) {
        chat_state_set_status(state, "Nie mogę otworzyć live_chat z yt-dlp", 1);
        kill(child, SIGTERM); waitpid(child, NULL, 0); cleanup_dir(dir); return NULL;
    }

    chat_state_set_status(state, "", 0);
    char *pending = NULL;
    size_t pending_len = 0, pending_cap = 0;
    while (!chat_state_should_stop(state)) {
        if (!process_new_bytes(state, fd, &pending, &pending_len, &pending_cap, args->emotes)) {
            chat_state_set_status(state, "Błąd odczytu live chat z yt-dlp", 1); break;
        }
        pid_t rc = waitpid(child, &child_status, WNOHANG);
        if (rc == child) {
            chat_state_set_status(state, "Live chat zakończony", 0); break;
        }
        usleep(100000);
    }

    free(pending); close(fd);
    pid_t rc = waitpid(child, NULL, WNOHANG);
    if (rc == 0) { kill(child, SIGTERM); waitpid(child, NULL, 0); }
    cleanup_dir(dir);
    return NULL;
}
