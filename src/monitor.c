#include "monitor.h"

#include <X11/extensions/Xrandr.h>
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

int monitor_query(Display *dpy, Window root, ChatMonitor *out, int capacity) {
    int major = 1, minor = 5;
    if (capacity <= 0 || !XRRQueryVersion(dpy, &major, &minor)) return 0;
    int count = 0;
    if (major > 1 || (major == 1 && minor >= 5)) {
        int available = 0;
        XRRMonitorInfo *monitors = XRRGetMonitors(dpy, root, True, &available);
        for (int i = 0; monitors && i < available && count < capacity; i++) {
            XRRMonitorInfo *m = &monitors[i];
            if (m->width <= 0 || m->height <= 0) continue;
            char *name = XGetAtomName(dpy, m->name);
            ChatMonitor *target = &out[count++];
            memset(target, 0, sizeof(*target));
            snprintf(target->name, sizeof(target->name), "%s", name ? name : "unknown");
            if (name) XFree(name);
            target->x = m->x; target->y = m->y;
            target->width = m->width; target->height = m->height;
            target->primary = m->primary;
        }
        if (monitors) XRRFreeMonitors(monitors);
        if (count) return count;
    }
    /* RandR 1.2/1.4 fallback: use individual active outputs, not the root size. */
    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(dpy, root);
    if (!resources) return 0;
    RROutput primary = XRRGetOutputPrimary(dpy, root);
    for (int i = 0; i < resources->noutput && count < capacity; i++) {
        XRROutputInfo *output = XRRGetOutputInfo(dpy, resources, resources->outputs[i]);
        if (!output) continue;
        if (output->connection == RR_Connected && output->crtc != None) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, resources, output->crtc);
            if (crtc && crtc->width && crtc->height) {
                ChatMonitor *target = &out[count++];
                memset(target, 0, sizeof(*target));
                snprintf(target->name, sizeof(target->name), "%.*s", output->nameLen, output->name);
                target->x = crtc->x; target->y = crtc->y;
                target->width = (int)crtc->width; target->height = (int)crtc->height;
                target->primary = resources->outputs[i] == primary;
            }
            if (crtc) XRRFreeCrtcInfo(crtc);
        }
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(resources);
    return count;
}

ChatMonitor monitor_select(Display *dpy, Window root, const char *name) {
    ChatMonitor monitors[CHATHUD_MAX_MONITORS];
    int count = monitor_query(dpy, root, monitors, CHATHUD_MAX_MONITORS);
    int selected = 0;
    for (int i = 0; i < count; i++) {
        if (name && strcmp(name, "primary") != 0 && strcmp(name, monitors[i].name) == 0)
            return monitors[i];
        if (monitors[i].primary) selected = i;
    }
    if (count) return monitors[selected];
    /* Minimal X servers without RandR expose only a single screen. */
    XWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    XGetWindowAttributes(dpy, root, &attributes);
    ChatMonitor fallback = {.x = 0, .y = 0, .width = attributes.width,
                            .height = attributes.height, .primary = 1};
    snprintf(fallback.name, sizeof(fallback.name), "screen");
    if (fallback.width < 1) fallback.width = 1;
    if (fallback.height < 1) fallback.height = 1;
    return fallback;
}

int monitor_print_json(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ChatHUD: nie mogę odczytać monitorów X11/XWayland (DISPLAY).\n");
        return 2;
    }
    ChatMonitor monitors[CHATHUD_MAX_MONITORS];
    int count = monitor_query(dpy, DefaultRootWindow(dpy), monitors, CHATHUD_MAX_MONITORS);
    if (!count) {
        monitors[0] = monitor_select(dpy, DefaultRootWindow(dpy), "primary");
        count = 1;
    }
    json_object *list = json_object_new_array();
    if (!list) { XCloseDisplay(dpy); return 1; }
    for (int i = 0; i < count; i++) {
        ChatMonitor *m = &monitors[i];
        json_object *item = json_object_new_object();
        json_object_object_add(item, "name", json_object_new_string(m->name));
        json_object_object_add(item, "x", json_object_new_int(m->x));
        json_object_object_add(item, "y", json_object_new_int(m->y));
        json_object_object_add(item, "width", json_object_new_int(m->width));
        json_object_object_add(item, "height", json_object_new_int(m->height));
        json_object_object_add(item, "primary", json_object_new_boolean(m->primary));
        json_object_array_add(list, item);
    }
    puts(json_object_to_json_string_ext(list, JSON_C_TO_STRING_PLAIN));
    json_object_put(list);
    XCloseDisplay(dpy);
    return 0;
}
