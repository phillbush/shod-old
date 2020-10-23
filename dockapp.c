#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include "shod.h"
#include "dockapp.h"
#include "monitor.h"

/* checks whether a window was specified as -d argument */
static unsigned
isdocked(Window win)
{
	char **argv = NULL;
	int argc;
	int retval = ~0;
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
	size_t prevnum;         /* previous number of dockapps */

	if ((d = malloc(sizeof *d)) == NULL)
		err(1, "malloc");

	prevnum = dock.num;
	dock.num++;
	d->next = NULL;
	d->win = win;
	d->w = wa->width;
	d->h = wa->height;
	d->pos = isdocked(win);
	for (last = dock.list; last; last = last->next)
		if (last->pos <= d->pos)
			tmp = last;
	if (!tmp && !dock.list) {
		d->prev = NULL;
		d->next = NULL;
		dock.list = d;
	} else if (!tmp && dock.list) {
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
	if (config.dockmode == DockBelow) {
		Window wins[2];

		wins[0] = layerwin[LayerDockapps];
		wins[1] = d->parent;
		XRestackWindows(dpy, wins, sizeof wins);
	}
	XReparentWindow(dpy, d->win, dock.win, 0, 0);
	dockapp_redock();
	XMapWindow(dpy, d->win);
	if (prevnum == 0) {
		XMapWindow(dpy, dock.win);
		monitor_updatearea();
	}
}

void
dockapp_del(struct Dockapp *d)
{
	if (d == NULL)
		return;

	dock.num--;
	if (d->prev)
		d->prev->next = d->next;
	else
		dock.list = d->next;
	if (d->next)
		d->next->prev = d->prev;
	XDestroyWindow(dpy, d->parent);
	free(d);

	dockapp_redock();
	if (dock.num == 0) {
		XUnmapWindow(dpy, dock.win);
		monitor_updatearea();
	}
}

void
dockapp_redock(void)
{
	struct Dockapp *d;
	unsigned w, h;
	int x, y;
	size_t n;

	if (dock.num == 0)
		return;

	if (config.dockside == DockTop || config.dockside == DockBottom) {
		w = config.dockwidth * dock.num;
		h = config.dockwidth;
		for (d = dock.list, n = 0; d; d = d->next, n++) {
			y = (config.dockwidth - d->h) / 2;
			if (config.dockinverse)
				x = w - (n + 1) * config.dockwidth;
			else
				x = n * config.dockwidth;
			x += (config.dockwidth - d->w) / 2;
			XMoveWindow(dpy, d->win, x, y);
		}
	} else {
		w = config.dockwidth;
		h = config.dockwidth * dock.num;
		for (d = dock.list, n = 0; d; d = d->next, n++) {
			x = (config.dockwidth - d->w) / 2;
			if (config.dockinverse)
				y = h - (n + 1) * config.dockwidth;
			else
				y = n * config.dockwidth;
			y += (config.dockwidth - d->h) / 2;
			XMoveWindow(dpy, d->win, x, y);
		}
	}
	switch (config.dockside) {
	case DockTop:
		y = wm.mon->my;
		switch (config.dockplace) {
		case DockBegin:
			x = wm.mon->mx;
			break;
		case DockCenter:
			x = wm.mon->mx + (wm.mon->mw - dock.num * config.dockwidth) / 2;
			break;
		case DockEnd:
			x = wm.mon->mx + wm.mon->mw - dock.num * config.dockwidth;
			break;
		}
		break;
	case DockLeft:
		x = wm.mon->mx;
		switch (config.dockplace) {
		case DockBegin:
			y = wm.mon->my;
			break;
		case DockCenter:
			y = wm.mon->my + (wm.mon->mh - dock.num * config.dockwidth) / 2;
			break;
		case DockEnd:
			y = wm.mon->my + wm.mon->mh - dock.num * config.dockwidth;
			break;
		}
		break;
	case DockBottom:
		y = wm.mon->my + wm.mon->mh - config.dockwidth;
		switch (config.dockplace) {
		case DockBegin:
			x = wm.mon->mx;
			break;
		case DockCenter:
			x = wm.mon->mx + (wm.mon->mw - dock.num * config.dockwidth) / 2;
			break;
		case DockEnd:
			x = wm.mon->mx + wm.mon->mw - dock.num * config.dockwidth;
			break;
		}
		break;
	case DockRight:
		x = wm.mon->mx + wm.mon->mw - config.dockwidth;
		switch (config.dockplace) {
		case DockBegin:
			y = wm.mon->my;
			break;
		case DockCenter:
			y = wm.mon->my + (wm.mon->mh - dock.num * config.dockwidth) / 2;
			break;
		case DockEnd:
			y = wm.mon->my + wm.mon->mh - dock.num * config.dockwidth;
			break;
		}
		break;
	}
	XMoveResizeWindow(dpy, dock.win, x, y, w, h);
}
