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
	struct Dockapp *d;
	struct Dockapp *tmp = NULL;

	if ((d = malloc(sizeof *d)) == NULL)
		err(1, "malloc");

	d->next = NULL;
	d->win = win;
	d->w = wa->width;
	d->h = wa->height;
	d->parent = createparentwin();
	d->pos = isdocked(win);
	for (tmp = dock.list; tmp && d->pos < tmp->pos; tmp = tmp->next)
		;
	if (!tmp) {
		d->prev = NULL;
		dock.list = d;
	} else {
		d->prev = tmp->prev;
		if (tmp->prev)
			tmp->prev->next = d;
		tmp->prev = d;
	}
	d->next = tmp;
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

	xs = (dock.position == DockRight) ? wm.mon->mw - wm.mon->br - dock.size : wm.mon->mx + wm.mon->bl;
	ys = (dock.position == DockBottom) ? wm.mon->mh - wm.mon->bb - dock.size : wm.mon->my + wm.mon->bt;
	xe = (dock.position == DockLeft) ? wm.mon->mx + wm.mon->bl : wm.mon->mw - wm.mon->br;
	ye = (dock.position == DockTop) ? wm.mon->my + wm.mon->bt : wm.mon->mh - wm.mon->bb;
	for (d = dock.list; d; d = d->next) {
		if (dock.orientation) {
			switch (dock.position) {
			case DockTop:
			case DockBottom:
				x = xe - (d->pos + 1) * dock.size;
				y = ys;
				break;
			case DockLeft:
			case DockRight:
				x = xs;
				y = ye - (d->pos + 1) * dock.size;
				break;
			}
		} else {
			switch (dock.position) {
			case DockTop:
			case DockBottom:
				x = xs + d->pos * dock.size;
				y = ys;
				break;
			case DockLeft:
			case DockRight:
				x = xs;
				y = ys + d->pos * dock.size;
				break;
			}
		}

		XMoveWindow(dpy, d->parent, x, y);
	}
}
