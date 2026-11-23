#define _POSIX_C_SOURCE 200809L
#include "engine.h"
#include "frontend.h"
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    Display *display;
    Pixmap frame;
    GC gc;
    Visual *visual;
} XRenderer;

static unsigned long channel(unsigned long mask, unsigned value)
{
    unsigned long unit = mask & (0ul - mask);
    if (!unit) return 0;
    return (unsigned long)(((uint64_t)value * (mask / unit) + 127) / 255) * unit;
}

static void color(XRenderer *x, uint32_t rgb)
{
    XSetForeground(x->display, x->gc,
                   channel(x->visual->red_mask, (rgb >> 16) & 255) |
                   channel(x->visual->green_mask, (rgb >> 8) & 255) |
                   channel(x->visual->blue_mask, rgb & 255));
}

static void clear(void *context, uint32_t rgb)
{
    XRenderer *x = context;
    color(x, rgb);
    XFillRectangle(x->display, x->frame, x->gc, 0, 0, NX_WIDTH, NX_HEIGHT);
}

static void draw(void *context, const NxEntity *e)
{
    XRenderer *x = context;
    int left = e->x / NX_HZ, top = e->y / NX_HZ;
    if (left >= NX_WIDTH || top >= NX_HEIGHT ||
        left + e->width <= 0 || top + e->height <= 0) return;
    color(x, e->rgb);
    switch (e->shape) {
    case NX_RECT:
        XFillRectangle(x->display, x->frame, x->gc, left, top, e->width, e->height);
        break;
    case NX_OUTLINE:
        XDrawRectangle(x->display, x->frame, x->gc, left, top, e->width - 1, e->height - 1);
        break;
    case NX_OVAL:
        XFillArc(x->display, x->frame, x->gc, left, top, e->width, e->height, 0, 360 * 64);
        break;
    }
}

static enum NxKey translate(KeySym key)
{
    switch (key) {
    case XK_Left: return NX_LEFT;
    case XK_Right: return NX_RIGHT;
    case XK_Up: return NX_UP;
    case XK_Down: return NX_DOWN;
    case XK_space: return NX_SPACE;
    default: return NX_KEYS;
    }
}

static bool poll(Display *display, Atom protocols, Atom close, NxWorld *world)
{
    /* Bound input processing per frame so key repeats cannot starve simulation. */
    for (unsigned i = 0; i < 256 && XPending(display); ++i) {
        XEvent event;
        XNextEvent(display, &event);
        switch (event.type) {
        case ClientMessage:
            if (event.xclient.message_type == protocols && event.xclient.format == 32 &&
                (Atom)event.xclient.data.l[0] == close) return false;
            break;
        case FocusOut:
            for (enum NxKey key = NX_LEFT; key < NX_KEYS; ++key)
                nx_key(world, key, false);
            break;
        case KeyPress:
        case KeyRelease: {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape && event.type == KeyPress) return false;
            nx_key(world, translate(key), event.type == KeyPress);
            break;
        }
        }
    }
    return true;
}

static bool timestamp(int64_t *ns)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return false;
    *ns = (int64_t)time.tv_sec * 1000000000 + time.tv_nsec;
    return true;
}

int nx_window(const NxProgram *program)
{
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fputs("Could not open X11 display\n", stderr);
        return 1;
    }
    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    if (visual->class != TrueColor) {
        fputs("X11 requires a TrueColor visual\n", stderr);
        XCloseDisplay(display);
        return 1;
    }
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        0, 0, NX_WIDTH, NX_HEIGHT, 0, 0, 0);
    XSizeHints hints = {.flags = PMinSize | PMaxSize,
                       .min_width = NX_WIDTH, .max_width = NX_WIDTH,
                       .min_height = NX_HEIGHT, .max_height = NX_HEIGHT};
    XSetWMNormalHints(display, window, &hints);
    Atom protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    Atom close = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &close, 1);
    XStoreName(display, window, "NEXIR 0.7");
    unsigned long pid = (unsigned long)getpid();
    XChangeProperty(display, window, XInternAtom(display, "_NET_WM_PID", False),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
    XSelectInput(display, window, KeyPressMask | KeyReleaseMask | FocusChangeMask);
    XkbSetDetectableAutoRepeat(display, True, NULL);
    XMapWindow(display, window);
    XRenderer x = {display, XCreatePixmap(display, window, NX_WIDTH, NX_HEIGHT,
                                          (unsigned)DefaultDepth(display, screen)),
                   XCreateGC(display, window, 0, NULL), visual};
    NxRenderer renderer = {&x, clear, draw};
    NxWorld world;
    nx_start(&world, program);
    int64_t previous, accumulated = 0;
    bool clock_ok = timestamp(&previous);
    while (clock_ok && poll(display, protocols, close, &world)) {
        int64_t now;
        clock_ok = timestamp(&now);
        if (!clock_ok) break;
        int64_t elapsed = now - previous;
        previous = now;
        if (elapsed > 250000000) elapsed = 250000000;
        accumulated += elapsed * NX_HZ;
        while (accumulated >= 1000000000) {
            nx_step(&world);
            accumulated -= 1000000000;
        }
        nx_render(&world, &renderer);
        XCopyArea(display, x.frame, window, x.gc, 0, 0, NX_WIDTH, NX_HEIGHT, 0, 0);
        XFlush(display);
        nanosleep(&(struct timespec){.tv_nsec = 8000000}, NULL);
    }
    XFreeGC(display, x.gc);
    XFreePixmap(display, x.frame);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    if (!clock_ok) fputs("Could not read monotonic clock\n", stderr);
    return clock_ok ? 0 : 1;
}
