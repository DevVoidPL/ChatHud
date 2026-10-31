#ifndef CHATHUD_MONITOR_H
#define CHATHUD_MONITOR_H

#include <X11/Xlib.h>

#define CHATHUD_MAX_MONITORS 32

typedef struct {
    char name[256];
    int x, y, width, height, primary;
} ChatMonitor;

int monitor_query(Display *dpy, Window root, ChatMonitor *out, int capacity);
ChatMonitor monitor_select(Display *dpy, Window root, const char *name);
int monitor_print_json(void);

#endif
