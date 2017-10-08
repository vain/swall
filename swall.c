#include <Imlib2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

#define __NAME__ "swall"
#define MAX_MONS 16

struct Monitor
{
    int x, y;
    unsigned int width, height;
};

Display *dpy;
Window root;
int screen;
unsigned int root_w, root_h;
struct Monitor monitors[MAX_MONS] = {0};
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
read_monitors(void)
{
    XRRCrtcInfo *ci;
    XRRScreenResources *sr;
    int c;

    sr = XRRGetScreenResources(dpy, root);
    if (sr->ncrtc <= 0)
    {
        fprintf(stderr, __NAME__": No XRandR screens found\n");
        return false;
    }

    num_mons = 0;
    for (c = 0; c < sr->ncrtc && num_mons < MAX_MONS; c++)
    {
        ci = XRRGetCrtcInfo(dpy, sr, sr->crtcs[c]);
        if (ci == NULL || ci->noutput == 0 || ci->mode == None)
            continue;

        monitors[num_mons].x = ci->x;
        monitors[num_mons].y = ci->y;
        monitors[num_mons].width = ci->width;
        monitors[num_mons].height = ci->height;

        num_mons++;
    }
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
compose(char **paths, size_t num_paths, bool single)
{
    int orig_w, orig_h, src_x, src_y, src_w, src_h;
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
        if (path_i >= num_paths)
        {
            fprintf(stderr, __NAME__": Not enough images\n");
            goto errout;
        }

        image = imlib_load_image(paths[path_i]);
        if (image == NULL)
        {
            fprintf(stderr, __NAME__": Cannot load image '%s'\n", paths[path_i]);
            goto errout;
        }

        imlib_context_set_image(image);
        orig_w = imlib_image_get_width();
        orig_h = imlib_image_get_height();

        if ((unsigned int)orig_w == monitors[i].width &&
            (unsigned int)orig_h == monitors[i].height)
        {
            printf(__NAME__": Exact size match of '%s' for monitor %zu\n",
                   paths[path_i], i);

            src_x = 0;
            src_y = 0;
            src_w = orig_w;
            src_h = orig_h;
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
                printf(__NAME__": Using full height of '%s' for monitor %zu\n",
                       paths[path_i], i);

                src_y = 0;
                src_h = orig_h;

                src_w = orig_h * target_aspect;
                src_x = (orig_w - src_w) * 0.5;
            }
            else
            {
                printf(__NAME__": Using full width of '%s' for monitor %zu\n",
                       paths[path_i], i);

                src_x = 0;
                src_w = orig_w;

                src_h = orig_w / target_aspect;
                src_y = (orig_h - src_h) * 0.5;
            }
        }

        imlib_context_set_image(canvas);
        imlib_blend_image_onto_image(image, 0,
                                     src_x, src_y,
                                     src_w, src_h,
                                     monitors[i].x, monitors[i].y,
                                     monitors[i].width, monitors[i].height);

        /* What an awkward API. */
        imlib_context_set_image(image);
        imlib_free_image();

        if (!single)
            path_i++;
    }

    use_image_as_wallpaper(canvas);

errout:
    imlib_context_set_image(canvas);
    imlib_free_image_and_decache();
}

bool
tile(char *path)
{
    Imlib_Image image;
    bool use_as_tile = true;
    size_t i;

    image = imlib_load_image(path);
    if (image == NULL)
    {
        fprintf(stderr, __NAME__": Cannot load image '%s'\n", path);
        use_as_tile = false;
    }
    else
    {
        imlib_context_set_image(image);
        for (i = 0; use_as_tile && i < num_mons; i++)
        {
            if ((unsigned int)imlib_image_get_width() > monitors[i].width ||
                (unsigned int)imlib_image_get_height() > monitors[i].height)
            {
                use_as_tile = false;
            }
        }

        if (use_as_tile)
        {
            printf(__NAME__": Tiling '%s'\n", path);
            use_image_as_wallpaper(image);
        }
        imlib_free_image();
    }
    return use_as_tile;
}

int
main(int argc, char **argv)
{
    Window dw;
    int screen, di;
    unsigned int dui;

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

    /* If only one image is specified, see if it's smaller than all of
     * the monitors. If so, it will be used as a tile. If not, it will
     * be scaled to each screen's size individually and used as a single
     * wallpaper. */
    if (argc == 2)
    {
        if (!tile(argv[1]))
            compose(++argv, argc - 1, true);
    }
    else
        compose(++argv, argc - 1, false);

    XFlush(dpy);
    XCloseDisplay(dpy);

    return 0;
}
