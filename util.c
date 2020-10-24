#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include "shod.h"
#include "util.h"

Atom
getatomprop(Window win, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, win, prop, 0L, sizeof atom, False, XA_ATOM,
	                       &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

unsigned long *
getcardinalprop(Window win, Atom atom, unsigned long size)
{
	unsigned char *prop_ret = NULL;
	unsigned long *values = NULL;
	Atom da;            /* dummy */
	int di;             /* dummy */
	unsigned long dl;   /* dummy */
	int status; 

	status = XGetWindowProperty(dpy, win, atom, 0, size, False,
	                            XA_CARDINAL, &da, &di, &dl, &dl,
	                            (unsigned char **)&prop_ret);

	if (status == Success && prop_ret)
		values = (unsigned long *)prop_ret;

	return values;
}

/* return client position, width and height */
void
getgeom(struct Client *c, int *x_ret, int *y_ret, int *w_ret, int *h_ret)
{
	if (c == NULL)
		return;

	if (x_ret)
		*x_ret = (c->isfullscreen) ? c->mon->mx : (c->state & ISMAXIMIZED) ? c->mx : c->ux;
	if (y_ret)
		*y_ret = (c->isfullscreen) ? c->mon->my : (c->state & ISMAXIMIZED) ? c->my : c->uy;
	if (w_ret)
		*w_ret = (c->isfullscreen) ? c->mon->mw : (c->state & ISMAXIMIZED) ? c->mw : c->uw;
	if (h_ret)
		*h_ret = (c->isfullscreen) ? c->mon->mh : (c->state & ISMAXIMIZED) ? c->mh : c->uh;
}

struct Monitor *
getmon(int x, int y)
{
	struct Monitor *mon;

	for (mon = wm.mon; mon; mon = mon->next)
		if (x >= mon->mx && x <= mon->mx + mon->mw &&
		    y >= mon->my && y <= mon->my + mon->mh)
			return mon;

	return NULL;
}

/* get window's WM_STATE property */
long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}
