#include "chat.h"
#include "config.h"
#include "overlay.h"
#include "youtube.h"
#include "ytdlp.h"

#include <curl/curl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static ChatState *g_state = NULL;

static void on_signal(int sig) {
    (void)sig;
    if (g_state) g_state->stop = 1;
}

static void normalize_url_arg(const char *input, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!input) return;

    const char *start = input;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;

    /* Jeśli ktoś wklei link skopiowany z renderowanego Markdownu:
       [https://youtube/...](https://youtube/...)
       bierzemy URL z nawiasów zamiast przekazywać śmieci do yt-dlp. */
    if (*start == '[') {
        const char *mid = strstr(start, "](");
        const char *end = strrchr(start, ')');
        if (mid && end && end > mid + 2) {
            size_t n = (size_t)(end - (mid + 2));
            if (n >= out_size) n = out_size - 1;
            memcpy(out, mid + 2, n);
            out[n] = '\0';
            return;
        }
    }

    /* Obsłuż też <https://...>. */
    size_t len = strlen(start);
    if (len >= 2 && start[0] == '<' && start[len - 1] == '>') {
        start++;
        len -= 2;
    }
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' ||
                       start[len - 1] == '\n' || start[len - 1] == '\r')) len--;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "ChatHUD 0.5.1\n\n"
        "Użycie:\n"
        "  %s --url URL -- %%command%%\n"
        "  %s --url URL --no-game\n"
        "  %s --demo --no-game\n\n"
        "Opcje:\n"
        "  --url URL       Link do YouTube Live\n"
        "  --api-key KEY   Nadpisz youtube_api_key z configu\n"
        "  --config PATH   Własny plik configu\n"
        "  --no-game       Pokaż overlay bez uruchamiania gry\n"
        "  --demo          Wstaw przykładowe wiadomości\n"
        "  --help          Pokaż pomoc\n",
        argv0, argv0, argv0);
}

static void add_demo_messages(ChatState *state) {
    chat_state_set_status(state, "ChatHUD demo", 0);
    chat_state_add(state, "DevVoidPL", "siema, to już leci nad grą");
    chat_state_add(state, "PixelCat", "boss za tobą xD");
    chat_state_add(state, "LinuxEnjoyer", "MangoHud + ChatHUD brzmi dobrze");
    chat_state_add(state, "Karton", "test polskich znaków: ąęśćżźółń");
    chat_state_add(state, "BarryFan", "lekki overlay bez Electrona");
}

/*
 * Steam lubi wrappery, które kończą się exec() właściwej komendy gry.
 * Wtedy PID uruchomiony przez Steam pozostaje tym samym PID-em po przejściu
 * z ChatHUD do Proton/native game. Overlay działa w osobnym lekkim procesie.
 */
static int spawn_worker(const char *url,
                        const char *api_key,
                        const char *config_path,
                        int demo,
                        pid_t watch_pid,
                        pid_t *worker_pid) {
    char self[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n < 0) {
        fprintf(stderr, "ChatHUD: readlink(/proc/self/exe): %s\n", strerror(errno));
        return -1;
    }
    self[n] = '\0';

    char pid_buf[32];
    snprintf(pid_buf, sizeof(pid_buf), "%ld", (long)watch_pid);

    /* Maksymalnie: exe, --worker, --watch-pid PID, --config P, --url U,
       --api-key K, --demo, NULL. */
    char *args[16];
    int i = 0;
    args[i++] = self;
    args[i++] = "--worker";
    args[i++] = "--watch-pid";
    args[i++] = pid_buf;

    if (config_path && config_path[0]) {
        args[i++] = "--config";
        args[i++] = (char *)config_path;
    }
    if (url && url[0]) {
        args[i++] = "--url";
        args[i++] = (char *)url;
    }
    if (api_key && api_key[0]) {
        args[i++] = "--api-key";
        args[i++] = (char *)api_key;
    }
    if (demo) args[i++] = "--demo";
    args[i] = NULL;

    int rc = posix_spawn(worker_pid, self, NULL, NULL, args, environ);
    if (rc != 0) {
        fprintf(stderr, "ChatHUD: nie udało się uruchomić procesu overlaya: %s\n",
                strerror(rc));
        return -1;
    }
    return 0;
}

static int run_overlay_worker(const AppConfig *cfg,
                              const char *url,
                              int demo,
                              pid_t watch_pid) {
    ChatState state;
    chat_state_init(&state);
    g_state = &state;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "ChatHUD: curl_global_init nie powiodło się.\n");
        chat_state_destroy(&state);
        return 1;
    }

    pthread_t chat_thread;
    int chat_thread_started = 0;
    YouTubeThreadArgs yt_args;
    YtDlpThreadArgs ytdlp_args;
    memset(&yt_args, 0, sizeof(yt_args));
    memset(&ytdlp_args, 0, sizeof(ytdlp_args));

    if (demo) {
        add_demo_messages(&state);
    } else {
        int want_ytdlp = strcmp(cfg->provider, "yt-dlp") == 0 ||
            (strcmp(cfg->provider, "auto") == 0 && ytdlp_available());

        if (want_ytdlp) {
            ytdlp_args.state = &state;
            ytdlp_args.emotes = cfg->emotes;
            snprintf(ytdlp_args.url, sizeof(ytdlp_args.url), "%s", url);
            if (pthread_create(&chat_thread, NULL, ytdlp_thread, &ytdlp_args) == 0) {
                chat_thread_started = 1;
            } else {
                chat_state_set_status(&state, "Nie udało się uruchomić wątku yt-dlp", 1);
            }
        } else {
            yt_args.state = &state;
            snprintf(yt_args.url, sizeof(yt_args.url), "%s", url);
            snprintf(yt_args.api_key, sizeof(yt_args.api_key), "%s", cfg->youtube_api_key);
            if (pthread_create(&chat_thread, NULL, youtube_thread, &yt_args) == 0) {
                chat_thread_started = 1;
            } else {
                chat_state_set_status(&state, "Nie udało się uruchomić wątku YouTube API", 1);
            }
        }
    }

    OverlayArgs overlay_args = {
        .state = &state,
        .config = cfg,
        .watch_pid = watch_pid
    };

    int exit_code = overlay_run(&overlay_args);

    chat_state_request_stop(&state);
    if (chat_thread_started) pthread_join(chat_thread, NULL);

    curl_global_cleanup();
    chat_state_destroy(&state);
    g_state = NULL;
    return exit_code;
}

int main(int argc, char **argv) {
    AppConfig cfg;
    config_defaults(&cfg);

    char config_path[1024];
    config_default_path(config_path, sizeof(config_path));

    const char *url = NULL;
    const char *api_key_override = NULL;
    int no_game = 0;
    int demo = 0;
    int worker = 0;
    pid_t watch_pid = 0;
    int command_index = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            command_index = i + 1;
            break;
        } else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
            url = argv[++i];
        } else if (strcmp(argv[i], "--api-key") == 0 && i + 1 < argc) {
            api_key_override = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            snprintf(config_path, sizeof(config_path), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--no-game") == 0) {
            no_game = 1;
        } else if (strcmp(argv[i], "--demo") == 0) {
            demo = 1;
        } else if (strcmp(argv[i], "--worker") == 0) {
            worker = 1;
        } else if (strcmp(argv[i], "--watch-pid") == 0 && i + 1 < argc) {
            char *end = NULL;
            long parsed = strtol(argv[++i], &end, 10);
            if (!end || *end != '\0' || parsed <= 0) {
                fprintf(stderr, "ChatHUD: zły --watch-pid.\n");
                return 2;
            }
            watch_pid = (pid_t)parsed;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Nieznana opcja: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    config_load(&cfg, config_path);
    if (api_key_override) {
        snprintf(cfg.youtube_api_key, sizeof(cfg.youtube_api_key), "%s", api_key_override);
    }

    char normalized_url[2048];
    if (url) {
        normalize_url_arg(url, normalized_url, sizeof(normalized_url));
        url = normalized_url;
    }

    if (!demo && !url) {
        fprintf(stderr, "Brak --url. Do testu użyj --demo --no-game.\n");
        return 2;
    }

    /* Proces roboczy wyłącznie obsługuje czat + overlay. */
    if (worker) {
        return run_overlay_worker(&cfg, url, demo, watch_pid);
    }

    /* Tryb ręczny: overlay bez gry. */
    if (no_game) {
        return run_overlay_worker(&cfg, url, demo, 0);
    }

    if (command_index < 0 || command_index >= argc) {
        fprintf(stderr, "Brak komendy gry po --.\n");
        fprintf(stderr, "Steam launch options: chathud --url \"URL\" -- %%command%%\n");
        return 2;
    }

    /*
     * WAŻNE: nie uruchamiamy gry jako dziecka ChatHUD i nie czekamy na nią.
     * Odpalamy tylko overlay-worker, a następnie zastępujemy proces ChatHUD
     * dokładnie komendą otrzymaną ze Steam. Dzięki execvp PID widziany przez
     * Steam pozostaje bez zmian, co jest znacznie bardziej kompatybilne z
     * Protonem, Steam Runtime i innymi wrapperami.
     */
    pid_t worker_pid = -1;
    pid_t launcher_pid = getpid();
    if (spawn_worker(url, api_key_override, config_path, demo,
                     launcher_pid, &worker_pid) != 0) {
        return 1;
    }

    fprintf(stderr, "ChatHUD: overlay PID=%ld, exec gry:", (long)worker_pid);
    for (int i = command_index; i < argc; i++) fprintf(stderr, " %s", argv[i]);
    fputc('\n', stderr);

    execvp(argv[command_index], &argv[command_index]);

    /* Dochodzimy tutaj tylko, gdy exec się nie udał. */
    int saved_errno = errno;
    fprintf(stderr, "ChatHUD: execvp('%s') nie powiodło się: %s\n",
            argv[command_index], strerror(saved_errno));
    if (worker_pid > 0) {
        kill(worker_pid, SIGTERM);
        waitpid(worker_pid, NULL, 0);
    }
    return 127;
}
