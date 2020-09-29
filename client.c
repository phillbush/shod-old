#include <err.h>
#include <stdlib.h>
#include <X11/Xutil.h>
#include "shod.h"
#include "client.h"
#include "winlist.h"
#include "workspace.h"
#include "ewmh.h"
#include "util.h"

#define DIV 15      /* number to divide the screen into grids */

#define NOTINCSIZE  0
#define INCSIZEW    1 << 0
#define INCSIZEH    1 << 1
#define INCSIZEWH   1 << 0 | 1 << 1

/* function declarations */
static void applyaspectsize(struct Client *c);
static void applyincsize(struct Client *c, int *w, int *h);
static struct Column *colalloc(struct Column *prev, struct Column *next, struct Client *c);
static void colfree(struct Column *col);
static void moveresize(struct Client *c, int x, int y, int w, int h, int incsize);
static void querypointer(int *x_ret, int *y_ret);
static long setsizehints(struct Client *c);

/* global variables */
static int showingdesk = 0;     /* whether the wm is showing the desktop */

/* get client given a window */
struct Client *
getclient(Window w)
{
	struct Monitor *mon;
	struct WS *ws;
	struct Column *col;
	struct Client *c;

	for (mon = wm.mon; mon; mon = mon->next) {
		for (ws = mon->ws; ws; ws = ws->next) {
			for (col = ws->col; col; col = col->next) {
				for (c = col->row; c; c = c->next) {
					if (c->win == w) {
						return c;
					}
				}
			}
			for (c = ws->floating; c; c = c->next) {
				if (c->win == w) {
					return c;
				}
			}
		}
		for (c = mon->sticky; c; c = c->next) {
			if (c->win == w) {
				return c;
			}
		}
	}
	for (c = wm.minimized; c; c = c->next) {
		if (c->win == w) {
			return c;
		}
	}
	return NULL;
}

/* adopt a client */
void
client_add(Window win, XWindowAttributes *wa)
{
	struct Client *c;
	long flags;
	int focus = 1;
	unsigned long *values;

	if ((c = malloc(sizeof *c)) == NULL)
		err(1, "malloc");

	/* check whether to focus window */
	if ((values = getcardinalprop(win, netatom[NetWMUserTime], 1)) != NULL) {
		if (values[0] == 0)
			focus = 0;
		XFree(values);
	}

	c->prev = c->next = NULL;
	c->fprev = c->fnext = NULL;
	c->col = NULL;
	c->state = ISNORMAL;
	c->layer = 0;
	c->win = win;
	c->ux = wa->x;
	c->uy = wa->y;
	c->uw = wa->width;
	c->uh = wa->height;
	c->isfullscreen = 0;
	flags = setsizehints(c);
	c->isuserplaced = (flags & USPosition) ? 1 : 0;

	client_raise(c);

	XSelectInput(dpy, c->win, EnterWindowMask | StructureNotifyMask
	             | PropertyChangeMask | FocusChangeMask);

	XGrabButton(dpy, AnyButton, AnyModifier, win, False,
	            ButtonPressMask | ButtonReleaseMask,
	            GrabModeSync, GrabModeSync, None, None);

	XSetWindowBorderWidth(dpy, c->win, config.border_width);

	winlist_add(c->win);
	ewmh_setframeextents(c->win);
	ewmh_setallowedactions(c->win);
	ewmh_setclients();
	ewmh_setclientsstacking();

	client_sendtows(c, wm.selmon->selws, 1, 1, 0);

	XMapWindow(dpy, c->win);
	/* we don't want a client to steal focus when in fullscreen */
	if (focus && (wm.selmon->selws->focused == NULL || !(wm.selmon->selws->focused->isfullscreen)))
		client_focus(c);
}

/* delete a client, maybe free it and focus another client */
void
client_del(struct Client *c, int dofree, int delws)
{
	struct Column *col;
	struct Client *bestfocus = NULL;
	int focus = 0;

	if (c == NULL)
		return;

	if (dofree && !(c->state & ISMINIMIZED) && wm.selmon->focused == c) {
		bestfocus = client_bestfocus(c);
		focus = 1;
	}

	if (c->prev) {  /* c is not at the beginning of the list */
		c->prev->next = c->next;
	} else {
		switch (c->state) {
		case ISMINIMIZED:
			wm.minimized = c->next;
			break;
		case ISSTICKY:
			c->mon->sticky = c->next;
			break;
		case ISNORMAL:
			c->ws->floating = c->next;
			break;
		case ISMAXIMIZED:
			for (col = c->ws->col; col; col = col->next) {
				if (col->row == c) {
					if (c->next)
						col->row = c->next;
					else
						colfree(col);
					break;
				}
			}
			break;
		}
	}

	if (c->next)
		c->next->prev = c->prev;

	if (c->state & ISMAXIMIZED && c->ws == wm.selmon->selws)
		client_tile(c->ws, 1);

	/*
	 * the calling routine wants client's workspace to be deleted if
	 * the client was the last client in it
	 */
	if (delws && c->state & ISBOUND) {
		struct WS *ws, *lastws;

		/* find last workspace */
		for (ws = c->ws->mon->ws; ws; ws = ws->next)
			lastws = ws;

		c->ws->nclients--;

		/* only the last ws can have 0 clients */
		if (c->ws->nclients == 0) {
			int changedesk = 0;

			if (ws_isvisible(c->ws)) {
				c->mon->selws = lastws;
				if (c->mon == wm.selmon) {
					wm.selmon->selws = lastws;
					changedesk = 1;
				}
			}
			ws_del(c->ws);
			ewmh_setnumberofdesktops();
			if (changedesk)
				ewmh_setcurrentdesktop(getwsnum(lastws));
		}
	}

	if (dofree) {
		struct WS *ws;

		/* if other workspaces' focus is on c, change it */
		if (!(c->state & ISMINIMIZED))
			for (ws = c->mon->ws; ws; ws = ws->next)
				if (ws->focused == c)
					ws->mon->focused = ws->focused = NULL;

		client_unfocus(c);
		if (focus)
			client_focus(bestfocus);

		winlist_del(c->win);
		ewmh_setclients();
		ewmh_setclientsstacking();
		ewmh_setwmdesktop();
		free(c);
	}
}

/* move client above others */
void
client_above(struct Client *c, int above)
{
	/* maximized and fullscreen windows should not change layer */
	if (c == NULL || c->state & ISMAXIMIZED || c->isfullscreen)
		return;

	c->layer = (above) ? 1 : 0;
	client_raise(c);
}

/* move client below others */
void
client_below(struct Client *c, int below)
{
	/* maximized and fullscreen windows should not change layer */
	if (c == NULL || c->state & ISMAXIMIZED || c->isfullscreen)
		return;

	c->layer = (below) ? -1 : 0;
	client_raise(c);
}

/* find the best client to focus after deleting c */
struct Client *
client_bestfocus(struct Client *c)
{
	struct Client *focus = NULL;
	struct Client *tmp = NULL;

	if (c == NULL || c->state & ISMINIMIZED)
		return NULL;

	/* If client is floating, try to focus next floating */
	if (c->state & ISFLOATING) {
		for (tmp = c->mon->focused; tmp; tmp = tmp->fnext) {
			if (tmp == c)
				continue;
			if (tmp->state & ISNORMAL && tmp->ws == c->ws)
				break;
			if (tmp->state & ISSTICKY)
				break;
		}
		focus = tmp;
		if (!focus) {
			for (tmp = c->mon->focused; tmp; tmp = tmp->fnext) {
				if (tmp == c) {
					break;
				}
			}
		}
		focus = tmp;
	} else if (c->state & ISMAXIMIZED) {
		if (c->next)
			focus = c->next;
		if (!focus && c->prev)
			focus = c->prev;
		if (!focus && c->state & ISMAXIMIZED) {
			if (!focus && c->col->prev)
				focus = c->col->prev->row;
			if (!focus && c->col->next)
				focus = c->col->next->row;
		}
		if (!focus) {
			for (tmp = c->mon->focused; tmp; tmp = tmp->fnext) {
				if (tmp == c)
					continue;
				if (tmp->state & ISNORMAL && tmp->ws == c->ws)
					break;
				if (tmp->state & ISSTICKY && tmp->mon == c->mon)
					break;
			}
			focus = tmp;
		}
	}

	return focus;
}

/* send a WM_DELETE message to client */
void
client_close(struct Client *c)
{
	XEvent ev;

	ev.type = ClientMessage;
	ev.xclient.window = c->win;
	ev.xclient.message_type = wmatom[WMProtocols];
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = wmatom[WMDeleteWindow];
	ev.xclient.data.l[1] = CurrentTime;

	/*
	 * communicate with the given Client, kindly telling it to
	 * close itself and terminate any associated processes using
	 * the WM_DELETE_WINDOW protocol
	 */
	XSendEvent(dpy, c->win, False, NoEventMask, &ev);
}

/* configure client size and position */
void
client_configure(struct Client *c, XWindowChanges wc, unsigned valuemask)
{
	int x, y, w, h;

	if (c == NULL || c->state & ISMINIMIZED || c->isfullscreen)
		return;

	client_getgeom(c, &x, &y, &w, &h);

	x = (valuemask & CWX) ? wc.x - x : 0;
	y = (valuemask & CWY) ? wc.y - y : 0;
	w = (valuemask & CWWidth) ? wc.width - w : 0;
	h = (valuemask & CWHeight) ? wc.height - h : 0;

	if (valuemask & (CWX | CWY))
		client_move(c, x, y);
	if (valuemask & (CWWidth | CWHeight))
		client_resize(c, SE, w, h);
}

/* focus client */
void
client_focus(struct Client *c)
{
	struct Client *prevfocused;
	struct Monitor *mon;
	struct WS *ws;
	int wsnum = 0;

	if (showingdesk)
		client_showdesktop(0);

	if (c == NULL || c->state & ISMINIMIZED) {
		XSetInputFocus(dpy, focuswin, RevertToParent, CurrentTime);
		ewmh_setactivewindow(None);
		return;
	}

	client_setborder(wm.selmon->focused, config.unfocused);

	prevfocused = wm.selmon->focused;
	wm.selmon = c->mon;
	if (c->state & ISBOUND) {
		wm.selmon->selws = c->ws;
		wm.selmon->selws->focused = c;
		wm.selmon->selws = c->ws;
	}
	client_unfocus(c);
	c->fnext = wm.selmon->focused;
	c->fprev = NULL;
	if (wm.selmon->focused)
		wm.selmon->focused->fprev = c;
	wm.selmon->focused = c;

	client_setborder(c, config.focused);

	if (c->state & ISMINIMIZED)
		client_minimize(c, 0);

	XSetInputFocus(dpy, c->win, RevertToParent, CurrentTime);

	winlist_focus(c->win);

	for (mon = wm.mon; mon; mon = mon->next) {
		for (ws = mon->ws; ws; ws = ws->next) {
			if (ws == c->ws)
				goto found;
			else
				wsnum++;
		}
	}

found:
	ewmh_setstate(prevfocused);
	ewmh_setstate(c);
	ewmh_setactivewindow(c->win);
	ewmh_setclientsstacking();
	ewmh_setcurrentdesktop(wsnum);
}

/* make/unmake client fullscreen */
void
client_fullscreen(struct Client *c, int fullscreen)
{
	if (c == NULL || c->state & ISMINIMIZED)
		return;

	if (c->state & ISSTICKY)
		client_stick(c, 0);

	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		XSetWindowBorderWidth(dpy, c->win, 0);
		if (ISVISIBLE(c))
			moveresize(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh, NOTINCSIZE);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen) {
		c->isfullscreen = 0;
		XSetWindowBorderWidth(dpy, c->win, config.border_width);
		if (c->state & ISMAXIMIZED)
			client_tile(c->ws, 0);
		else if (ISVISIBLE(c))
			moveresize(c, c->ux, c->uy, c->uw, c->uh, INCSIZEWH);
		client_raise(c);
	}
}

/* return client position, width and height */
void
client_getgeom(struct Client *c, int *x_ret, int *y_ret, int *w_ret, int *h_ret)
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

/* go to workspace */
void
client_gotows(struct WS *ws, int wsnum)
{
	struct Column *col;
	struct Client *c;
	int cursorx, cursory;

	if (ws == NULL)
		ws = getws(wsnum);
	else
		wsnum = getwsnum(ws);

	if (ws == NULL || ws == wm.selmon->selws)
		return;

	client_tile(ws, 0);

	if (!ws_isvisible(ws)) {
		/* hide clients of previous current workspace */
		if (ws->mon->selws) {
			for (c = ws->mon->selws->floating; c; c = c->next)
				client_hide(c, 1);
			for (col = ws->mon->selws->col; col; col = col->next)
				for (c = col->row; c; c = c->next)
					client_hide(c, 1);
		}

		/* unhide clients of new current workspace */
		for (c = ws->floating; c; c = c->next)
			client_hide(c, 0);
		for (col = ws->col; col; col = col->next)
			for (c = col->row; c; c = c->next)
				client_hide(c, 0);
	}

	/* if changing focus to a new monitor and the cursor isn't there, warp it */
	querypointer(&cursorx, &cursory);
	if (ws->mon != wm.selmon && ws->mon != getmon(cursorx, cursory))
		XWarpPointer(dpy, None, root, 0, 0, 0, 0,
		             ws->mon->mx + ws->mon->mw/2,
		             ws->mon->my + ws->mon->mh/2);

	/* update current workspace */
	wm.selmon->selws = ws;
	wm.selmon = ws->mon;
	wm.selmon->selws = ws;
	ewmh_setcurrentdesktop(wsnum);

	/* find which client to focus on the new current workspace */
	if (ws->nclients == 0)
		client_focus(ws->mon->sticky);
	else if (ws->focused)
		client_focus(ws->focused);
	else if (ws->floating)
		client_focus(ws->floating);
	else if (ws->col)
		client_focus(ws->col->row);
	else
		client_focus(NULL);
}

/* hide client */
void
client_hide(struct Client *c, int hide)
{
	int x, y, w, h;
	int incsize;

	if (c == NULL)
		return;

	client_getgeom(c, &x, &y, &w, &h);
	if (hide)
		x = (w + 2 * config.border_width) * -2;
	incsize = (c->isfullscreen || c->state & ISMAXIMIZED) ? NOTINCSIZE : INCSIZEWH;
	moveresize(c, x, y, w, h, incsize);
}

/* returns 1 if position x and y are on client's border, 0 otherwise */
int
client_isborder(struct Client *c, int x, int y)
{
	int w, h;

	if (c == NULL || c->state == ISMINIMIZED || c->isfullscreen)
		return 0;

	client_getgeom(c, NULL, NULL, &w, &h);

	if (x < 0 || x > w)
		return 1;
	if (y < 0 || y > h)
		return 1;
	return 0;
}

/* maximize client */
void
client_maximize(struct Client *c, int maximize)
{
	struct Column *col;

	if (c == NULL || (c->state & ISMINIMIZED) || c->isfixed || c->isfullscreen)
		return;

	/* make client unsticky, for it to be on current workspace */
	if (c->state & ISSTICKY)
		client_stick(c, 0);

	if (maximize && !(c->state & ISMAXIMIZED)) {
		c->layer = 0;

		client_del(c, 0, 0);

		if (c->ws->col == NULL) {
			c->prev = NULL;
			col = colalloc(NULL, NULL, c);
			c->ws->col = col;
		} else if (c->ws->col->next == NULL) {
			c->prev = NULL;
			col = colalloc(c->ws->col, NULL, c);
			c->ws->col->next = col;
		} else {
			struct Client *tmp;

			/* get the last column */
			for (col = c->ws->col; col->next; col = col->next)
				;

			if (col->row == NULL)
				errx(1, "found column with no client");

			/* get last item in column */
			for (tmp = col->row; tmp->next; tmp = tmp->next)
				;

			c->prev = tmp;
			tmp->next = c;
		}
		c->next = NULL;
		c->col = col;

		c->state = ISMAXIMIZED;
	} else if (!maximize && (c->state & ISMAXIMIZED)) {
		client_del(c, 0, 0);

		c->prev = NULL;
		c->next = c->ws->floating;
		c->col = NULL;

		if (c->ws->floating)
			c->ws->floating->prev = c;
		c->ws->floating = c;

		/* restore client's border width and stacking order (maximizing may change them) */
		XSetWindowBorderWidth(dpy, c->win, config.border_width);

		if (ISVISIBLE(c))
			moveresize(c, c->ux, c->uy, c->uw, c->uh, INCSIZEWH);

		c->state = ISNORMAL;
	}
	client_raise(c);
	client_tile(c->ws, 1);
}

/* minimize client */
void
client_minimize(struct Client *c, int minimize)
{
	int focus = 0;

	if (c == NULL)
		return;

	if (wm.selmon->selws == c->ws)
		focus = 1;

	if (c->state & ISMAXIMIZED)
		client_maximize(c, 0);

	if (minimize && !(c->state & ISMINIMIZED)) {
		client_del(c, 0, 1);
		if (wm.minimized)
			wm.minimized->prev = c;
		c->next = wm.minimized;
		wm.minimized = c;
		c->prev = NULL;

		/* focus another window */
		if (c->ws) {
			c->mon->focused = c->ws->focused = client_bestfocus(c);
			if (focus)
				client_focus(c->ws->focused);
		}

		c->ws = NULL;
		c->mon = NULL;
		c->state = ISMINIMIZED;
		ewmh_setwmdesktop();
	} else if (!minimize && (c->state & ISMINIMIZED)) {
		client_del(c, 0, 0);
		c->state = ISNORMAL;
		client_place(c, wm.selmon->selws);
		client_sendtows(c, wm.selmon->selws, 1, 1, 0);

		client_focus(c);
		client_raise(c);
	}

	client_hide(c, minimize);
}

/* move client x pixels to the right and y pixels down */
void
client_move(struct Client *c, int x, int y)
{
	if (c == NULL || c->state & ISMINIMIZED || c->isfullscreen)
		return;

	if (c->state & ISMAXIMIZED) {
		struct Column *col;

		col = c->col;
		if (x > 0) {
			if (c->next)
				c->next->prev = c->prev;
			if (c->prev)
				c->prev->next = c->next;
			else
				col->row = c->next;

			c->prev = NULL;
			if (col->next) {
				c->next = col->next->row;
				col->next->row->prev = c;
				col->next->row = c;
				c->mw = c->next->mw;
			} else {
				col->next = colalloc(col, NULL, c);
				c->next = NULL;
			}
			c->col = col->next;

			if (!col->row)
				colfree(col);
		} else if (x < 0) {
			if (c->next)
				c->next->prev = c->prev;
			if (c->prev)
				c->prev->next = c->next;
			else
				col->row = c->next;

			c->prev = NULL;
			if (col->prev) {
				c->next = col->prev->row;
				col->prev->row->prev = c;
				col->prev->row = c;
				c->mw = c->next->mw;
			} else {
				col->prev = colalloc(NULL, col, c);
				c->next = NULL;
			}
			c->col = col->prev;

			if (c->ws->col == col)
				c->ws->col = col->prev;

			if (!col->row)
				colfree(col);
		} else if (y < 0 && c->prev) {
			struct Client *pprev, *prev, *next;

			if (c->col->row == c->prev)
				c->col->row = c;

			pprev = c->prev->prev;
			prev = c->prev;
			next = c->next;

			c->prev->next = next;
			c->prev->prev = c;

			c->prev = pprev;
			c->next = prev;

			if (pprev)
				pprev->next = c;
			if (next)
				next->prev = prev;
		} else if (y > 0 && c->next) {
			struct Client *prev, *next, *nnext;

			if (c->col->row == c)
				c->col->row = c->next;

			prev = c->prev;
			next = c->next;
			nnext = c->next->next;

			c->next->prev = prev;
			c->next->next = c;

			c->prev = next;
			c->next = nnext;

			if (prev)
				prev->next = next;
			if (nnext)
				nnext->prev = c;

		} else {
			return;
		}
		client_tile(c->ws, 1);
	} else {
		c->ux += x;
		c->uy += y;

		if (ISVISIBLE(c))
			XMoveWindow(dpy, c->win, c->ux, c->uy);

		if (c->state & ISNORMAL) {
			struct Monitor *monto;  /* monitor to move the client to */

			monto = getmon(c->ux + c->uw / 2, c->uy + c->uh / 2);
			if (monto && monto != c->mon) {
				client_sendtows(c, monto->selws, 0, 0, 1);
				client_focus(c);
			}
		}
	}
}

/* test if window size is acceptable after adding x or y */
int
client_oksize(struct Client *c, int x, int y)
{
	int w, h;

	client_getgeom(c, NULL, NULL, &w, &h);

	w += x;
	h += y;

	if (x != 0)
		return w >= MINW(c) && (!c->maxw || w <= c->maxw);
	if (y != 0)
		return h >= MINH(c) && (!c->maxh || w <= c->maxh);
	return 1;
}

/* find best position to place a client on screen */
void
client_place(struct Client *c, struct WS *ws)
{
	struct Monitor *mon;
	struct Client *tmp;
	unsigned long grid[DIV][DIV] = {{0}, {0}};
	unsigned long lowest;
	int i, j, k, w, h;
	int posi, posj;         /* position of the larger region */
	int lw, lh;             /* larger region width and height */
	int checkedsticky;

	if (c == NULL || !(c->state & ISNORMAL))
		return;

	mon = ws->mon;

	if (c->ux < mon->wx || c->ux > mon->wx + mon->ww) {
		c->ux = mon->wx;
		c->isuserplaced = 0;
	}
	if (c->uy < mon->wy || c->uy > mon->wy + mon->wh) {
		c->uy = mon->wy;
		c->isuserplaced = 0;
	}
	c->uw = MIN(c->uw, mon->ww - 2 * config.border_width);
	c->uh = MIN(c->uh, mon->wh - 2 * config.border_width);
	applyaspectsize(c);

	/* if the user placed the window, we should not re-place it */
	if (c->isuserplaced)
		return;

	/* If this is the first floating window in the workspace, just center it */
	if (ws->floating == c && c->next == NULL && ws->mon->sticky == NULL) {
		c->ux = mon->wx + mon->ww/2 - WIDTH(c)/2;
		c->uy = mon->wy + mon->wh/2 - HEIGHT(c)/2;
		return;
	}

	/* increment cells of grid a window in */
	tmp = ws->floating;
	checkedsticky = 0;
	while (tmp) {

		for (i = 0; i < DIV; i++) {
			for (j = 0; j < DIV; j++) {
				int ha, hb, wa, wb;
				int ya, yb, xa, xb;

				ha = mon->wy + (mon->wh * i)/DIV;
				hb = mon->wy + (mon->wh * (i + 1))/DIV;
				wa = mon->wx + (mon->ww * j)/DIV;
				wb = mon->wx + (mon->ww * (j + 1))/DIV;

				ya = tmp->uy;
				yb = tmp->uy + HEIGHT(tmp);
				xa = tmp->ux;
				xb = tmp->ux + WIDTH(tmp);

				if (ya <= hb && ha <= yb && xa <= wb && wa <= xb) {
					grid[i][j]++;
					if (ya < ha && yb > hb)
						grid[i][j]++;
					if (xa < wa && xb > wb)
						grid[i][j]++;
				}
			}
		}

		/*
		 * after checking the floating windows on the workspace,
		 * check the sticky windows
		 */
		if (!checkedsticky && tmp->next == NULL) {
			tmp = ws->mon->sticky;
			checkedsticky = 1;
		} else {
			tmp = tmp->next;
		}
	}

	/* find biggest region in grid with less windows in it */
	lowest = grid[0][0];
	posi = posj = 0;
	lw = lh = 0;
	for (i = 0; i < DIV; i++) {
		for (j = 0; j < DIV; j++) {
			if (grid[i][j] > lowest)
				continue;
			else if (grid[i][j] < lowest)
				lowest = grid[i][j];
			for (w = 0; j+w < DIV && grid[i][j + w] == lowest; w++)
				;
			for (h = 1; i+h < DIV && grid[i + h][j] == lowest; h++) {
				for (k = 0; k < w && grid[i + h][j + k] == lowest; k++)
					;
				if (k < w)
					break;
			}
			if (k < w)
				h--;
			if (h > lh && w * h > lw * lh) {
				lw = w;
				lh = h;
				posi = i;
				posj = j;
			}
		}
	}

	posj += lw;
	posi += lh;

	/* calculate y */
	if (posi == 0) {
		c->uy = mon->wy;
	} else if (posi >= DIV - 1) {
		c->uy = mon->wy + mon->wh - HEIGHT(c);
	} else {
		int n, d, maxy;

		n = posi;
		d = DIV;
		maxy = mon->wy + mon->wh - HEIGHT(c);
		c->uy = (mon->wy + mon->wh * n)/d - HEIGHT(c);
		c->uy = MAX(mon->wy, c->uy);
		c->uy = MIN(c->uy, maxy);
	}

	/* calculate x */
	if (posj == 0) {
		c->ux = mon->wx;
	} else if (posj >= DIV - 1) {
		c->ux = mon->wx + mon->ww - WIDTH(c);
	} else {
		int n, d, maxx;

		n = posj;
		d = DIV;
		maxx = mon->wx + mon->ww - WIDTH(c);
		c->ux = (mon->wx + mon->ww * n)/d - WIDTH(c);
		c->ux = MAX(mon->wx, c->ux);
		c->ux = MIN(c->ux, maxx);
	}
}

/* get in which corner of the window the cursor is in */
enum Quadrant
client_quadrant(struct Client *c, int x, int y)
{
	int midw, midh;

	if (c == NULL || c->state == ISMINIMIZED)
		return SE;

	client_getgeom(c, NULL, NULL, &midw, &midh);
	midw /= 2;
	midh /= 2;

	if (x < midw && y < midh)
		return NW;
	if (x >= midw && y < midh)
		return NE;
	if (x < midw && y >= midh)
		return SW;
	else
		return SE;
}

/* raise client */
void
client_raise(struct Client *c)
{
	Window wins[2];

	if (c == NULL || c->state & ISMINIMIZED || c->isfullscreen)
		return;

	if (c->state & ISMAXIMIZED)
		wins[0] = layerwin[LayerTiled];
	else if (c->layer < 0)
		wins[0] = layerwin[LayerBelow];
	else if (c->layer > 0)
		wins[0] = layerwin[LayerAbove];
	else
		wins[0] = layerwin[LayerTop];

	wins[1] = c->win;
	XRestackWindows(dpy, wins, sizeof wins);
}

/* resize client x and y pixels out of quadrant q */
void
client_resize(struct Client *c, enum Quadrant q, int x, int y)
{
	if (c == NULL || c->isfixed || c->state & ISMINIMIZED || c->isfullscreen)
		return;

	if (c->state & ISMAXIMIZED) {
		switch (q) {
		case NW:
			if (c->col->prev &&
			    client_oksize(c->col->prev->row, -x, 0) &&
			    client_oksize(c->col->row, +x, 0)) {
				c->col->row->mw += x;
				c->col->prev->row->mw -= x;
			}
			if (c->prev &&
			    client_oksize(c->prev, 0, -y) &&
			    client_oksize(c, 0, +y)) {
				c->mh += y;
				c->prev->mh -= y;
			}
			break;
		case NE:
			if (c->col->next &&
			    client_oksize(c->col->next->row, -x, 0) &&
			    client_oksize(c->col->row, +x, 0)) {
				c->col->next->row->mw -= x;
				c->col->row->mw += x;
			}
			if (c->prev &&
			    client_oksize(c->prev, 0, -y) &&
			    client_oksize(c, 0, +y)) {
				c->mh += y;
				c->prev->mh -= y;
			}
			break;
		case SW:
			if (c->col->prev &&
			    client_oksize(c->col->prev->row, -x, 0) &&
			    client_oksize(c->col->row, +x, 0)) {
				c->col->row->mw += x;
				c->col->prev->row->mw -= x;
			}
			if (c->next &&
			    client_oksize(c->next, 0, -y) &&
			    client_oksize(c, 0, +y)) {
				c->next->mh -= y;
				c->mh += y;
			}
			break;
		case SE:
			if (c->col->next &&
			    client_oksize(c->col->next->row, -x, 0) &&
			    client_oksize(c->col->row, +x, 0)) {
				c->col->next->row->mw -= x;
				c->col->row->mw += x;
			}
			if (c->next &&
			    client_oksize(c->next, 0, -y) &&
			    client_oksize(c, 0, +y)) {
				c->next->mh -= y;
				c->mh += y;
			}
			break;
		}
		client_tile(c->ws, 0);
	} else {
		int origw, origh, incsize;

		if (!(client_oksize(c, +x, 0) && client_oksize(c, 0, +y)))
			return;
		switch (q) {
		case NW:
			applyincsize(c, &c->uw, &c->uh);
			origw = c->uw;
			origh = c->uh;
			c->uw += x;
			c->uh += y;
			applyaspectsize(c);
			applyincsize(c, &c->uw, &c->uh);
			c->ux -= c->uw - origw;
			c->uy -= c->uh - origh;
			incsize = NOTINCSIZE;
			break;
		case NE:
			applyincsize(c, NULL, &c->uh);
			origw = c->uw;
			origh = c->uh;
			c->uw += x;
			c->uh += y;
			applyaspectsize(c);
			applyincsize(c, NULL, &c->uh);
			c->uy -= c->uh - origh;
			incsize = INCSIZEW;
			break;
		case SW:
			applyincsize(c, &c->uw, NULL);
			origw = c->uw;
			origh = c->uh;
			c->uw += x;
			c->uh += y;
			applyaspectsize(c);
			applyincsize(c, &c->uw, NULL);
			c->ux -= c->uw - origw;
			incsize = INCSIZEH;
			break;
		case SE:
			origw = c->uw;
			origh = c->uh;
			c->uw += x;
			c->uh += y;
			applyaspectsize(c);
			incsize = INCSIZEWH;
			break;
		}
		if (ISVISIBLE(c))
			moveresize(c, c->ux, c->uy, c->uw, c->uh, incsize);
	}
}

/* send client to ws, delete from old ws if it isn't new and place it on scren if necessary */
void
client_sendtows(struct Client *c, struct WS *ws, int new, int place, int move)
{
	//struct Monitor *mon;
	struct WS *lastws;
	int createnewws = 0;    /* whether to create a new workspace */
	int focus = 0;

	if (c == NULL || ws == NULL || (c->state & ISFREE))
		return;

	if (wm.selmon->focused == c)
		focus = 1;

	/* find last workspace in this monitor */
	for (lastws = ws->mon->ws; lastws->next; lastws = lastws->next)
		;

	if (!new) { /* if the client already exists, delete it from its previous ws */
		if (ws == c->ws)
			return;

		if (c->state & ISMAXIMIZED)
			client_maximize(c, 0);

		if (!move && c->ws->mon != ws->mon)
			place = 1;

		if (!move) {
			c->mon->focused = c->ws->focused = client_bestfocus(c);
			if (wm.selmon->selws == c->ws) {
				client_hide(c, 1);
				client_focus(c->ws->focused);
			}
		}

		client_del(c, 0, 1);
	}

	/* add client to workspace's floating */
	if (ws->floating)
		ws->floating->prev = c;
	c->next = ws->floating;
	c->prev = NULL;

	ws->floating = c;
	if (ws->nclients == 0)
		createnewws = 1;
	ws->nclients++;
	if (focus)
		ws->focused = c;

	c->ws = ws;
	c->mon = ws->mon;

	if (ws == lastws && createnewws) {
		ws_add(ws->mon);
		ewmh_setnumberofdesktops();
	}
	ewmh_setwmdesktop();

	if (place) {
		client_place(c, ws);
		if (ws->mon->selws == ws) {
			moveresize(c, c->ux, c->uy, c->uw, c->uh, INCSIZEWH);
		}
	}

	client_raise(c);
	XSync(dpy, False);
}

/* set client border */
void
client_setborder(struct Client *c, unsigned long color)
{
	if (c == NULL)
		return;

	XSetWindowBorder(dpy, c->win, color);
}

/* hide all windows and show the desktop */
void
client_showdesktop(int n)
{
	struct Column *col;
	struct Client *c;

	showingdesk = n;

	if (n) {    /* show desktop */
		for (c = wm.selmon->sticky; c; c = c->next)
			client_hide(c, 1);
		for (c = wm.selmon->selws->floating; c; c = c->next)
			client_hide(c, 1);
		for (col = wm.selmon->selws->col; col; col = col->next)
			for (c = col->row; c; c = c->next)
				client_hide(c, 1);

		ewmh_setshowingdesktop(1);
	} else {    /* show hidden windows */
		for (c = wm.selmon->sticky; c; c = c->next)
			client_hide(c, 0);
		for (c = wm.selmon->selws->floating; c; c = c->next)
			client_hide(c, 0);
		for (col = wm.selmon->selws->col; col; col = col->next)
			for (c = col->row; c; c = c->next)
				client_hide(c, 0);

		ewmh_setshowingdesktop(0);
	}
}

/* stick/unstick client */
void
client_stick(struct Client *c, int stick)
{
	struct WS *ws;

	if (c == NULL || (c->state & (ISMINIMIZED)))
		return;

	/* maximized and fullscreen windows should not go sticky */
	if (c->state & ISMAXIMIZED || c->isfullscreen)
		return;

	if (stick && !(c->state & ISSTICKY)) {
		client_del(c, 0, 1);
		if (c->mon->sticky)
			c->mon->sticky->prev = c;
		c->next = c->mon->sticky;
		c->mon->sticky = c;
		c->state = ISSTICKY;
		c->prev = NULL;
		c->ws = NULL;
		if (wm.selmon->focused == c)
			for (ws = c->mon->ws; ws; ws = ws->next)
				ws->focused = c;
		ewmh_setwmdesktop();
	} else if (!stick && (c->state & ISSTICKY)) {
		struct WS *ws;

		/* if other workspaces' focus is on c, change it */
		for (ws = c->mon->ws; ws; ws = ws->next)
			if (ws != wm.selmon->selws && ws->focused == c)
				ws->mon->focused = ws->focused = NULL;

		client_del(c, 0, 0);
		c->state = ISNORMAL;
		client_sendtows(c, wm.selmon->selws, 1, 0, 0);
	}
}

/* retile clients */
void
client_tile(struct WS *ws, int recalc)
{
	struct Monitor *mon;
	struct Column *col;
	struct Client *c;
	int recol = 0, rerow = 0;   /* whether to resize columns and rows */
	int sumw = 0, sumh = 0;
	int zerow = 0, zeroh = 0;
	int ncol, nrow;
	int x, y;
	int w, h;

	mon = ws->mon;

	/* get number of columns and sum their widths */
	for (ncol = 0, col = ws->col; col; col = col->next, ncol++) {
		sumw += col->row->mw + config.border_width * 2;
		if (col->row->mw == 0)
			zerow = 1;
	}
	sumw += config.gapinner * (ncol-1);

	if (ncol == 0)
		return;

	/* decides whether to recalculate column widths */
	recol = 0;
	if (!(recalc && sumw != mon->ww) || (sumw == mon->ww && !zerow))
		recol = 1;

	w = (mon->ww - config.gapinner * (ncol - 1) - config.border_width * 2 * ncol) / ncol;
	x = mon->wx;
	for (col = ws->col; col; col = col->next) {
		/* the last column gets the remaining width */
		if (!col->next)
			w = mon->ww + mon->wx - x - 2 * config.border_width;

		/* get number of clients in current column and sum their heights */
		for (nrow = 0, c = col->row; c; c = c->next, nrow++) {
			sumh += c->mh + config.border_width * 2;
			if (c->mh == 0)
				zeroh = 1;
		}
		if (nrow == 0)
			continue;
		sumh += config.gapinner * (nrow-1);

		/* decides whether to recalculate client heights */
		rerow = 0;
		if (!(recalc && sumh != mon->wh) || (sumh == mon->wh && !zeroh))
			rerow = 1;

		h = (mon->wh - config.gapinner * (nrow - 1) - config.border_width * 2 * nrow) / nrow;
		y = mon->wy;
		for (c = col->row; c; c = c->next) {
			if (c->isfullscreen)
				continue;

			/* the last client gets the remaining height */
			if (!c->next)
				h = mon->wh + mon->wy - y - 2 * config.border_width;

			c->mx = x;
			c->my = y;
			if (!recol || !col->next)
				c->mw = w;
			else
				c->mw = col->row->mw;
			if (!rerow || !c->next)
				c->mh = h;

			/* if there is only one tiled window, it is subject to bflag */
			if (nrow == 1 && ncol == 1) {
				if (bflag) {
					c->mh += 2 * config.border_width;
					c->mw += 2 * config.border_width;
					XSetWindowBorderWidth(dpy, c->win, 0);
				}
				if (gflag) {
					c->mx = mon->dx;
					c->my = mon->dy;
					c->mw = mon->dw - ((!bflag) ? 2 * config.border_width : 0);
					c->mh = mon->dh - ((!bflag) ? 2 * config.border_width : 0);
				}
			} else {
				XSetWindowBorderWidth(dpy, c->win, config.border_width);
			}

			if (ISVISIBLE(c))
				moveresize(c, c->mx, c->my, c->mw, c->mh, NOTINCSIZE);

			y += c->mh + config.gapinner + config.border_width * 2;
		}

		x += col->row->mw + config.gapinner + config.border_width * 2;
	}
}

/* remove client from focus history */
void
client_unfocus(struct Client *c)
{
	if (wm.selmon->focused == c)
		wm.selmon->focused = c->fnext;
	if (c->fnext)
		c->fnext->fprev = c->fprev;
	if (c->fprev)
		c->fprev->fnext = c->fnext;
}

/* apply size hints for unmaximized window */
static void
applyaspectsize(struct Client *c)
{
	if (c == NULL)
		return;

	if (c->mina > 0 && c->maxa > 0) {
		if (c->maxa < (float)c->uw/c->uh) {
			if (c->uw == MAX(c->uw, c->uh)) {
				c->uh = c->uw / c->maxa - 0.5;
			} else {
				c->uw = c->uh * c->maxa + 0.5;
			}
		} else if (c->mina < (float)c->uh/c->uw) {
			if (c->uh == MAX(c->uw, c->uh)) {
				c->uw = c->uh / c->mina - 0.5;
			} else {
				c->uh = c->uw * c->mina + 0.5;
			}
		}
	}
}

/* apply size hints for unmaximized window */
static void
applyincsize(struct Client *c, int *w, int *h)
{
	if (w && c->incw > 0 && c->basew < *w) {
		*w -= c->basew;
		*w /= c->incw;
		*w *= c->incw;
		*w += c->basew;
	}
	if (h && c->inch > 0 && c->baseh < *h) {
		*h -= c->baseh;
		*h /= c->inch;
		*h *= c->inch;
		*h += c->baseh;
	}
}

/* create a column */
static struct Column *
colalloc(struct Column *prev, struct Column *next, struct Client *c)
{
	struct Column *col;

	if (c == NULL || c->ws == NULL)
		errx(1, "trying to create column with improper client");

	if ((col = malloc(sizeof *col)) == NULL)
		err(1, "malloc");

	col->next = next;
	col->prev = prev;
	col->row = c;
	col->ws = c->ws;

	return col;
}

/* free a column */
static void
colfree(struct Column *col)
{
	struct WS *ws;

	if (col == NULL)
		return;

	ws = col->ws;

	if (ws->col == col) {
		if (col->prev)
			errx(1, "there is a prev column for the first column");
		ws->col = col->next;
		if (col->next)
			col->next->prev = NULL;
	} else {
		if (col->next)
			col->next->prev = col->prev;
		col->prev->next = col->next;
	}

	free(col);
}

/* call XMoveResizeWindow and apply size increment, depending on incsize */
static void
moveresize(struct Client *c, int x, int y, int w, int h, int incsize)
{
	if (incsize & INCSIZEW && c->incw > 0 && c->basew < w) {
		w -= c->basew;
		w /= c->incw;
		w *= c->incw;
		w += c->basew;
	}
	if (incsize & INCSIZEH && c->inch > 0 && c->baseh < h) {
		h -= c->baseh;
		h /= c->inch;
		h *= c->inch;
		h += c->baseh;
	}
	XMoveResizeWindow(dpy, c->win, x, y, w, h);
}

/* get cursor position */
static void
querypointer(int *x_ret, int *y_ret)
{
	Window da, db;
	int dx, dy;
	unsigned dm;

	XQueryPointer(dpy, root, &da, &db, x_ret, y_ret, &dx, &dy, &dm);
}

/* set client size hints */
static long
setsizehints(struct Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;

	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;

	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;

	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;

	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;

	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;

	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);

	return size.flags;
}
