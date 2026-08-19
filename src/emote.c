#include "emote.h"

#include <curl/curl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static uint64_t fnv1a64(const char *s) {
    uint64_t h = UINT64_C(1469598103934665603);
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static int ensure_dir(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static size_t write_file_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    return fwrite(ptr, size, nmemb, (FILE *)userdata);
}

int emote_cache_fetch(const char *url, char *out_path, size_t out_size) {
    if (!url || !*url || !out_path || out_size == 0) return 0;

    const char *base = getenv("XDG_CACHE_HOME");
    char dir[1024];
    if (base && *base) {
        snprintf(dir, sizeof(dir), "%s/chathud/emotes", base);
    } else {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(dir, sizeof(dir), "%s/.cache/chathud/emotes", home);
    }
    if (!ensure_dir(dir)) return 0;

    uint64_t hash = fnv1a64(url);
    char path[1200];
    char tmp[1240];
    snprintf(path, sizeof(path), "%s/%016llx.img", dir, (unsigned long long)hash);
    snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid());

    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 0) {
        snprintf(out_path, out_size, "%s", path);
        return 1;
    }

    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(f);
        unlink(tmp);
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ChatHUD/0.5.1");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);

    if (rc != CURLE_OK) {
        unlink(tmp);
        return 0;
    }

    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return 0;
    }

    snprintf(out_path, out_size, "%s", path);
    return 1;
}
