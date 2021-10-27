/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* X Windows driver initialization/finalization */
#include "memory_.h"
#include "x_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gsparamx.h"
#include "gdevbbox.h"
#include "gdevx.h"

extern char *getenv(const char *);

extern const gx_device_bbox gs_bbox_device;
extern const gx_device_X gs_x11_device;

extern const gx_device_bbox_procs_t gdev_x_box_procs;

/* Define constants for orientation from ghostview */
/* Number represents clockwise rotation of the paper in degrees */
typedef enum {
    Portrait = 0,		/* Normal portrait orientation */
    Landscape = 90,		/* Normal landscape orientation */
    Upsidedown = 180,		/* Don't think this will be used much */
    Seascape = 270		/* Landscape rotated the wrong way */
} orientation;

/* ---------------- Opening/initialization ---------------- */

/* Forward references */
static void x_get_work_area(gx_device_X *xdev, int *pwidth, int *pheight);
static long *x_get_win_property(gx_device_X *xdev, const char *atom_name);

/* Catch the alloc error when there is not enough resources for the
 * backing pixmap.  Automatically shut off backing pixmap and let the
 * user know when this happens.
 *
 * Because the X API was designed without adequate thought to reentrancy,
 * these variables must be allocated statically.  We do not see how this
 * code can work reliably in the presence of multi-threading.
 */
static struct xv_ {
    Boolean alloc_error;
    XErrorHandler orighandler;
    XErrorHandler oldhandler;
    Boolean set;
} x_error_handler = {0};

static int
x_catch_alloc(Display * dpy, XErrorEvent * err)
{
    if (err->error_code == BadAlloc)
        x_error_handler.alloc_error = True;
    if (x_error_handler.alloc_error)
        return 0;
    return x_error_handler.oldhandler(dpy, err);
}

int
x_catch_free_colors(Display * dpy, XErrorEvent * err)
{
    if (err->request_code == X_FreeColors ||
        x_error_handler.orighandler == x_catch_free_colors)
        return 0;
    return x_error_handler.orighandler(dpy, err);
}

/* Open the X device */
int
gdev_x_open(gx_device_X * xdev)
{
    XSizeHints sizehints;
    char *window_id;
    XEvent event;
    XVisualInfo xvinfo;
    int nitems;
    XtAppContext app_con;
    Widget toplevel;
    Display *dpy;
    XColor xc;
    int zero = 0;
    int xid_height = 0, xid_width = 0;
    int code;

#ifdef DEBUG
# ifdef have_Xdebug
    if (gs_debug['X']) {
        extern int _Xdebug;

        _Xdebug = 1;
    }
# endif
#endif

    if (!(xdev->dpy = XOpenDisplay((char *)NULL))) {
        char *dispname = getenv("DISPLAY");

        emprintf1(xdev->memory,
            "Cannot open X display `%s'.\n",
            (dispname == NULL ? "(null)" : dispname));
        return_error(gs_error_ioerror);
    }

    xdev->dest = 0;
    if ((window_id = getenv("GHOSTVIEW"))) {
        if (!(xdev->ghostview = sscanf(window_id, "%ld %ld",
                                       &(xdev->win), &(xdev->dest)))) {
            emprintf(xdev->memory, "Cannot get Window ID from ghostview.\n");
            return_error(gs_error_ioerror);
        }
    }
    if (xdev->pwin != (Window) None) {	/* pick up the destination window parameters if specified */
        XWindowAttributes attrib;

        xdev->win = xdev->pwin;
        if (XGetWindowAttributes(xdev->dpy, xdev->win, &attrib)) {
            xdev->scr = attrib.screen;
            xvinfo.visual = attrib.visual;
            xdev->cmap = attrib.colormap;
            xid_width = attrib.width;
            xid_height = attrib.height;
        } else {
            /* No idea why we can't get the attributes, but */
            /* we shouldn't let it lead to a failure below. */
            xid_width = xid_height = 0;
        }
    } else if (xdev->ghostview) {
        XWindowAttributes attrib;
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        char *buf;
        Atom ghostview_atom = XInternAtom(xdev->dpy, "GHOSTVIEW", False);

        if (XGetWindowAttributes(xdev->dpy, xdev->win, &attrib)) {
            xdev->scr = attrib.screen;
            xvinfo.visual = attrib.visual;
            xdev->cmap = attrib.colormap;
            xdev->width = attrib.width;
            xdev->height = attrib.height;
        }
        /* Delete property if explicit dest is given */
        if (XGetWindowProperty(xdev->dpy, xdev->win, ghostview_atom, 0,
                               256, (xdev->dest != 0), XA_STRING,
                               &type, &format, &nitems, &bytes_after,
                               (unsigned char **)&buf) == 0 &&
            type == XA_STRING) {
            int llx, lly, urx, ury;
            int left_margin = 0, bottom_margin = 0;
            int right_margin = 0, top_margin = 0;

            /* We declare page_orientation as an int so that we can */
            /* use an int * to reference it for sscanf; compilers */
            /* might be tempted to use less space to hold it if */
            /* it were declared as an orientation. */
            int /*orientation */ page_orientation;
            float xppp, yppp;	/* pixels per point */

            nitems = sscanf(buf,
                            "%ld %d %d %d %d %d %f %f %d %d %d %d",
                            &(xdev->bpixmap), &page_orientation,
                            &llx, &lly, &urx, &ury,
                            &(xdev->x_pixels_per_inch),
                            &(xdev->y_pixels_per_inch),
                            &left_margin, &bottom_margin,
                            &right_margin, &top_margin);
            if (!(nitems == 8 || nitems == 12)) {
                emprintf(xdev->memory, "Cannot get ghostview property.\n");
                return_error(gs_error_ioerror);
            }
            if (xdev->dest && xdev->bpixmap) {
                emprintf(xdev->memory,
                         "Both destination and backing pixmap specified.\n");
                return_error(gs_error_rangecheck);
            }
            if (xdev->dest) {
                Window root;
                int x, y;
                unsigned int width, height;
                unsigned int border_width, depth;

                if (XGetGeometry(xdev->dpy, xdev->dest, &root, &x, &y,
                                 &width, &height, &border_width, &depth)) {
                    xdev->width = width;
                    xdev->height = height;
                }
            }
            xppp = xdev->x_pixels_per_inch / 72.0;
            yppp = xdev->y_pixels_per_inch / 72.0;
            switch (page_orientation) {
                case Portrait:
                    xdev->initial_matrix.xx = xppp;
                    xdev->initial_matrix.xy = 0.0;
                    xdev->initial_matrix.yx = 0.0;
                    xdev->initial_matrix.yy = -yppp;
                    xdev->initial_matrix.tx = -llx * xppp;
                    xdev->initial_matrix.ty = ury * yppp;
                    break;
                case Landscape:
                    xdev->initial_matrix.xx = 0.0;
                    xdev->initial_matrix.xy = yppp;
                    xdev->initial_matrix.yx = xppp;
                    xdev->initial_matrix.yy = 0.0;
                    xdev->initial_matrix.tx = -lly * xppp;
                    xdev->initial_matrix.ty = -llx * yppp;
                    break;
                case Upsidedown:
                    xdev->initial_matrix.xx = -xppp;
                    xdev->initial_matrix.xy = 0.0;
                    xdev->initial_matrix.yx = 0.0;
                    xdev->initial_matrix.yy = yppp;
                    xdev->initial_matrix.tx = urx * xppp;
                    xdev->initial_matrix.ty = -lly * yppp;
                    break;
                case Seascape:
                    xdev->initial_matrix.xx = 0.0;
                    xdev->initial_matrix.xy = -yppp;
                    xdev->initial_matrix.yx = -xppp;
                    xdev->initial_matrix.yy = 0.0;
                    xdev->initial_matrix.tx = ury * xppp;
                    xdev->initial_matrix.ty = urx * yppp;
                    break;
            }

            /* The following sets the imageable area according to the */
            /* bounding box and margins sent by ghostview.            */
            /* This code has been patched many times; its current state */
            /* is per a recommendation by Tim Theisen on 4/28/95. */

            xdev->ImagingBBox[0] = llx - left_margin;
            xdev->ImagingBBox[1] = lly - bottom_margin;
            xdev->ImagingBBox[2] = urx + right_margin;
            xdev->ImagingBBox[3] = ury + top_margin;
            xdev->ImagingBBox_set = true;

        } else if (xdev->pwin == (Window) None) {
            emprintf(xdev->memory, "Cannot get ghostview property.\n");
            return_error(gs_error_ioerror);
        }
    } else {
        Screen *scr = DefaultScreenOfDisplay(xdev->dpy);

        xdev->scr = scr;
        xvinfo.visual = DefaultVisualOfScreen(scr);
        xdev->cmap = DefaultColormapOfScreen(scr);
        if (xvinfo.visual->class != TrueColor) {
            int scrno = DefaultScreen(xdev->dpy);
            if ( XMatchVisualInfo(xdev->dpy, scrno, 24, TrueColor, &xvinfo) ||
                 XMatchVisualInfo(xdev->dpy, scrno, 32, TrueColor, &xvinfo) ||
                 XMatchVisualInfo(xdev->dpy, scrno, 16, TrueColor, &xvinfo) ||
                 XMatchVisualInfo(xdev->dpy, scrno, 15, TrueColor, &xvinfo)  ) {
                xdev->cmap = XCreateColormap (xdev->dpy,
                                              DefaultRootWindow(xdev->dpy),
                                              xvinfo.visual, AllocNone );
            }
        }
    }
    xvinfo.visualid = XVisualIDFromVisual(xvinfo.visual);
    xdev->vinfo = XGetVisualInfo(xdev->dpy, VisualIDMask, &xvinfo, &nitems);
    if (xdev->vinfo == NULL) {
        emprintf(xdev->memory, "Cannot get XVisualInfo.\n");
        return_error(gs_error_ioerror);
    }
    /* Buggy X servers may cause a Bad Access on XFreeColors. */
    if (!x_error_handler.set) {
        x_error_handler.orighandler = XSetErrorHandler(x_catch_free_colors);
        x_error_handler.set = True;
    }
    /* Get X Resources.  Use the toolkit for this. */
    XtToolkitInitialize();
    app_con = XtCreateApplicationContext();
    XtAppSetFallbackResources(app_con, gdev_x_fallback_resources);
    dpy = XtOpenDisplay(app_con, NULL, "ghostscript", "Ghostscript",
                        NULL, 0, &zero, NULL);
    toplevel = XtAppCreateShell(NULL, "Ghostscript",
                                applicationShellWidgetClass, dpy, NULL, 0);
    XtGetApplicationResources(toplevel, (XtPointer) xdev,
                              gdev_x_resources, gdev_x_resource_count,
                              NULL, 0);

    /* Reserve foreground and background colors under the regular connection. */
    xc.pixel = xdev->foreground;
    XQueryColor(xdev->dpy, DefaultColormap(xdev->dpy,DefaultScreen(xdev->dpy)), &xc);
    XAllocColor(xdev->dpy, xdev->cmap, &xc);
    xdev->foreground = xc.pixel;
    xc.pixel = xdev->background;
    XQueryColor(xdev->dpy, DefaultColormap(xdev->dpy,DefaultScreen(xdev->dpy)), &xc);
    XAllocColor(xdev->dpy, xdev->cmap, &xc);
    xdev->background = xc.pixel;

    code = gdev_x_setup_colors(xdev);
    if (code < 0) {
        XCloseDisplay(xdev->dpy);
        return code;
    }
    /* Now that the color map is setup check if the device is separable. */
    check_device_separable((gx_device *)xdev);

    if (!xdev->ghostview) {
        XWMHints wm_hints;
        XClassHint class_hint;
        XSetWindowAttributes xswa;
        gx_device *dev = (gx_device *) xdev;

        /* Take care of resolution and paper size. */
        if (xdev->x_pixels_per_inch == FAKE_RES ||
            xdev->y_pixels_per_inch == FAKE_RES) {
            float xsize = (float)xdev->width / xdev->x_pixels_per_inch;
            float ysize = (float)xdev->height / xdev->y_pixels_per_inch;
            int workarea_width = WidthOfScreen(xdev->scr), workarea_height = HeightOfScreen(xdev->scr);

            /* Get area available for windows placement */
            x_get_work_area(xdev, &workarea_width, &workarea_height);

            if (xdev->xResolution == 0.0 && xdev->yResolution == 0.0) {
                float dpi, xdpi, ydpi;

                xdpi = 25.4 * WidthOfScreen(xdev->scr) /
                    WidthMMOfScreen(xdev->scr);
                ydpi = 25.4 * HeightOfScreen(xdev->scr) /
                    HeightMMOfScreen(xdev->scr);
                dpi = min(xdpi, ydpi);
                /*
                 * Some X servers provide very large "virtual screens", and
                 * return the virtual screen size for Width/HeightMM but the
                 * physical size for Width/Height.  Attempt to detect and
                 * correct for this now.  This is a KLUDGE required because
                 * the X server provides no way to read the screen
                 * resolution directly.
                 */
                if (dpi < 30)
                    dpi = 75;	/* arbitrary */
                else {
                    while (xsize * dpi > WidthOfScreen(xdev->scr) - 32 ||
                           ysize * dpi > HeightOfScreen(xdev->scr) - 32)
                        dpi *= 0.95;
                }
                xdev->x_pixels_per_inch = dpi;
                xdev->y_pixels_per_inch = dpi;
            } else {
                xdev->x_pixels_per_inch = xdev->xResolution;
                xdev->y_pixels_per_inch = xdev->yResolution;
            }
            /* Restrict maximum window size to available work area */
            if (xdev->width > workarea_width) {
                xdev->width = min(xsize * xdev->x_pixels_per_inch, workarea_width);
            }
            if (xdev->height > workarea_height) {
                xdev->height = min(ysize * xdev->y_pixels_per_inch, workarea_height);
            }
            xdev->MediaSize[0] =
                (float)xdev->width / xdev->x_pixels_per_inch * 72;
            xdev->MediaSize[1] =
                (float)xdev->height / xdev->y_pixels_per_inch * 72;
        }

        sizehints.x = 0;
        sizehints.y = 0;
        sizehints.width = xdev->width;
        sizehints.height = xdev->height;
        sizehints.flags = 0;

        if (xdev->geometry != NULL) {
            /*
             * Note that border_width must be set first.  We can't use
             * scr, because that is a Screen*, and XWMGeometry wants
             * the screen number.
             */
            char gstr[40];
            int bitmask;

            gs_sprintf(gstr, "%dx%d+%d+%d", sizehints.width,
                    sizehints.height, sizehints.x, sizehints.y);
            bitmask = XWMGeometry(xdev->dpy, DefaultScreen(xdev->dpy),
                                  xdev->geometry, gstr, xdev->borderWidth,
                                  &sizehints,
                                  &sizehints.x, &sizehints.y,
                                  &sizehints.width, &sizehints.height,
                                  &sizehints.win_gravity);

            if (bitmask & (XValue | YValue))
                sizehints.flags |= USPosition;
        }
        gx_default_get_initial_matrix(dev, &(xdev->initial_matrix));

        if (xdev->pwin != (Window) None && xid_width != 0 && xid_height != 0) {
#if 0				/*************** */

            /*
             * The user who originally implemented the WindowID feature
             * provided the following code to scale the displayed output
             * to fit in the window.  We think this is a bad idea,
             * since it doesn't track window resizing and is generally
             * completely at odds with the way Ghostscript treats
             * window or paper size in all other contexts.  We are
             * leaving the code here in case someone decides that
             * this really is the behavior they want.
             */

            /* Scale to fit in the window. */
            xdev->initial_matrix.xx
                = xdev->initial_matrix.xx *
                (float)xid_width / (float)xdev->width;
            xdev->initial_matrix.yy
                = xdev->initial_matrix.yy *
                (float)xid_height / (float)xdev->height;

#endif /*************** */
            xdev->width = xid_width;
            xdev->height = xid_height;
            xdev->initial_matrix.ty = xdev->height;
        } else {		/* !xdev->pwin */
            xswa.event_mask = ExposureMask;
            xswa.background_pixel = xdev->background;
            xswa.border_pixel = xdev->borderColor;
            xswa.colormap = xdev->cmap;
            xdev->win = XCreateWindow(xdev->dpy, RootWindowOfScreen(xdev->scr),
                                      sizehints.x, sizehints.y,		/* upper left */
                                      xdev->width, xdev->height,
                                      xdev->borderWidth,
                                      xdev->vinfo->depth,
                                      InputOutput,	/* class        */
                                      xdev->vinfo->visual,	/* visual */
                                      CWEventMask | CWBackPixel |
                                      CWBorderPixel | CWColormap,
                                      &xswa);
            XStoreName(xdev->dpy, xdev->win, "ghostscript");
            XSetWMNormalHints(xdev->dpy, xdev->win, &sizehints);
            wm_hints.flags = InputHint;
            wm_hints.input = False;
            XSetWMHints(xdev->dpy, xdev->win, &wm_hints);	/* avoid input focus */
            class_hint.res_name = (char *)"ghostscript";
            class_hint.res_class = (char *)"Ghostscript";
            XSetClassHint(xdev->dpy, xdev->win, &class_hint);
        }
    }
    /***
     *** According to Ricard Torres (torres@upf.es), we have to wait until here
     *** to close the toolkit connection, which we formerly did
     *** just after the calls on XAllocColor above.  I suspect that
     *** this will cause things to be left dangling if an error occurs
     *** anywhere in the above code, but I'm willing to let users
     *** fight over fixing it, since I have no idea what's right.
     ***/

    /* And close the toolkit connection. */
    XtDestroyWidget(toplevel);
    XtCloseDisplay(dpy);
    XtDestroyApplicationContext(app_con);

    xdev->ht.pixmap = (Pixmap) 0;
    xdev->ht.id = gx_no_bitmap_id;;
    xdev->fill_style = FillSolid;
    xdev->function = GXcopy;
    xdev->fid = (Font) 0;

    /* Set up a graphics context */
    xdev->gc = XCreateGC(xdev->dpy, xdev->win, 0, (XGCValues *) NULL);
    XSetFunction(xdev->dpy, xdev->gc, GXcopy);
    XSetLineAttributes(xdev->dpy, xdev->gc, 0,
                       LineSolid, CapButt, JoinMiter);

    gdev_x_clear_window(xdev);

    if (!xdev->ghostview) {	/* Make the window appear. */
        XMapWindow(xdev->dpy, xdev->win);

        /* Before anything else, do a flush and wait for */
        /* an exposure event. */
        XSync(xdev->dpy, False);
        if (xdev->pwin == (Window) None) {	/* there isn't a next event for existing windows */
            XNextEvent(xdev->dpy, &event);
        }
        /* Now turn off graphics exposure events so they don't queue up */
        /* indefinitely.  Also, since we can't do anything about real */
        /* Expose events, mask them out. */
        XSetGraphicsExposures(xdev->dpy, xdev->gc, False);
        XSelectInput(xdev->dpy, xdev->win, NoEventMask);
    } else {
        /* Create an unmapped window, that the window manager will ignore.
         * This invisible window will be used to receive "next page"
         * events from ghostview */
        XSetWindowAttributes attributes;

        attributes.override_redirect = True;
        xdev->mwin = XCreateWindow(xdev->dpy, RootWindowOfScreen(xdev->scr),
                                   0, 0, 1, 1, 0, CopyFromParent,
                                   CopyFromParent, CopyFromParent,
                                   CWOverrideRedirect, &attributes);
        xdev->NEXT = XInternAtom(xdev->dpy, "NEXT", False);
        xdev->PAGE = XInternAtom(xdev->dpy, "PAGE", False);
        xdev->DONE = XInternAtom(xdev->dpy, "DONE", False);
    }

    xdev->ht.no_pixmap = XCreatePixmap(xdev->dpy, xdev->win, 1, 1,
                                       xdev->vinfo->depth);

    return 0;
}

/* Get area available for windows placement */
static void
x_get_work_area(gx_device_X *xdev, int *pwidth, int *pheight)
{
    /* First get work area from window manager if available */
    long *area;

    /* Try to get NET_WORKAREA then WIN_WORKAREA */
    if ((area = x_get_win_property(xdev, "_NET_WORKAREA")) != NULL ||
            (area = x_get_win_property(xdev, "_WIN_WORKAREA")) != NULL) {
        /* Update maximum screen size with usable screen size */
        *pwidth = area[2];
        *pheight = area[3];
        XFree(area);
    }
}

/* Get window property with specified name (should be CARDINAL, four 32-bit words) */
static long *
x_get_win_property(gx_device_X *xdev, const char *atom_name)
{
    Atom r_type = (Atom)0;
    int r_format = 0;
    unsigned long count = 0;
    unsigned long bytes_remain;
    unsigned char *prop;

    if (XGetWindowProperty(xdev->dpy, RootWindowOfScreen(xdev->scr),
                           XInternAtom(xdev->dpy, atom_name, False),
                           0, 4, False, XA_CARDINAL,
                           &r_type, &r_format,
                           &count, &bytes_remain, &prop) == Success &&
                           prop &&
                           r_type == XA_CARDINAL &&
                           r_format == 32 &&
                           count == 4 &&
                           bytes_remain == 0)
            return (long *)prop;	/* should free at the caller */
    /* property does not exists or something went wrong */
    XFree(prop);
    return NULL;
}

/* Set up or take down buffering in a RAM image. */
static int
x_set_buffer(gx_device_X * xdev)
{
    /*
     * We must use the stable memory here, since the existence of the
     * buffer is independent of save/restore.
     */
    gs_memory_t *mem = gs_memory_stable(xdev->memory);
    bool buffered = xdev->space_params.MaxBitmap > 0;
    union {
      gx_device dev;
      gx_device_bbox bbox;
      gx_device_X x11;
    } tempdev;

 setup:
    if (buffered) {
        /* We want to buffer. */
        /* Check that we can set up a memory device. */
        gx_device_memory *mdev = (gx_device_memory *)xdev->target;

        /* This is icky, but is pickled into the architecture of the x11 devices.
         * This function is called (amongst other places) after a put_params that
         * changes the page buffer parameters.
         * If we're running one of the "wrapped" x11 devices (x11mono etc), we have to "patch"
         * the color_info so it matches the actual x11 device (because of the ways get_params
         * interacts with put_params in the Postscript world), rather than the "wrapping" device
         * (for example, x11mono "wraps" x11).
         * *But* if we run buffered, we have to use the real specs of the real x11 device.
         * Hence, the real color_info is saved into orig_color_info, and we use that here.
         */
        if (mdev == 0 || mdev->color_info.depth != xdev->orig_color_info.depth) {
            const gx_device_memory *mdproto =
                gdev_mem_device_for_bits(xdev->orig_color_info.depth);

            if (!mdproto) {
                buffered = false;
                goto setup;
            }
            if (mdev) {
                /* Update the pointer we're about to overwrite. */
                gx_device_set_target((gx_device_forward *)mdev, NULL);
            } else {
                mdev = gs_alloc_struct(mem, gx_device_memory,
                                       &st_device_memory, "memory device");
                if (mdev == 0) {
                    buffered = false;
                    goto setup;
                }
            }
            /*
             * We want to forward image drawing to the memory device.
             * That requires making the memory device forward its color
             * mapping operations back to the X device, creating a circular
             * pointer structure.  This is not a disaster, we just need to
             * be aware that this is going on.
             */
            gs_make_mem_device(mdev, mdproto, mem, 0, (gx_device *)xdev);
            gx_device_set_target((gx_device_forward *)xdev, (gx_device *)mdev);
            xdev->is_buffered = true;
        }
        if (mdev->width != xdev->width || mdev->height != xdev->height) {
            byte *buffer;
            ulong space;

            if (gdev_mem_data_size(mdev, xdev->width, xdev->height, &space) < 0 ||
                space > xdev->space_params.MaxBitmap) {
                buffered = false;
                goto setup;
            }
            buffer =
                (xdev->buffer ?
                 (byte *)gs_resize_object(mem, xdev->buffer, space, "buffer") :
                 gs_alloc_bytes(mem, space, "buffer"));
            if (!buffer) {
                buffered = false;
                goto setup;
            }
            xdev->buffer_size = space;
            xdev->buffer = buffer;
            mdev->width = xdev->width;
            mdev->height = xdev->height;
            rc_decrement(mdev->icc_struct, "x_set_buffer");
            mdev->icc_struct = xdev->icc_struct;
            rc_increment(xdev->icc_struct);
            mdev->color_info = xdev->orig_color_info;
            mdev->base = xdev->buffer;
            gdev_mem_open_scan_lines(mdev, xdev->height);
        }
        xdev->white = gx_device_white((gx_device *)xdev);
        xdev->black = gx_device_black((gx_device *)xdev);
        tempdev.bbox = gs_bbox_device;
    } else {
        /* Not buffering.  Release the buffer and memory device. */
        gs_free_object(mem, xdev->buffer, "buffer");
        xdev->buffer = 0;
        xdev->buffer_size = 0;
        if (!xdev->is_buffered)
            return 0;
        gx_device_set_target((gx_device_forward *)xdev->target, NULL);
        gx_device_set_target((gx_device_forward *)xdev, NULL);
        xdev->is_buffered = false;
        tempdev.x11 = gs_x11_device;
    }
    tempdev.dev.initialize_device_procs(&tempdev.dev);
    if (dev_proc(xdev, fill_rectangle) != tempdev.dev.procs.fill_rectangle) {
#define COPY_PROC(p) set_dev_proc(xdev, p, tempdev.dev.procs.p)
        COPY_PROC(initialize_device);
        COPY_PROC(fill_rectangle);
        COPY_PROC(copy_mono);
        COPY_PROC(copy_color);
        COPY_PROC(copy_alpha);
        COPY_PROC(fill_path);
        COPY_PROC(stroke_path);
        COPY_PROC(fill_mask);
        COPY_PROC(fill_trapezoid);
        COPY_PROC(fill_parallelogram);
        COPY_PROC(fill_triangle);
        COPY_PROC(draw_thin_line);
        COPY_PROC(strip_tile_rectangle);
        COPY_PROC(begin_typed_image);
        COPY_PROC(text_begin);
#undef COPY_PROC
        if (xdev->is_buffered) {
            check_device_separable((gx_device *)xdev);
            gx_device_forward_fill_in_procs((gx_device_forward *)xdev);
            xdev->box_procs = gdev_x_box_procs;
            xdev->box_proc_data = xdev;
        } else {
            check_device_separable((gx_device *)xdev);
            gx_device_fill_in_procs((gx_device *)xdev);
        }
    }
    return 0;
}

/* Allocate the backing pixmap, if any, and clear the window. */
void
gdev_x_clear_window(gx_device_X * xdev)
{
    if (!xdev->ghostview) {
        if (xdev->useBackingPixmap) {
            if (xdev->bpixmap == 0) {
                x_error_handler.oldhandler = XSetErrorHandler(x_catch_alloc);
                x_error_handler.alloc_error = False;
                xdev->bpixmap =
                    XCreatePixmap(xdev->dpy, xdev->win,
                                  xdev->width, xdev->height,
                                  xdev->vinfo->depth);
                XSync(xdev->dpy, False);	/* Force the error */
                if (x_error_handler.alloc_error) {
                    xdev->useBackingPixmap = False;
#ifdef DEBUG
                    emprintf(xdev->memory,
                             "Warning: Failed to allocated backing pixmap.\n");
#endif
                    if (xdev->bpixmap) {
                        XFreePixmap(xdev->dpy, xdev->bpixmap);
                        xdev->bpixmap = None;
                        XSync(xdev->dpy, False);	/* Force the error */
                    }
                }
                x_error_handler.oldhandler =
                    XSetErrorHandler(x_error_handler.oldhandler);
            }
        } else {
            if (xdev->bpixmap != 0) {
                XFreePixmap(xdev->dpy, xdev->bpixmap);
                xdev->bpixmap = (Pixmap) 0;
            }
        }
    }
    x_set_buffer(xdev);
    /* Clear the destination pixmap to avoid initializing with garbage. */
    if (xdev->dest == (Pixmap) 0) {
        xdev->dest = (xdev->bpixmap != (Pixmap) 0 ?
                  xdev->bpixmap : (Pixmap) xdev->win);
    }
    if (xdev->dest != (Pixmap) 0) {
        XSetForeground(xdev->dpy, xdev->gc, xdev->background);
        XFillRectangle(xdev->dpy, xdev->dest, xdev->gc,
                       0, 0, xdev->width, xdev->height);
    }

    /* Clear the background pixmap to avoid initializing with garbage. */
    if (xdev->bpixmap != (Pixmap) 0) {
        if (!xdev->ghostview)
            XSetWindowBackgroundPixmap(xdev->dpy, xdev->win, xdev->bpixmap);
        XSetForeground(xdev->dpy, xdev->gc, xdev->background);
        XFillRectangle(xdev->dpy, xdev->bpixmap, xdev->gc,
                       0, 0, xdev->width, xdev->height);
    }
    /* Initialize foreground and background colors */
    xdev->back_color = xdev->background;
    XSetBackground(xdev->dpy, xdev->gc, xdev->background);
    xdev->fore_color = xdev->background;
    XSetForeground(xdev->dpy, xdev->gc, xdev->background);
    xdev->colors_or = xdev->colors_and = xdev->background;
}

static int
x_initialize_device(gx_device *dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int code = gx_init_non_threadsafe_device(dev);

    if (code < 0)
        return code;

    /* Mark the new instance as closed. */
    xdev->is_open = false;

    /* Clear all other pointers. */
    xdev->target = 0;
    xdev->buffer = 0;
    xdev->dpy = 0;
    xdev->scr = 0;
    xdev->vinfo = 0;

    /* Clear pointer-like parameters. */
    xdev->win = (Window)None;
    xdev->bpixmap = (Pixmap)0;
    xdev->dest = (Pixmap)0;
    xdev->cp.pixmap = (Pixmap)0;
    xdev->ht.pixmap = (Pixmap)0;

    /* Reset pointer-related parameters. */
    xdev->is_buffered = false;

    return 0;
}


/* (External procedures are declared in gdevx.h.) */
void
gdev_x_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, initialize_device, x_initialize_device);
    set_dev_proc(dev, open_device, x_open);
    set_dev_proc(dev, get_initial_matrix, x_get_initial_matrix);
    set_dev_proc(dev, sync_output, x_sync);
    set_dev_proc(dev, output_page, x_output_page);
    set_dev_proc(dev, close_device, x_close);
    set_dev_proc(dev, map_rgb_color, gdev_x_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gdev_x_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, x_fill_rectangle);
    set_dev_proc(dev, copy_mono, x_copy_mono);
    set_dev_proc(dev, copy_color, x_copy_color);
    set_dev_proc(dev, get_params, gdev_x_get_params);
    set_dev_proc(dev, put_params, gdev_x_put_params);
    set_dev_proc(dev, get_page_device, x_get_page_device);
    set_dev_proc(dev, strip_tile_rectangle, x_strip_tile_rectangle);
    set_dev_proc(dev, get_bits_rectangle, x_get_bits_rectangle);
    set_dev_proc(dev, fillpage, x_fillpage);
}

/* ---------------- Get/put parameters ---------------- */

/* Get the device parameters.  See below. */
int
gdev_x_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int code = gx_default_get_params(dev, plist);
    long id = (long)xdev->pwin;

    if (code < 0 ||
        (code = param_write_long(plist, "WindowID", &id)) < 0 ||
        (code = param_write_bool(plist, ".IsPageDevice", &xdev->IsPageDevice)) < 0 ||
        (code = param_write_int(plist, "MaxTempPixmap", &xdev->MaxTempPixmap)) < 0 ||
        (code = param_write_int(plist, "MaxTempImage", &xdev->MaxTempImage)) < 0
        )
        DO_NOTHING;
    return code;
}

/* Set the device parameters.  We reimplement this so we can resize */
/* the window and avoid closing and reopening the device, and to add */
/* .IsPageDevice. */
int
gdev_x_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    /*
     * Provide copies of values of parameters being set:
     * is_open, width, height, HWResolution, IsPageDevice, Max*.
     */
    gx_device_X values;
    int orig_MaxBitmap = xdev->space_params.MaxBitmap;

    long pwin = (long)xdev->pwin;
    bool save_is_page = xdev->IsPageDevice;
    int ecode = 0, code;
    bool clear_window = false;

    values = *xdev;

    /* Handle extra parameters */

    ecode = param_put_long(plist, "WindowID", &pwin, ecode);
    ecode = param_put_bool(plist, ".IsPageDevice", &values.IsPageDevice, ecode);
    ecode = param_put_int(plist, "MaxTempPixmap", &values.MaxTempPixmap, ecode);
    ecode = param_put_int(plist, "MaxTempImage", &values.MaxTempImage, ecode);

    if (ecode < 0)
        return ecode;

    /* Unless we specified a new window ID, */
    /* prevent gx_default_put_params from closing the device. */
    if (pwin == (long)xdev->pwin)
        dev->is_open = false;
    xdev->IsPageDevice = values.IsPageDevice;
    code = gx_default_put_params(dev, plist);
    dev->is_open = values.is_open; /* saved value */
    if (code < 0) {		/* Undo setting of .IsPageDevice */
        xdev->IsPageDevice = save_is_page;
        return code;
    }
    if (pwin != (long)xdev->pwin) {
        if (xdev->is_open)
            gs_closedevice(dev);
        xdev->pwin = (Window) pwin;
    }
    /* Restore the original page size if it was set by Ghostview */
    /* This gives the Ghostview user control over the /setpage entry */
    if (xdev->is_open && xdev->ghostview) {
       dev->width = values.width;
       dev->height = values.height;
       dev->x_pixels_per_inch = values.x_pixels_per_inch;
       dev->y_pixels_per_inch = values.y_pixels_per_inch;
       dev->HWResolution[0] = values.HWResolution[0];
       dev->HWResolution[1] = values.HWResolution[1];
       dev->MediaSize[0] = values.MediaSize[0];
       dev->MediaSize[1] = values.MediaSize[1];
    }
    /* If the device is open, resize the window. */
    /* Don't do this if Ghostview is active. */
    if (xdev->is_open && !xdev->ghostview &&
        (dev->width != values.width || dev->height != values.height ||
         dev->HWResolution[0] != values.HWResolution[0] ||
         dev->HWResolution[1] != values.HWResolution[1])
        ) {
        int area_width = WidthOfScreen(xdev->scr), area_height = HeightOfScreen(xdev->scr);
        int dw, dh;

        /* Get work area */
        x_get_work_area(xdev, &area_width, &area_height);

        /* Preserve screen resolution */
        dev->x_pixels_per_inch = values.x_pixels_per_inch;
        dev->y_pixels_per_inch = values.y_pixels_per_inch;
        dev->HWResolution[0] = values.HWResolution[0];
        dev->HWResolution[1] = values.HWResolution[1];

        /* Recompute window size using screen resolution and available work area size*/
        /* pixels */
        dev->width = min(dev->width, area_width);
        dev->height = min(dev->height, area_height);

        if (dev->width <= 0 || dev->height <= 0) {
            emprintf3(dev->memory, "Requested pagesize %d x %d not supported by %s device\n",
                                  dev->width, dev->height, dev->dname);
            return_error(gs_error_rangecheck);
        }

        /* points */
        dev->MediaSize[0] = (float)dev->width / xdev->x_pixels_per_inch * 72;
        dev->MediaSize[1] = (float)dev->height / xdev->y_pixels_per_inch * 72;

        dw = dev->width - values.width;
        dh = dev->height - values.height;
        if (dw || dh) {
            XResizeWindow(xdev->dpy, xdev->win,
                          dev->width, dev->height);
            if (xdev->bpixmap != (Pixmap) 0) {
                XFreePixmap(xdev->dpy, xdev->bpixmap);
                xdev->bpixmap = (Pixmap) 0;
            }
            xdev->dest = 0;
            clear_window = true;
        }
        /* Attempt to update the initial matrix in a sensible way. */
        /* The whole handling of the initial matrix is a hack! */
        if (xdev->initial_matrix.xy == 0) {
            if (xdev->initial_matrix.xx < 0) {	/* 180 degree rotation */
                xdev->initial_matrix.tx += dw;
            } else {		/* no rotation */
                xdev->initial_matrix.ty += dh;
            }
        } else {
            if (xdev->initial_matrix.xy < 0) {	/* 90 degree rotation */
                xdev->initial_matrix.tx += dh;
                xdev->initial_matrix.ty += dw;
            } else {		/* 270 degree rotation */
            }
        }
    }
    xdev->MaxTempPixmap = values.MaxTempPixmap;
    xdev->MaxTempImage = values.MaxTempImage;

    if (clear_window || xdev->space_params.MaxBitmap != orig_MaxBitmap) {
        if (xdev->is_open)
            gdev_x_clear_window(xdev);
    }
    return 0;
}

/* ---------------- Closing/finalization ---------------- */
/* Close the device. */
int
gdev_x_close(gx_device_X *xdev)
{
    long MaxBitmap = xdev->space_params.MaxBitmap;

    if (xdev->ghostview)
        gdev_x_send_event(xdev, xdev->DONE);
    if (xdev->vinfo) {
        XFree((char *)xdev->vinfo);
        xdev->vinfo = NULL;
    }
    gdev_x_free_colors(xdev);
    if (xdev->dpy && xdev->cmap != DefaultColormapOfScreen(xdev->scr)) {
        XFreeColormap(xdev->dpy, xdev->cmap);
        xdev->cmap = DefaultColormapOfScreen(xdev->scr);
    }
    if (xdev->dpy && xdev->gc) {
        XFreeGC(xdev->dpy, xdev->gc);
        xdev->gc = NULL;
    }
    if (xdev->dpy && xdev->bpixmap != (Pixmap)0) {
        XFreePixmap(xdev->dpy, xdev->bpixmap);
        xdev->bpixmap = (Pixmap)0;
        xdev->dest = (Pixmap)0;
    }

    /* Closure of device will not close the window
       finalize will do that. */
    xdev->pwin = xdev->win;

    /* MaxBitmap == 0 ensures x_set_buffer() configures as non-buffering */
    xdev->space_params.MaxBitmap = 0;
    x_set_buffer(xdev);
    xdev->space_params.MaxBitmap = MaxBitmap;

    return 0;
}
