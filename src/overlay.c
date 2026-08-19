#include "overlay.h"
#include "image.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUILT_TEXT 16384
#define IMAGE_CACHE_MAX 128

typedef struct {
    int byte_index;
    char path[CHATHUD_EMOTE_PATH_MAX];
} EmotePlacement;

typedef struct {
    char text[MAX_BUILT_TEXT];
    int author_end;
    EmotePlacement emotes[CHATHUD_MAX_RUNS];
    size_t emote_count;
} BuiltMessage;

typedef struct {
    char path[CHATHUD_EMOTE_PATH_MAX];
    cairo_surface_t *surface;
} ImageCacheEntry;

typedef struct {
    ImageCacheEntry entries[IMAGE_CACHE_MAX];
    size_t count;
} ImageCache;

/*
 * Te struktury są duże (ChatMessage zawiera runy i ścieżki emotek).
 * W v0.5 snapshot + BuiltMessage leżały lokalnie w render_overlay(),
 * co zużywało kilka MB stosu na jedno wywołanie i na części systemów
 * kończyło się SIGSEGV przy wejściu do renderera. Trzymamy je raz na heapie.
 */
typedef struct {
    ChatMessage snapshot[CHATHUD_MAX_MESSAGES];
    BuiltMessage built[CHATHUD_MAX_MESSAGES];
    int heights[CHATHUD_MAX_MESSAGES];
} RenderWorkspace;

static void set_atom_list(Display *dpy, Window win, const char *property,
                          const char **names, size_t count) {
    Atom prop = XInternAtom(dpy, property, False);
    Atom *atoms = calloc(count, sizeof(Atom));
    if (!atoms) return;
    for (size_t i = 0; i < count; i++) atoms[i] = XInternAtom(dpy, names[i], False);
    XChangeProperty(dpy, win, prop, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)atoms, (int)count);
    free(atoms);
}

static void round_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    double x2 = x + w, y2 = y + h;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x2 - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x2 - r, y2 - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y2 - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

static int anchor_x(const AppConfig *cfg, int screen_w, int panel_w) {
    if (strstr(cfg->position, "left")) return cfg->margin;
    if (strstr(cfg->position, "right")) return screen_w - panel_w - cfg->margin;
    return (screen_w - panel_w) / 2;
}

static int anchor_y(const AppConfig *cfg, int screen_h, int panel_h) {
    if (strstr(cfg->position, "top")) return cfg->margin;
    if (strstr(cfg->position, "bottom")) return screen_h - panel_h - cfg->margin;
    return (screen_h - panel_h) / 2;
}

static PangoAlignment text_alignment(const AppConfig *cfg) {
    if (strcmp(cfg->text_align, "center") == 0) return PANGO_ALIGN_CENTER;
    if (strcmp(cfg->text_align, "right") == 0) return PANGO_ALIGN_RIGHT;
    return PANGO_ALIGN_LEFT;
}

static void append_buf(char *dst, size_t cap, const char *src) {
    if (!src || !*src) return;
    size_t used = strlen(dst);
    if (used + 1 >= cap) return;
    snprintf(dst + used, cap - used, "%s", src);
}

static void build_message(const ChatMessage *m, BuiltMessage *out) {
    memset(out, 0, sizeof(*out));
    append_buf(out->text, sizeof(out->text), m->author);
    out->author_end = (int)strlen(out->text);
    append_buf(out->text, sizeof(out->text), ": ");

    if (m->run_count == 0) {
        append_buf(out->text, sizeof(out->text), m->text);
        return;
    }

    for (size_t i = 0; i < m->run_count; i++) {
        const ChatRun *r = &m->runs[i];
        if (r->type == CHAT_RUN_TEXT || !r->emote_path[0]) {
            append_buf(out->text, sizeof(out->text), r->text);
            continue;
        }
        if (out->emote_count >= CHATHUD_MAX_RUNS) {
            append_buf(out->text, sizeof(out->text), r->text);
            continue;
        }
        size_t used = strlen(out->text);
        if (used + 4 >= sizeof(out->text)) break;
        EmotePlacement *ep = &out->emotes[out->emote_count++];
        ep->byte_index = (int)used;
        snprintf(ep->path, sizeof(ep->path), "%s", r->emote_path);
        /* U+FFFC OBJECT REPLACEMENT CHARACTER. Pango rezerwuje na niego
           miejsce, a sam glif ukrywamy atrybutem alpha=0 i rysujemy obraz. */
        memcpy(out->text + used, "\xEF\xBF\xBC", 3);
        out->text[used + 3] = '\0';
    }
}

static PangoAttrList *message_attrs(const BuiltMessage *msg) {
    PangoAttrList *attrs = pango_attr_list_new();
    if (msg->author_end > 0) {
        PangoAttribute *bold = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        bold->start_index = 0;
        bold->end_index = (guint)msg->author_end;
        pango_attr_list_insert(attrs, bold);
    }
    for (size_t i = 0; i < msg->emote_count; i++) {
        PangoAttribute *alpha = pango_attr_foreground_alpha_new(0);
        alpha->start_index = (guint)msg->emotes[i].byte_index;
        alpha->end_index = (guint)(msg->emotes[i].byte_index + 3);
        pango_attr_list_insert(attrs, alpha);
    }
    return attrs;
}

static void prepare_layout(PangoLayout *layout, PangoFontDescription *font,
                           const AppConfig *cfg, const BuiltMessage *msg,
                           int content_width) {
    pango_font_description_set_size(font, cfg->font_size * PANGO_SCALE);
    pango_font_description_set_weight(font, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, msg->text, -1);
    pango_layout_set_width(layout, content_width * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    pango_layout_set_single_paragraph_mode(layout, FALSE);
    pango_layout_set_alignment(layout, text_alignment(cfg));
    pango_layout_set_spacing(layout, 2 * PANGO_SCALE);
    PangoAttrList *attrs = message_attrs(msg);
    pango_layout_set_attributes(layout, attrs);
    pango_attr_list_unref(attrs);
}

static cairo_surface_t *cached_image(ImageCache *cache, const char *path) {
    for (size_t i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].path, path) == 0) return cache->entries[i].surface;
    }
    cairo_surface_t *surface = chathud_image_load(path);
    if (!surface) return NULL;
    if (cache->count >= IMAGE_CACHE_MAX) {
        cairo_surface_destroy(cache->entries[0].surface);
        memmove(&cache->entries[0], &cache->entries[1], sizeof(cache->entries[0]) * (IMAGE_CACHE_MAX - 1));
        cache->count = IMAGE_CACHE_MAX - 1;
    }
    ImageCacheEntry *e = &cache->entries[cache->count++];
    snprintf(e->path, sizeof(e->path), "%s", path);
    e->surface = surface;
    return surface;
}

static void draw_emotes(cairo_t *cr, PangoLayout *layout, const BuiltMessage *msg,
                        ImageCache *cache, int base_x, int base_y, int emote_size) {
    for (size_t i = 0; i < msg->emote_count; i++) {
        const EmotePlacement *ep = &msg->emotes[i];
        cairo_surface_t *surface = cached_image(cache, ep->path);
        if (!surface) continue;
        int sw = cairo_image_surface_get_width(surface);
        int sh = cairo_image_surface_get_height(surface);
        if (sw <= 0 || sh <= 0) continue;

        PangoRectangle pos;
        pango_layout_index_to_pos(layout, ep->byte_index, &pos);
        double x = base_x + (double)pos.x / PANGO_SCALE;
        double line_y = base_y + (double)pos.y / PANGO_SCALE;
        double line_h = (double)pos.height / PANGO_SCALE;
        double target_h = emote_size;
        double target_w = target_h * ((double)sw / (double)sh);
        double y = line_y + (line_h - target_h) / 2.0;

        cairo_save(cr);
        cairo_translate(cr, x, y);
        cairo_scale(cr, target_w / sw, target_h / sh);
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }
}

static int measure_message(PangoLayout *layout, PangoFontDescription *font,
                           const AppConfig *cfg, const ChatMessage *m,
                           int content_width, BuiltMessage *built) {
    build_message(m, built);
    prepare_layout(layout, font, cfg, built, content_width);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    int min_h = cfg->font_size + 6;
    if (h < min_h) h = min_h;
    return h;
}

static void draw_layout_with_shadow(cairo_t *cr, PangoLayout *layout, int x, int y) {
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.82);
    cairo_move_to(cr, x + 1.5, y + 1.5);
    pango_cairo_show_layout(cr, layout);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
}

static void render_overlay(cairo_t *cr, PangoLayout *layout, PangoFontDescription *font,
                           const AppConfig *cfg, ChatState *state,
                           int screen_w, int screen_h, ImageCache *image_cache,
                           RenderWorkspace *work) {
    ChatMessage *snapshot = work->snapshot;
    BuiltMessage *built = work->built;
    int *heights = work->heights;
    size_t count;
    char title[CHATHUD_TITLE_MAX];
    char status[CHATHUD_STATUS_MAX];
    int has_error;

    pthread_mutex_lock(&state->mutex);
    count = state->count;
    if (count > CHATHUD_MAX_MESSAGES) count = CHATHUD_MAX_MESSAGES;
    memcpy(snapshot, state->messages, sizeof(ChatMessage) * count);
    snprintf(title, sizeof(title), "%s", state->title);
    snprintf(status, sizeof(status), "%s", state->status);
    has_error = state->has_error;
    pthread_mutex_unlock(&state->mutex);

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

    const int pad = 12;
    const int gap = 5;
    int panel_w = cfg->width;
    if (panel_w > screen_w - 2 * cfg->margin) panel_w = screen_w - 2 * cfg->margin;
    if (panel_w < 160) panel_w = 160;
    int content_w = panel_w - 2 * pad;

    int header_h = 0;
    char header[CHATHUD_TITLE_MAX + CHATHUD_STATUS_MAX + 8];
    header[0] = '\0';
    if (cfg->show_header) {
        if (has_error && status[0]) snprintf(header, sizeof(header), "%s", status);
        else if (title[0]) snprintf(header, sizeof(header), "%s", title);
        else if (status[0]) snprintf(header, sizeof(header), "%s", status);
        else snprintf(header, sizeof(header), "ChatHUD");
        header_h = cfg->header_font_size + 12;
    }

    size_t visible = count;
    if ((int)visible > cfg->lines) visible = (size_t)cfg->lines;
    size_t first = count > visible ? count - visible : 0;

    memset(heights, 0, sizeof(work->heights));
    int messages_h = 0;
    for (size_t i = 0; i < visible; i++) {
        size_t idx = first + i;
        heights[i] = measure_message(layout, font, cfg, &snapshot[idx], content_w, &built[i]);
        messages_h += heights[i];
        if (i + 1 < visible) messages_h += gap;
    }

    int panel_h = pad * 2 + header_h + messages_h;
    if (visible == 0) panel_h += cfg->font_size + 8;

    int max_h = screen_h - 2 * cfg->margin;
    while (visible > 1 && panel_h > max_h) {
        panel_h -= heights[0] + gap;
        memmove(&built[0], &built[1], sizeof(built[0]) * (visible - 1));
        memmove(&heights[0], &heights[1], sizeof(heights[0]) * (visible - 1));
        first++;
        visible--;
    }
    if (panel_h > max_h) panel_h = max_h;

    int px = anchor_x(cfg, screen_w, panel_w);
    int py = anchor_y(cfg, screen_h, panel_h);

    if (cfg->opacity > 0.0) {
        round_rect(cr, px, py, panel_w, panel_h, 12.0);
        cairo_set_source_rgba(cr, 0.03, 0.03, 0.03, cfg->opacity);
        cairo_fill(cr);
    }

    int y = py + pad;
    if (cfg->show_header) {
        pango_font_description_set_size(font, cfg->header_font_size * PANGO_SCALE);
        pango_font_description_set_weight(font, PANGO_WEIGHT_MEDIUM);
        pango_layout_set_font_description(layout, font);
        pango_layout_set_attributes(layout, NULL);
        pango_layout_set_text(layout, header, -1);
        pango_layout_set_width(layout, content_w * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_layout_set_single_paragraph_mode(layout, TRUE);
        pango_layout_set_alignment(layout, text_alignment(cfg));
        if (has_error) cairo_set_source_rgba(cr, 1.0, 0.38, 0.38, 1.0);
        else cairo_set_source_rgba(cr, 0.78, 0.88, 1.0, 1.0);
        cairo_move_to(cr, px + pad, y);
        pango_cairo_show_layout(cr, layout);
        y += header_h;
    }

    if (visible == 0) {
        pango_font_description_set_size(font, cfg->font_size * PANGO_SCALE);
        pango_font_description_set_weight(font, PANGO_WEIGHT_NORMAL);
        pango_layout_set_font_description(layout, font);
        pango_layout_set_attributes(layout, NULL);
        pango_layout_set_text(layout, "Czekam na wiadomości…", -1);
        pango_layout_set_width(layout, content_w * PANGO_SCALE);
        pango_layout_set_alignment(layout, text_alignment(cfg));
        draw_layout_with_shadow(cr, layout, px + pad, y);
    } else {
        int newest_first = strcmp(cfg->message_order, "newest-first") == 0;
        for (size_t slot = 0; slot < visible; slot++) {
            size_t bi = newest_first ? (visible - 1 - slot) : slot;
            prepare_layout(layout, font, cfg, &built[bi], content_w);
            draw_layout_with_shadow(cr, layout, px + pad, y);
            draw_emotes(cr, layout, &built[bi], image_cache, px + pad, y, cfg->emote_size);
            y += heights[bi] + gap;
            if (y > py + panel_h - pad) break;
        }
    }

    cairo_surface_flush(cairo_get_target(cr));
}

static int watched_process_alive(pid_t pid) {
    if (pid <= 0) return 1;
    if (kill(pid, 0) == 0) return 1;
    return errno == EPERM;
}

int overlay_run(const OverlayArgs *args) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ChatHUD: nie mogę otworzyć DISPLAY.\n");
        fprintf(stderr, "Jeśli jesteś na Waylandzie, upewnij się, że działa XWayland.\n");
        return 2;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
        fprintf(stderr, "ChatHUD: brak 32-bitowego visuala z alpha.\n");
        XCloseDisplay(dpy);
        return 2;
    }

    Colormap cmap = XCreateColormap(dpy, root, vinfo.visual, AllocNone);
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.colormap = cmap;
    attrs.border_pixel = 0;
    attrs.background_pixel = 0;
    attrs.override_redirect = True;
    attrs.event_mask = ExposureMask | StructureNotifyMask;

    /* Pełnoekranowe, całkowicie przezroczyste i click-through okno.
       Sam panel jest rysowany tylko w wybranym rogu/środku. Dzięki temu
       może zmieniać wysokość gdy wiadomości się zawijają bez XMoveResize. */
    Window win = XCreateWindow(dpy, root, 0, 0, (unsigned)screen_w, (unsigned)screen_h,
                               0, vinfo.depth, InputOutput, vinfo.visual,
                               CWColormap | CWBorderPixel | CWBackPixel |
                               CWOverrideRedirect | CWEventMask, &attrs);

    const char *states[] = {
        "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_STICKY",
        "_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_STATE_SKIP_PAGER"
    };
    set_atom_list(dpy, win, "_NET_WM_STATE", states, sizeof(states) / sizeof(states[0]));
    const char *types[] = {"_NET_WM_WINDOW_TYPE_NOTIFICATION"};
    set_atom_list(dpy, win, "_NET_WM_WINDOW_TYPE", types, 1);

    XserverRegion empty = XFixesCreateRegion(dpy, NULL, 0);
    XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, empty);
    XFixesDestroyRegion(dpy, empty);

    XMapRaised(dpy, win);
    XFlush(dpy);

    cairo_surface_t *surface = cairo_xlib_surface_create(dpy, win, vinfo.visual, screen_w, screen_h);
    cairo_t *cr = cairo_create(surface);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string("Sans");
    ImageCache *image_cache = calloc(1, sizeof(*image_cache));
    RenderWorkspace *work = calloc(1, sizeof(*work));
    if (!image_cache || !work) {
        fprintf(stderr, "ChatHUD: brak pamięci dla buforów renderera.\n");
        free(image_cache);
        free(work);
        pango_font_description_free(font);
        g_object_unref(layout);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        XDestroyWindow(dpy, win);
        XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        return 2;
    }

    while (!chat_state_should_stop(args->state)) {
        while (XPending(dpy)) { XEvent ev; XNextEvent(dpy, &ev); }
        render_overlay(cr, layout, font, args->config, args->state,
                       screen_w, screen_h, image_cache, work);
        XFlush(dpy);
        if (args->watch_pid > 0 && !watched_process_alive(args->watch_pid)) break;
        usleep((useconds_t)args->config->refresh_ms * 1000);
    }

    for (size_t i = 0; i < image_cache->count; i++)
        cairo_surface_destroy(image_cache->entries[i].surface);
    free(image_cache);
    free(work);
    pango_font_description_free(font);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XDestroyWindow(dpy, win);
    XFreeColormap(dpy, cmap);
    XCloseDisplay(dpy);
    return 0;
}
