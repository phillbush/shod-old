#include "shod.h"
#include "client.h"
#include "ewmh.h"
#include "monitor.h"
#include "workspace.h"
#include "manage.h"

enum MotionAction {NoAction, Moving, Resizing};

/* function declarations */
static void xevent_buttonpress(XEvent *);
static void xevent_buttonrelease(XEvent *);
static void xevent_clientmessage(XEvent *);
static void xevent_configurenotify(XEvent *e);
static void xevent_configurerequest(XEvent *);
static void xevent_destroynotify(XEvent *);
static void xevent_enternotify(XEvent *);
static void xevent_focusin(XEvent *);
static void xevent_maprequest(XEvent *);
static void xevent_motionnotify(XEvent *);
static void xevent_unmapnotify(XEvent *);

/* global variables */
static int motionx = 0, motiony = 0;
static enum MotionAction motionaction = NoAction;
static enum Quadrant quadrant = SE;
static void (*xevents[LASTEvent])(XEvent *) = {
	[ButtonPress]      = xevent_buttonpress,
	[ButtonRelease]    = xevent_buttonrelease,
	[ClientMessage]    = xevent_clientmessage,
	[ConfigureNotify]  = xevent_configurenotify,
	[ConfigureRequest] = xevent_configurerequest,
	[DestroyNotify]    = xevent_destroynotify,
	[EnterNotify]      = xevent_enternotify,
	[FocusIn]          = xevent_focusin,
	[MapRequest]       = xevent_maprequest,
	[MotionNotify]     = xevent_motionnotify,
	[UnmapNotify]      = xevent_unmapnotify
};

/* focus window when clicking on it, and activate moving/resizing */
static void
xevent_buttonpress(XEvent *e)
{
	XButtonPressedEvent *ev = &e->xbutton;
	int isborder = 0;
	struct Client *c;
	int focus = 0;

	/* if user clicked in no window, focus the monitor below cursor */
	if ((c = getclient(ev->window)) == NULL) {
		struct Monitor *mon;
		mon = getmon(ev->x_root, ev->y_root);
		if (mon)
			client_gotows(mon->selws, 0);
		goto done;
	}

	/* initiate motion action */
	isborder = client_isborder(c, ev->x, ev->y);
	if ((ev->state == config.modifier || isborder)
	    && (ev->button == Button1 || ev->button == Button3)) {
		Cursor curs = None;
		if (isborder || ev->button == Button3) {
			quadrant = client_quadrant(c, ev->x, ev->y);
			motionaction = Resizing;
			switch (quadrant) {
			case NW: curs = cursor[CursNW]; break;
			case NE: curs = cursor[CursNE]; break;
			case SW: curs = cursor[CursSW]; break;
			case SE: curs = cursor[CursSE]; break;
			}
		} else {
			motionaction = Moving;
			curs = cursor[CursMove];
		}
		XGrabPointer(dpy, c->win, False,
		             ButtonReleaseMask | Button1MotionMask | Button3MotionMask,
			         GrabModeAsync, GrabModeAsync, None, curs, CurrentTime);
		motionx = ev->x;
		motiony = ev->y;
	}

	/* focus client */
	if (ev->button == Button1 && config.focusbuttons & 1 << 1)
		focus = 1;
	else if (ev->button == Button2 && config.focusbuttons & 1 << 2)
		focus = 1;
	else if (ev->button == Button3 && config.focusbuttons & 1 << 3)
		focus = 1;
	else if (ev->button == Button4 && config.focusbuttons & 1 << 4)
		focus = 1;
	else if (ev->button == Button5 && config.focusbuttons & 1 << 5)
		focus = 1;
	if (focus) {
		client_focus(c);
	}

	/* raise client */
	if (ev->button == Button1 && config.raisebuttons & 1 << 1)
		focus = 1;
	else if (ev->button == Button2 && config.raisebuttons & 1 << 2)
		focus = 1;
	else if (ev->button == Button3 && config.raisebuttons & 1 << 3)
		focus = 1;
	else if (ev->button == Button4 && config.raisebuttons & 1 << 4)
		focus = 1;
	else if (ev->button == Button5 && config.raisebuttons & 1 << 5)
		focus = 1;
	if (focus && c->state & ISFLOATING)
		client_raise(c);

done:
	XAllowEvents(dpy, ReplayPointer, CurrentTime);
}

/* interrupts moving/resizing action */
static void
xevent_buttonrelease(XEvent *e)
{
	(void)e;
	XUngrabPointer(dpy, CurrentTime);
	motionaction = NoAction;
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */

/* handle client message event */
static void
xevent_clientmessage(XEvent *e)
{
	XClientMessageEvent *ev = &e->xclient;
	struct Client *c;

	c = getclient(ev->window);

	if (ev->message_type == netatom[NetCurrentDesktop]) {
		client_gotows(NULL, ev->data.l[0]);
	} else if (ev->message_type == netatom[NetShowingDesktop]) {
		if (ev->data.l[0])
			client_showdesktop(1);
		else
			client_focus(selmon->selws->focused);
	} else if (ev->message_type == netatom[NetRequestFrameExtents]) {
		ewmh_setframeextents(ev->window);
	} else if (ev->message_type == netatom[NetWMState]) {
		if (c == NULL)
			return;

		/*
		 * ev->data.l[0] == 0: _NET_WM_STATE_REMOVE
		 * ev->data.l[0] == 1: _NET_WM_STATE_ADD
		 * ev->data.l[0] == 2: _NET_WM_STATE_TOGGLE
		 */
		if ((Atom) ev->data.l[1] == netatom[NetWMStateMaximizedVert]
		    || (Atom) ev->data.l[1] == netatom[NetWMStateMaximizedHorz]
		    || (Atom) ev->data.l[2] == netatom[NetWMStateMaximizedVert]
		    || (Atom) ev->data.l[2] == netatom[NetWMStateMaximizedHorz])
			client_maximize(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !(c->state & ISMAXIMIZED))));
		else if ((Atom) ev->data.l[1] == netatom[NetWMStateFullscreen]
		         || (Atom) ev->data.l[2] == netatom[NetWMStateFullscreen])
			client_fullscreen(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !c->isfullscreen)));
		else if ((Atom) ev->data.l[1] == netatom[NetWMStateSticky]
		         || (Atom) ev->data.l[2] == netatom[NetWMStateSticky])
			client_stick(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !(c->state & ISSTICKY))));
		else if ((Atom) ev->data.l[1] == netatom[NetWMStateHidden]
		         || (Atom) ev->data.l[2] == netatom[NetWMStateHidden])
			client_minimize(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !(c->state & ISMINIMIZED))));
		else if ((Atom) ev->data.l[1] == netatom[NetWMStateAbove]
		         || (Atom) ev->data.l[2] == netatom[NetWMStateAbove])
			client_above(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && (c->layer <= 0))));
		else if ((Atom) ev->data.l[1] == netatom[NetWMStateBelow]
		         || (Atom) ev->data.l[2] == netatom[NetWMStateBelow])
			client_below(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && (c->layer >= 0))));
		ewmh_setstate(c);
	} else if (ev->message_type == netatom[NetActiveWindow]) {
		if (c == NULL)
			return;

		if (c->state & ISMINIMIZED) {
			client_minimize(c, 0);
		} else {
			client_gotows(c->ws, 0);
			client_focus(c);
			client_raise(c);
		}
	} else if (ev->message_type == netatom[NetCloseWindow]) {
		if (c == NULL)
			return;

		client_close(c);
	} else if (ev->message_type == netatom[NetMoveresizeWindow]) {
		XWindowChanges wc;
		unsigned value_mask = 0;

		if (c == NULL)
			return;

		if (ev->data.l[0] & 1 << 8) {
			wc.x = ev->data.l[1];
			value_mask |= CWX;
		}
		if (ev->data.l[0] & 1 << 9) {
			wc.y = ev->data.l[2];
			value_mask |= CWY;
		}
		if (ev->data.l[0] & 1 << 10) {
			wc.width = ev->data.l[3];
			value_mask |= CWWidth;
		}
		if (ev->data.l[0] & 1 << 11) {
			wc.height = ev->data.l[4];
			value_mask |= CWHeight;
		}

		client_configure(c, wc, value_mask);
	} else if (ev->message_type == netatom[NetWMDesktop]) {
		if (c == NULL)
			return;

		if (ev->data.l[0] == 0xFFFFFFFF)
			client_stick(c, 1);
		else if (c->state & ISBOUND)
			client_sendtows(c, getws(ev->data.l[0]), 0, 0, 0);
	} else if (ev->message_type == netatom[NetWMMoveresize]) {
		/*
		 * Client-side decorated Gtk3 windows emit this signal when being
		 * dragged by their GtkHeaderBar
		 */
		if (c == NULL)
			return;

		if (ev->data.l[2] == _NET_WM_MOVERESIZE_CANCEL) {
			XUngrabPointer(dpy, CurrentTime);
			motionaction = NoAction;
		} else {
			Cursor curs = None;
			Window dw;          /* dummy variable */

			switch (ev->data.l[2]) {
			case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
				quadrant = SE;
				curs = cursor[CursSE];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
				quadrant = SW;
				curs = cursor[CursSW];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
				quadrant = NW;
				curs = cursor[CursNW];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
				quadrant = NE;
				curs = cursor[CursNE];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_MOVE:
				curs = cursor[CursMove];
				motionaction = Moving;
				break;
			default:
				return;
			}
			if (XTranslateCoordinates(dpy, root, c->win,
			                          ev->data.l[0], ev->data.l[1],
			                          &motionx, &motiony, &dw) == False) {
				motionx = 0;
				motiony = 0;
			}
			XGrabPointer(dpy, c->win, False,
			             ButtonReleaseMask | Button1MotionMask | Button3MotionMask,
			             GrabModeAsync, GrabModeAsync, None, curs, CurrentTime);
		}
	}
}

/* handle configure notify event */
static void
xevent_configurenotify(XEvent *e)
{
	XConfigureEvent *ev = &e->xconfigure;

	if (ev->window == root) {
		screenw = ev->width;
		screenh = ev->height;
		monitor_update();
		ewmh_setnumberofdesktops();
	}
}

/* handle configure request event */
static void
xevent_configurerequest(XEvent *e)
{
	XWindowChanges wc;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	struct Client *c;

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = ev->border_width;
	wc.sibling = ev->above;
	wc.stack_mode = ev->detail;

	if ((c = getclient(ev->window))) {
		client_configure(c, wc, ev->value_mask);
	} else {
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
}

/* forget about client */
static void
xevent_destroynotify(XEvent *e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	unmanage(ev->window);
}

/* focus window when cursor enter it, if fflag is set */
static void
xevent_enternotify(XEvent *e)
{
	struct Client *c;

	if (config.focusbuttons)
		return;

	while(XCheckTypedEvent(dpy, EnterNotify, e))
		;

	if ((c = getclient(e->xcrossing.window)))
		client_focus(c);
}

static void
xevent_focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->focused && (selmon->focused->win != ev->window
	                    || ev->detail == NotifyPointer
	                    || ev->detail == NotifyPointerRoot))
		client_focus(selmon->focused);
}

/* handle map request event */
static void
xevent_maprequest(XEvent *e)
{
	XMapRequestEvent *ev = &e->xmaprequest;

	manage(ev->window);
}

/* run moving/resizing action */
static void
xevent_motionnotify(XEvent *e)
{
	XMotionEvent *ev = &e->xmotion;
	struct Client *c;
	int x, y;

	if ((c = getclient(ev->window)) == NULL || motionaction == NoAction)
		return;

	if (motionaction == Resizing) {
		switch (quadrant) {
		case NW:
			x = motionx - ev->x;
			y = motiony - ev->y;
			break;
		case NE:
			x = ev->x - motionx;
			y = motiony - ev->y;
			motionx = ev->x;
			break;
		case SW:
			x = motionx - ev->x;
			y = ev->y - motiony;
			motiony = ev->y;
			break;
		case SE:
			x = ev->x - motionx;
			y = ev->y - motiony;
			motionx = ev->x;
			motiony = ev->y;
			break;
		}
		client_resize(c, quadrant, x, y);
	} else if (motionaction == Moving) {
		if (c->state & ISMAXIMIZED)
			return;
		x = ev->x - motionx;
		y = ev->y - motiony;
		client_move(c, x, y);
	}
}

/* forget about client */
static void
xevent_unmapnotify(XEvent *e)
{
	XUnmapEvent *ev = &e->xunmap;

	unmanage(ev->window);
}

/* the main event loop */
void
xevent_run(void)
{
	XEvent ev;

	XMoveWindow(dpy, focuswin, -1, 0);
	XMapWindow(dpy, focuswin);
	while (running && !XNextEvent(dpy, &ev))
		if (xevents[ev.type])
			xevents[ev.type](&ev);
}
