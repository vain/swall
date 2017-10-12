#include <Imlib2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

#define __NAME__ "swall"

struct Monitor
{
    int x, y;
    unsigned int width, height;
};

Display *dpy;
Window root;
int screen;
unsigned int root_w, root_h;
struct Monitor *monitors = NULL;
size_t num_mons = 0;

int
monitors_compare(const void *a, const void *b)
{
    const struct Monitor *ma, *mb;

    ma = (struct Monitor *)a;
    mb = (struct Monitor *)b;

    /* Sort monitors lemonbar/neeasade style */

    if (ma->x < mb->x || (unsigned int)ma->y + ma->height <= (unsigned int)mb->y)
        return -1;

    if (ma->x > mb->x || (unsigned int)ma->y + ma->height > (unsigned int)mb->y)
        return 1;

    return 0;
}

bool
read_monitors_is_duplicate(XRRMonitorInfo *allmons, int testmon, bool *chosen,
                           size_t nmon)
{
    size_t i;

    for (i = 0; i < nmon; i++)
    {
        if (chosen[i])
        {
            if (allmons[i].x == allmons[testmon].x &&
                allmons[i].y == allmons[testmon].y &&
                allmons[i].width == allmons[testmon].width &&
                allmons[i].height == allmons[testmon].height)
                return true;
        }
    }

    return false;
}

bool
read_monitors(void)
{
    XRRMonitorInfo *moninf;
    int nmon;
    size_t i, mon_i, nmon_st;
    bool *chosen = NULL;

    /* First, we iterate over all monitors and check each monitor if
     * it's not a duplicate. If it's okay, we mark it for use. After
     * this loop, we know how many usable monitors there are, so we can
     * allocate the "monitors" array. */

    moninf = XRRGetMonitors(dpy, root, True, &nmon);
    if (nmon <= 0 || moninf == NULL)
    {
        fprintf(stderr, __NAME__": No XRandR screens found\n");
        return false;
    }
    nmon_st = nmon;

    chosen = calloc(nmon_st, sizeof (bool));
    if (chosen == NULL)
    {
        fprintf(stderr, __NAME__": Could not allocate memory for pointer "
                "chosen\n");
        XRRFreeMonitors(moninf);
        return false;
    }

    for (num_mons = 0, i = 0; i < nmon_st; i++)
    {
        if (!read_monitors_is_duplicate(moninf, i, chosen, nmon_st))
        {
            chosen[i] = true;
            num_mons++;
        }
    }

    /* Copy result to our own array because I'm not sure if we can work
     * freely on the array returned by the lib. */
    monitors = calloc(num_mons, sizeof (struct Monitor));
    if (monitors == NULL)
    {
        fprintf(stderr, __NAME__": Cannot allocate memory for 'monitors': ");
        perror(NULL);
        XRRFreeMonitors(moninf);
        return false;
    }
    for (mon_i = 0, i = 0; i < num_mons; i++)
    {
        if (chosen[i])
        {
            monitors[mon_i].x = moninf[i].x;
            monitors[mon_i].y = moninf[i].y;
            monitors[mon_i].width = moninf[i].width;
            monitors[mon_i].height = moninf[i].height;

            mon_i++;
        }
    }
    XRRFreeMonitors(moninf);

    qsort(monitors, num_mons, sizeof (struct Monitor), monitors_compare);

    return true;
}

void
use_image_as_wallpaper(Imlib_Image image)
{
    Pixmap pm;

    imlib_context_set_image(image);
    pm = XCreatePixmap(dpy, root,
                       imlib_image_get_width(),
                       imlib_image_get_height(),
                       DefaultDepth(dpy, screen));

    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisual(dpy, screen));
    imlib_context_set_colormap(DefaultColormap(dpy, screen));
    imlib_context_set_drawable(pm);
    imlib_render_image_on_drawable(0, 0);

    XSetWindowBackgroundPixmap(dpy, root, pm);
    XClearWindow(dpy, root);

    XFreePixmap(dpy, pm);
}

void
compose(char **paths, size_t num_paths)
{
    int orig_w, orig_h, src_x, src_y, src_w, src_h, to_edge_x, to_edge_y;
    unsigned int x, y;
    size_t i, path_i;
    double source_aspect, target_aspect;
    Imlib_Image image, canvas;
    bool larger;

    canvas = imlib_create_image(root_w, root_h);
    if (canvas == NULL)
    {
        fprintf(stderr, __NAME__": Cannot create canvas root image\n");
        return;
    }

    for (i = 0, path_i = 0; i < num_mons; i++)
    {
        image = imlib_load_image(paths[path_i]);
        if (image == NULL)
        {
            fprintf(stderr, __NAME__": Cannot load image '%s'\n", paths[path_i]);
            goto free_canvas_and_leave;
        }

        printf(__NAME__": Monitor %zu uses image %zu: '%s' ", i, path_i,
               paths[path_i]);

        imlib_context_set_image(image);
        orig_w = imlib_image_get_width();
        orig_h = imlib_image_get_height();
        imlib_context_set_image(canvas);

        if ((unsigned int)orig_w == monitors[i].width &&
            (unsigned int)orig_h == monitors[i].height)
        {
            printf("(exact size match)\n");

            src_x = 0;
            src_y = 0;
            src_w = orig_w;
            src_h = orig_h;

            imlib_blend_image_onto_image(image, 0,
                                         src_x, src_y,
                                         src_w, src_h,
                                         monitors[i].x, monitors[i].y,
                                         monitors[i].width, monitors[i].height);
        }
        else if ((unsigned int)orig_w < 0.6 * monitors[i].width &&
                 (unsigned int)orig_h < 0.6 * monitors[i].height)
        {
            printf("(tiled)\n");

            for (y = 0; y < monitors[i].height; y += orig_h)
            {
                for (x = 0; x < monitors[i].width; x += orig_w)
                {
                    to_edge_x = monitors[i].width - x;
                    to_edge_y = monitors[i].height - y;

                    src_w = to_edge_x >= orig_w ? orig_w : to_edge_x;
                    src_h = to_edge_y >= orig_h ? orig_h : to_edge_y;

                    imlib_blend_image_onto_image(image, 0,
                                                 0, 0,
                                                 src_w, src_h,
                                                 monitors[i].x + x, monitors[i].y + y,
                                                 src_w, src_h);
                }
            }
        }
        else
        {
            /* Fill this monitor as best as you can.
             *
             * For larger images, crop width or height and then resize
             * to achieve the monitor's aspect ratio.
             *
             * Smaller images will be increased in size and black bars
             * will be introduced to achieve the monitor's aspect ratio.
             * This won't look pretty but it will look okay for images
             * that are just slightly smaller than the monitor. */

            if ((unsigned int)orig_w >= monitors[i].width &&
                (unsigned int)orig_h >= monitors[i].height)
                larger = true;
            else
                larger = false;

            source_aspect = (double)orig_w / orig_h;
            target_aspect = (double)monitors[i].width / monitors[i].height;

            if ((larger && source_aspect > target_aspect) ||
                (!larger && source_aspect < target_aspect))
            {
                printf("(fill area, use full height)\n");

                src_y = 0;
                src_h = orig_h;

                src_w = orig_h * target_aspect;
                src_x = (orig_w - src_w) * 0.5;
            }
            else
            {
                printf("(fill area, use full width)\n");

                src_x = 0;
                src_w = orig_w;

                src_h = orig_w / target_aspect;
                src_y = (orig_h - src_h) * 0.5;
            }

            imlib_blend_image_onto_image(image, 0,
                                         src_x, src_y,
                                         src_w, src_h,
                                         monitors[i].x, monitors[i].y,
                                         monitors[i].width, monitors[i].height);
        }

        /* What an awkward API. */
        imlib_context_set_image(image);
        imlib_free_image();

        if (path_i < num_paths - 1)
            path_i++;
    }

    use_image_as_wallpaper(canvas);

free_canvas_and_leave:
    imlib_context_set_image(canvas);
    imlib_free_image_and_decache();
}

int
main(int argc, char **argv)
{
    Window dw;
    int screen, di;
    unsigned int dui;

    if (argc < 2)
    {
        fprintf(stderr, __NAME__": Usage: %s <image> [<image>...]\n", argv[0]);
        return 1;
    }

    dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, __NAME__": Cannot open display\n");
        return 1;
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    XGetGeometry(dpy, root, &dw, &di, &di, &root_w, &root_h, &dui, &dui);
    if (!read_monitors() || num_mons == 0)
    {
        fprintf(stderr, __NAME__": No monitors\n");
        return 1;
    }

    compose(++argv, argc - 1);

    XFlush(dpy);
    XCloseDisplay(dpy);

    return 0;
}
