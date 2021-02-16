#include <string.h>
#include "shod.h"
#include "ewmh.h"
#include "winlist.h"

void
ewmh_init(void)
{
	unsigned long data[2];

	/* Set window and property that indicates that the wm is ewmh compliant */
	XChangeProperty(dpy, wmcheckwin, netatom[NetSupportingWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *) "shod", strlen("shod"));
	XChangeProperty(dpy, root, netatom[NetSupportingWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);

	/* Set properties that the window manager supports */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);

	/* This wm does not support viewports */
	data[0] = data[1] = 0;
	XChangeProperty(dpy, root, netatom[NetDesktopViewport], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 2);
}

void
ewmh_setallowedactions(Window win)
{
	XChangeProperty(dpy, win, netatom[NetWMAllowedActions],
	                XA_ATOM, 32, PropModeReplace,
	                (unsigned char *)&netatom[NetWMActionMove], 11);
	/*
	 * 11 is the number of actions supported, and NetWMActionMove is the
	 * first of them.  See the EWMH atoms enumeration in shod.h for more
	 * information.
	 */
}

void
ewmh_setactivewindow(Window w)
{
	XChangeProperty(dpy, root, netatom[NetActiveWindow],
	                XA_WINDOW, 32, PropModeReplace,
	                (unsigned char *)&w, 1);
}

void
ewmh_setnumberofdesktops(void)
{
    XChangeProperty(dpy, root, netatom[NetNumberOfDesktops],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&wm.wscount, 1);
}

void
ewmh_setcurrentdesktop(int wsnum)
{
	if (wsnum < 0 || wsnum >= wm.wscount)
		return;

	XChangeProperty(dpy, root, netatom[NetCurrentDesktop],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&wsnum, 1);
}

void
ewmh_setframeextents(struct Client *c)
{
	unsigned long data[4];

	data[0] = data[1] = data[3] = c->border;
	data[2] = c->border + ((config.ignoretitle) ? 0 : config.title_height);

	XChangeProperty(dpy, c->win, netatom[NetFrameExtents],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&data, 4);
}

void
ewmh_setshowingdesktop(int n)
{
	XChangeProperty(dpy, root, netatom[NetShowingDesktop],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&n, 1);
}

void
ewmh_setstate(struct Client *c)
{
	Atom data[6];
	int n = 0;

	if (c == NULL)
		return;

	if (wm.selmon->focused == c)
		data[n++] = netatom[NetWMStateFocused];

	if (c->isfullscreen)
		data[n++] = netatom[NetWMStateFullscreen];

	if (c->isshaded)
		data[n++] = netatom[NetWMStateShaded];

	switch (c->state) {
		case ISMINIMIZED:
			data[n++] = netatom[NetWMStateHidden];
			break;
		case ISSTICKY:
			data[n++] = netatom[NetWMStateSticky];
			break;
		case ISMAXIMIZED:
			data[n++] = netatom[NetWMStateMaximizedVert];
			data[n++] = netatom[NetWMStateMaximizedHorz];
			break;
		default:
			break;
	}

	if (c->layer > 0)
		data[n++] = netatom[NetWMStateAbove];
	else if (c->layer < 0)
		data[n++] = netatom[NetWMStateBelow];

	XChangeProperty(dpy, c->win, netatom[NetWMState],
	                XA_ATOM, 32, PropModeReplace,
	                (unsigned char *)data, n);
}

static void
ewmh_setdesktop(Window win, long d)
{
	XChangeProperty(dpy, win, netatom[NetWMDesktop],
	                XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&d, 1);
}

void
ewmh_setwmdesktop(void)
{
	struct Monitor *mon;
	struct WS *ws;
	struct Column *col;
	struct Client *c;
	long desk;
	desk = 0;

	for (c = wm.minimized; c; c = c->next) {
		ewmh_setdesktop(c->win, 0xFFFFFFFF);
	}
	for (mon = wm.mon; mon; mon = mon->next) {
		for (c = mon->sticky; c; c = c->next) {
			ewmh_setdesktop(c->win, 0xFFFFFFFF);
		}
		for (ws = mon->ws; ws; ws = ws->next, desk++) {
			for (c = ws->floating; c; c = c->next) {
				ewmh_setdesktop(c->win, desk);
			}
			for (col = ws->col; col; col = col->next) {
				for (c = col->row; c; c = c->next) {
					ewmh_setdesktop(c->win, desk);
				}
			}
		}
	}
}

void
ewmh_setworkarea(void)
{
	unsigned long data[4];

	data[0] = config.gapleft;
	data[1] = config.gaptop;
	data[2] = screenw - config.gapleft - config.gapright;
	data[3] = screenh - config.gaptop - config.gapbottom;

	XChangeProperty(dpy, root, netatom[NetWorkarea],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&data, 4);
}

void
ewmh_setclients(void)
{
	XChangeProperty(dpy, root, netatom[NetClientList],
	                XA_WINDOW, 32, PropModeReplace,
	                (unsigned char *) winlist.list, winlist.num);
}

void
ewmh_setclientsstacking(void)
{
	XChangeProperty(dpy, root, netatom[NetClientListStacking],
	                XA_WINDOW, 32, PropModeReplace,
	                (unsigned char *) winlist.list, winlist.num);
}
