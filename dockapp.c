#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include "shod.h"
#include "dockapp.h"

/* checks whether a window was specified as -d argument */
static int
isdocked(Window win)
{
	char **argv = NULL;
	int argc;
	int retval = -1;
	size_t i;

	if (XGetCommand(dpy, win, &argv, &argc) && argv) {
		for (i = 0; i < dock.ndockapps; i++) {
			if (strcmp(*argv, dock.dockapps[i]) == 0) {
				retval = i;
				break;
			}
		}
		XFreeStringList(argv);
	}
	return retval;
}

/* create dockapp's parent window */
static Window
createparentwin(void)
{
	XSetWindowAttributes swa;
	Window win;
	unsigned long valuemask;

	valuemask = CWEventMask;
	swa.event_mask = SubstructureNotifyMask | SubstructureRedirectMask
	               | StructureNotifyMask | ButtonPressMask;
	if (dock.xpm != None) {
		swa.background_pixmap = dock.xpm;
		valuemask |= CWBackPixmap;
	} else {
		swa.background_pixel = BlackPixel(dpy, screen);
		valuemask |= CWBackPixel;
	}
	win = XCreateWindow(dpy, root, 0, 0, dock.size, dock.size, 0,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    valuemask, &swa);
	return win;
}

struct Dockapp *
getdockapp(Window win)
{
	struct Dockapp *d;

	for (d = dock.list; d; d = d->next)
		if (d->win == win)
			return d;
	return NULL;
}

void
dockapp_add(Window win, XWindowAttributes *wa)
{
	struct Dockapp *d = NULL;
	struct Dockapp *last = NULL;
	struct Dockapp *tmp = NULL;

	if ((d = malloc(sizeof *d)) == NULL)
		err(1, "malloc");

	d->next = NULL;
	d->win = win;
	d->w = wa->width;
	d->h = wa->height;
	d->parent = createparentwin();
	d->pos = isdocked(win);
	for (last = dock.list; last && last->next; last = last->next)
		if (last->pos <= d->pos)
			tmp = last;
	if (!tmp && !last) {
		d->prev = NULL;
		d->next = NULL;
		dock.list = d;
	} else if (!tmp && last) {
		dock.list->prev = d;
		d->prev = NULL;
		d->next = dock.list;
		dock.list = d;
	} else {
		if (tmp->next)
			tmp->next->prev = d;
		d->next = tmp->next;
		d->prev = tmp;
		tmp->next = d;
	}
	if (dock.mode == DockBelow) {
		Window wins[2];

		wins[0] = layerwin[LayerDockapps];
		wins[1] = d->parent;
		XRestackWindows(dpy, wins, sizeof wins);
	}
	XReparentWindow(dpy, d->win, d->parent, (dock.size - d->w) / 2, (dock.size - d->h) / 2);
	dockapp_redock();
	XMapWindow(dpy, d->parent);
	XMapWindow(dpy, d->win);
}

void
dockapp_del(struct Dockapp *d)
{
	if (d == NULL)
		return;

	if (d->prev)
		d->prev->next = d->next;
	else
		dock.list = d->next;
	if (d->next)
		d->next->prev = d->prev;
	XDestroyWindow(dpy, d->parent);
	free(d);

	dockapp_redock();
}

void
dockapp_redock(void)
{
	struct Dockapp *d;
	int xs, ys;         /* beginning of dock */
	int xe, ye;         /* end of dock */
	int x, y;
	size_t n;

	xs = (dock.position == DockRight) ? wm.mon->mw - wm.mon->br - dock.size : wm.mon->mx + wm.mon->bl;
	ys = (dock.position == DockBottom) ? wm.mon->mh - wm.mon->bb - dock.size : wm.mon->my + wm.mon->bt;
	xe = (dock.position == DockLeft) ? wm.mon->mx + wm.mon->bl : wm.mon->mw - wm.mon->br;
	ye = (dock.position == DockTop) ? wm.mon->my + wm.mon->bt : wm.mon->mh - wm.mon->bb;
	for (d = dock.list, n = 0; d; d = d->next, n++) {
		if (dock.orientation) {
			switch (dock.position) {
			case DockTop:
			case DockBottom:
				x = xe - (n + 1) * dock.size;
				y = ys;
				break;
			case DockLeft:
			case DockRight:
				x = xs;
				y = ye - (n + 1) * dock.size;
				break;
			}
		} else {
			switch (dock.position) {
			case DockTop:
			case DockBottom:
				x = xs + n * dock.size;
				y = ys;
				break;
			case DockLeft:
			case DockRight:
				x = xs;
				y = ys + n * dock.size;
				break;
			}
		}
		XMoveWindow(dpy, d->parent, x, y);
	}
}
