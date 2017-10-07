#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

#define __NAME__ "rootfeld"

XImage *
stdin_ff_to_ximage(Display *dpy, int screen, uint32_t *w, uint32_t *h)
{
    uint32_t *ximg_data = NULL, x, y;
    uint32_t hdr[4];
    uint16_t *ffdata = NULL;
    XImage *ximg = NULL;

    if (fread(hdr, sizeof (uint32_t), 4, stdin) != 4)
    {
        fprintf(stderr, __NAME__": Could not read farbfeld from stdin\n");
        return NULL;
    }

    if (memcmp("farbfeld", hdr, (sizeof "farbfeld") - 1) != 0)
    {
        fprintf(stderr, __NAME__": Magic number is not 'farbfeld'\n");
        return NULL;
    }

    *w = ntohl(hdr[2]);
    *h = ntohl(hdr[3]);

    ffdata = calloc(*w * *h * 4, sizeof (uint16_t));
    if (!ffdata)
    {
        fprintf(stderr, __NAME__": Could not allocate memory, ffdata\n");
        return NULL;
    }

    if (fread(ffdata, *w * *h * 4 * sizeof (uint16_t), 1, stdin) != 1)
    {
        fprintf(stderr, __NAME__": Unexpected EOF when reading stdin\n");
        free(ffdata);
        return NULL;
    }

    for (y = 0; y < *h; y++)
        for (x = 0; x < *w; x++)
            ffdata[(y * *w + x) * 4] = ntohs(ffdata[(y * *w + x) * 4]);

    /* Will be freed implicitly by freeing the XImage. */
    ximg_data = calloc(*w * *h, sizeof (uint32_t));
    if (!ximg_data)
    {
        fprintf(stderr, __NAME__": Could not allocate memory, ximg_data\n");
        return NULL;
    }

    for (y = 0; y < *h; y++)
    {
        for (x = 0; x < *w; x++)
        {
            ximg_data[y * *w + x] =
                ((ffdata[(y * *w + x) * 4    ] / 256) << 16) |
                ((ffdata[(y * *w + x) * 4 + 1] / 256) << 8) |
                 (ffdata[(y * *w + x) * 4 + 2] / 256);
        }
    }

    ximg = XCreateImage(dpy, DefaultVisual(dpy, screen), 24, ZPixmap, 0,
                        (char *)ximg_data, *w, *h, 32, 0);

    free(ffdata);
    return ximg;
}

int
main()
{
    Display *dpy;
    int screen;
    Window root;
    XImage *ximg;
    uint32_t w, h;
    GC gc;
    Pixmap pm;
    int err = 0;

    dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, __NAME__": Cannot open display\n");
        return 1;
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    ximg = stdin_ff_to_ximage(dpy, screen, &w, &h);
    if (ximg != NULL)
    {
        pm = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
        gc = XCreateGC(dpy, pm, 0, NULL);
        XPutImage(dpy, pm, gc, ximg,
                  0, 0,
                  0, 0,
                  w, h);
        XDestroyImage(ximg);
        XFreeGC(dpy, gc);

        XSetWindowBackgroundPixmap(dpy, root, pm);
        XClearWindow(dpy, root);
    }
    else
        err = 1;

    XCloseDisplay(dpy);
    return err;
}
