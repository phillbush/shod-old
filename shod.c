#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xinerama.h>
#include "shod.h"

/* X stuff */
static Display *dpy;
static Window root;
static XrmDatabase xdb;
static GC gc;
static char *xrm;
static int screen, screenw, screenh;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static Atom atoms[AtomLast];

/* visual */
static struct Colors colors;
static Cursor cursor[CursLast];

/* mouse manipulation variables */
static int motionx = -1, motiony = -1;
static int motionaction = NoAction;
static enum Octant octant = SE;

/* dummy windows */
static Window wmcheckwin;
static Window focuswin;                /* dummy window to get focus */
static Window layerwin[LayerLast];     /* dummy windows used to restack clients */

/* windows, desktops, monitors */
static struct Client *clients = NULL;
static struct Client *focused = NULL;
static struct Monitor *selmon = NULL;
static struct Monitor *mons = NULL;
static unsigned long deskcount = 0;
static int showingdesk = 0;

/* other variables */
volatile sig_atomic_t running = 1;

/* include default configuration */
#include "config.h"

/* get maximum */
static int
max(int x, int y)
{
	return x > y ? x : y;
}

/* get minimum */
static int
min(int x, int y)
{
	return x < y ? x : y;
}

/* call malloc checking for error */
static void *
emalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(1, "malloc");
	return p;
}

/* call calloc checking for error */
static void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL)
		err(1, "malloc");
	return p;
}

/* alloc color pixel from color name */
static unsigned long
ealloccolor(const char *s)
{
	XColor color;

	if(!XAllocNamedColor(dpy, DefaultColormap(dpy, screen), s, &color, &color))
		errx(1, "cannot allocate color: %s", s);
	return color.pixel;
}

/* get atom property from window */
static Atom
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

/* get array of unsigned longs from window */
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

/* get configuration from X resources */
static void
getresources(void)
{
	XrmValue xval;
	long n;
	char *type;

	if (XrmGetResource(xdb, "shod.borderWidth", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.border_width = n;
	if (XrmGetResource(xdb, "shod.gapInner", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapinner = n;
	if (XrmGetResource(xdb, "shod.gapOuter", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapouter = n;
	if (XrmGetResource(xdb, "shod.ignoreGaps", "*", &type, &xval) == True)
		config.ignoregaps = (strcasecmp(xval.addr, "true") == 0 ||
		                     strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.ignoreBorders", "*", &type, &xval) == True)
		config.ignoreborders = (strcasecmp(xval.addr, "true") == 0 ||
		                        strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.urgent", "*", &type, &xval) == True)
		config.urgent_color = xval.addr;
	if (XrmGetResource(xdb, "shod.focused", "*", &type, &xval) == True)
		config.focused_color = xval.addr;
	if (XrmGetResource(xdb, "shod.unfocused", "*", &type, &xval) == True)
		config.unfocused_color = xval.addr;
}

/* get window's WM_STATE property */
static long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, atoms[WMState], 0L, 2L, False, atoms[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

/* get desktop pointer from desktop index */
static struct Desktop *
getdesk(unsigned long n)
{
	struct Monitor *mon;
	struct Desktop *desk;

	if (n >= deskcount)
		return NULL;
	for (mon = mons; mon; mon = mon->next) {
		for (desk = mon->desks; desk; desk = desk->next) {
			if (n == 0) {
				return desk;
			} else {
				n--;
			}
		}
	}
	return NULL;
}

/* get desktop index from desktop pointer */
static unsigned long
getdesknum(struct Desktop *desk)
{
	struct Monitor *mon;
	struct Desktop *tmp;
	unsigned long n = 0;

	for (mon = mons; mon; mon = mon->next) {
		for (tmp = mon->desks; tmp; tmp = tmp->next) {
			if (desk == tmp) {
				goto done;
			} else {
				n++;
			}
		}
	}
done:
	return n;
}

/* error handler */
static int
xerror(Display *dpy, XErrorEvent *e)
{
	/* stolen from berry, which stole from katriawm, which stole from dwm lol */

	/* There's no way to check accesses to destroyed windows, thus those
	 * cases are ignored (especially on UnmapNotify's). Other types of
	 * errors call Xlibs default error handler, which may call exit. */
	if (e->error_code == BadWindow ||
	    (e->request_code == X_SetInputFocus && e->error_code == BadMatch) ||
	    (e->request_code == X_PolyText8 && e->error_code == BadDrawable) ||
	    (e->request_code == X_PolyFillRectangle && e->error_code == BadDrawable) ||
	    (e->request_code == X_PolySegment && e->error_code == BadDrawable) ||
	    (e->request_code == X_ConfigureWindow && e->error_code == BadMatch) ||
	    (e->request_code == X_GrabButton && e->error_code == BadAccess) ||
	    (e->request_code == X_GrabKey && e->error_code == BadAccess) ||
	    (e->request_code == X_CopyArea && e->error_code == BadDrawable) ||
	    (e->request_code == 139 && e->error_code == BadDrawable) ||
	    (e->request_code == 139 && e->error_code == 143))
		return 0;

	errx(1, "Fatal request. Request code=%d, error code=%d", e->request_code, e->error_code);
	return xerrorxlib(dpy, e);
}

/* stop running */
static void
siginthandler(int signo)
{
	(void)signo;
	running = 0;
}

/* initialize signals */
static void
initsignal(void)
{
	struct sigaction sa;

	/* remove zombies, we may inherit children when exec'ing shod in .xinitrc */
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
		err(1, "sigaction");

	/* set running to 0 */
	sa.sa_handler = siginthandler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1)
		err(1, "sigaction");
}

/* create dummy windows used for controlling focus and the layer of clients */
static void
initdummywindows(void)
{
	XSetWindowAttributes swa;
	int i;

	swa.do_not_propagate_mask = NoEventMask;
	swa.event_mask = KeyPressMask;
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	focuswin = XCreateWindow(dpy, root, 0, 0, 1, 1, 0,
	                         CopyFromParent, CopyFromParent, CopyFromParent,
	                         CWDontPropagate | CWEventMask, &swa);
	for (i = 0; i < LayerLast; i++)
		layerwin[i] = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
}

/* Initialize cursors */
static void
initcursors(void)
{
	cursor[CursNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CursMove] = XCreateFontCursor(dpy, XC_fleur);
	cursor[CursNW] = XCreateFontCursor(dpy, XC_top_left_corner);
	cursor[CursNE] = XCreateFontCursor(dpy, XC_top_right_corner);
	cursor[CursSW] = XCreateFontCursor(dpy, XC_bottom_left_corner);
	cursor[CursSE] = XCreateFontCursor(dpy, XC_bottom_right_corner);
	cursor[CursN] = XCreateFontCursor(dpy, XC_top_side);
	cursor[CursS] = XCreateFontCursor(dpy, XC_bottom_side);
	cursor[CursW] = XCreateFontCursor(dpy, XC_left_side);
	cursor[CursE] = XCreateFontCursor(dpy, XC_right_side);
}

/* initialize colors array */
static void
initcolors(void)
{
	colors.unfocused = ealloccolor(config.unfocused_color);
	colors.focused = ealloccolor(config.focused_color);
	colors.urgent = ealloccolor(config.urgent_color);
}

/* initialize atom arrays */
static void
initatoms(void)
{
	char *atomnames[AtomLast] = {
		[Utf8String]                 = "UTF8_STRING",

		[WMDeleteWindow]             = "WM_DELETE_WINDOW",
		[WMTakeFocus]                = "WM_TAKE_FOCUS",
		[WMProtocols]                = "WM_PROTOCOLS",
		[WMState]                    = "WM_STATE",

		[NetSupported]               = "_NET_SUPPORTED",
		[NetClientList]              = "_NET_CLIENT_LIST",
		[NetClientListStacking]      = "_NET_CLIENT_LIST_STACKING",
		[NetNumberOfDesktops]        = "_NET_NUMBER_OF_DESKTOPS",
		[NetCurrentDesktop]          = "_NET_CURRENT_DESKTOP",
		[NetActiveWindow]            = "_NET_ACTIVE_WINDOW",
		[NetWorkarea]                = "_NET_WORKAREA",
		[NetSupportingWMCheck]       = "_NET_SUPPORTING_WM_CHECK",
		[NetShowingDesktop]          = "_NET_SHOWING_DESKTOP",
		[NetCloseWindow]             = "_NET_CLOSE_WINDOW",
		[NetMoveresizeWindow]        = "_NET_MOVERESIZE_WINDOW",
		[NetWMMoveresize]            = "_NET_WM_MOVERESIZE",
		[NetRequestFrameExtents]     = "_NET_REQUEST_FRAME_EXTENTS",
		[NetWMName]                  = "_NET_WM_NAME",
		[NetWMWindowType]            = "_NET_WM_WINDOW_TYPE",
		[NetWMWindowTypeDesktop]     = "_NET_WM_WINDOW_TYPE_DESKTOP",
		[NetWMWindowTypeDock]        = "_NET_WM_WINDOW_TYPE_DOCK",
		[NetWMWindowTypeToolbar]     = "_NET_WM_WINDOW_TYPE_TOOLBAR",
		[NetWMWindowTypeMenu]        = "_NET_WM_WINDOW_TYPE_MENU",
		[NetWMWindowTypeSplash]      = "_NET_WM_WINDOW_TYPE_SPLASH",
		[NetWMWindowTypeDialog]      = "_NET_WM_WINDOW_TYPE_DIALOG",
		[NetWMWindowTypeUtility]     = "_NET_WM_WINDOW_TYPE_UTILITY",
		[NetWMState]                 = "_NET_WM_STATE",
		[NetWMStateSticky]           = "_NET_WM_STATE_STICKY",
		[NetWMStateMaximizedVert]    = "_NET_WM_STATE_MAXIMIZED_VERT",
		[NetWMStateMaximizedHorz]    = "_NET_WM_STATE_MAXIMIZED_HORZ",
		[NetWMStateShaded]           = "_NET_WM_STATE_SHADED",
		[NetWMStateHidden]           = "_NET_WM_STATE_HIDDEN",
		[NetWMStateFullscreen]       = "_NET_WM_STATE_FULLSCREEN",
		[NetWMStateAbove]            = "_NET_WM_STATE_ABOVE",
		[NetWMStateBelow]            = "_NET_WM_STATE_BELOW",
		[NetWMStateFocused]          = "_NET_WM_STATE_FOCUSED",
		[NetWMAllowedActions]        = "_NET_WM_ALLOWED_ACTIONS",
		[NetWMActionMove]            = "_NET_WM_ACTION_MOVE",
		[NetWMActionResize]          = "_NET_WM_ACTION_RESIZE",
		[NetWMActionMinimize]        = "_NET_WM_ACTION_MINIMIZE",
		[NetWMActionStick]           = "_NET_WM_ACTION_STICK",
		[NetWMActionMaximizeHorz]    = "_NET_WM_ACTION_MAXIMIZE_HORZ",
		[NetWMActionMaximizeVert]    = "_NET_WM_ACTION_MAXIMIZE_VERT",
		[NetWMActionFullscreen]      = "_NET_WM_ACTION_FULLSCREEN",
		[NetWMActionChangeDesktop]   = "_NET_WM_ACTION_CHANGE_DESKTOP",
		[NetWMActionClose]           = "_NET_WM_ACTION_CLOSE",
		[NetWMActionAbove]           = "_NET_WM_ACTION_ABOVE",
		[NetWMActionBelow]           = "_NET_WM_ACTION_BELOW",
		[NetWMStrut]                 = "_NET_WM_STRUT",
		[NetWMStrutPartial]          = "_NET_WM_STRUT_PARTIAL",
		[NetWMUserTime]              = "_NET_WM_USER_TIME",
		[NetWMStateAttention]        = "_NET_WM_STATE_DEMANDS_ATTENTION",
		[NetWMDesktop]               = "_NET_WM_DESKTOP",
		[NetFrameExtents]            = "_NET_FRAME_EXTENTS",
		[NetDesktopViewport]         = "_NET_DESKTOP_VIEWPORT",
	};

	XInternAtoms(dpy, atomnames, AtomLast, False, atoms);
}

static void
icccmwmstate(Window win, int state)
{
	long data[2];

	data[0] = state;
	data[1] = None;

	XChangeProperty(dpy, win, atoms[WMState], atoms[WMState], 32,
	                PropModeReplace, (unsigned char *)&data, 2);
}

static void
ewmhinit(void)
{
	unsigned long data[2];

	/* Set window and property that indicates that the wm is ewmh compliant */
	XChangeProperty(dpy, wmcheckwin, atoms[NetSupportingWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, atoms[NetWMName], atoms[Utf8String], 8, PropModeReplace, (unsigned char *) "shod", strlen("shod"));
	XChangeProperty(dpy, root, atoms[NetSupportingWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);

	/* Set properties that the window manager supports */
	XChangeProperty(dpy, root, atoms[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, AtomLast);
	XDeleteProperty(dpy, root, atoms[NetClientList]);

	/* This wm does not support viewports */
	data[0] = data[1] = 0;
	XChangeProperty(dpy, root, atoms[NetDesktopViewport], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 2);
}

static void
ewmhsetallowedactions(Window win)
{
	XChangeProperty(dpy, win, atoms[NetWMAllowedActions],
	                XA_ATOM, 32, PropModeReplace,
	                (unsigned char *)&atoms[NetWMActionMove], 11);
	/*
	 * 11 is the number of actions supported, and NetWMActionMove is the
	 * first of them.  See the EWMH atoms enumeration in shod.h for more
	 * information.
	 */
}

static void
ewmhsetactivewindow(Window w)
{
	XChangeProperty(dpy, root, atoms[NetActiveWindow],
	                XA_WINDOW, 32, PropModeReplace,
	                (unsigned char *)&w, 1);
}

static void
ewmhsetnumberofdesktops(void)
{
    XChangeProperty(dpy, root, atoms[NetNumberOfDesktops],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&deskcount, 1);
}

static void
ewmhsetcurrentdesktop(unsigned long n)
{
	if (n >= deskcount)
		return;

	XChangeProperty(dpy, root, atoms[NetCurrentDesktop], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&n, 1);
}

static void
ewmhsetframeextents(struct Client *c)
{
	unsigned long data[4];

	data[0] = data[1] = data[2] = data[3] = c->b;

	XChangeProperty(dpy, c->win, atoms[NetFrameExtents], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&data, 4);
}

static void
ewmhsetshowingdesktop(int n)
{
	XChangeProperty(dpy, root, atoms[NetShowingDesktop],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&n, 1);
}

static void
ewmhsetstate(struct Client *c)
{
	Atom data[5];
	int n = 0;

	if (c == NULL)
		return;
	if (focused == c)
		data[n++] = atoms[NetWMStateFocused];
	if (c->isfullscreen)
		data[n++] = atoms[NetWMStateFullscreen];
	switch (c->state) {
	case Tiled:
		data[n++] = atoms[NetWMStateMaximizedVert];
		data[n++] = atoms[NetWMStateMaximizedHorz];
		break;
	case Sticky:
		data[n++] = atoms[NetWMStateSticky];
		break;
	case Minimized:
		data[n++] = atoms[NetWMStateHidden];
		break;
	default:
		break;
	}
	if (c->layer > 0)
		data[n++] = atoms[NetWMStateAbove];
	else if (c->layer < 0)
		data[n++] = atoms[NetWMStateBelow];
	XChangeProperty(dpy, c->win, atoms[NetWMState], XA_ATOM, 32,
	                PropModeReplace, (unsigned char *)data, n);
}

static void
ewmhsetdesktop(Window win, long d)
{
	XChangeProperty(dpy, win, atoms[NetWMDesktop],
	                XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&d, 1);
}

static void
ewmhsetwmdesktop(void)
{
	struct Client *c;
	long desk;
	desk = 0;

	for (c = clients; c; c = c->next) {
		if (c->state == Sticky || c->state == Minimized) {
			ewmhsetdesktop(c->win, 0xFFFFFFFF);
		} else {
			ewmhsetdesktop(c->win, getdesknum(c->desk));
		}
	}
}

static void
ewmhsetworkarea(int screenw, int screenh)
{
	unsigned long data[4];

	data[0] = 0;
	data[1] = 0;
	data[2] = screenw;
	data[3] = screenh;

	XChangeProperty(dpy, root, atoms[NetWorkarea],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&data, 4);
}

static void
ewmhsetclients(void)
{
	struct Client *c;
	Window *wins = NULL;
	size_t i = 0, nwins = 0;

	for (c = clients; c; c = c->next)
		nwins++;
	if (nwins)
		wins = ecalloc(nwins, sizeof *wins);
	for (c = clients; c; c = c->next)
		wins[i++] = c->win;
	XChangeProperty(dpy, root, atoms[NetClientList], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *)wins, nwins);
	free(wins);
}

static void
ewmhsetclientsstacking(void)
{
	struct Client *c, *last;
	Window *wins = NULL;
	size_t i = 0, nwins = 0;

	last = NULL;
	for (c = focused; c; c = c->fnext) {
		last = c;
		nwins++;
	}
	if (nwins)
		wins = ecalloc(nwins, sizeof *wins);
	for (c = last; c; c = c->fprev)
		if (c->state == Tiled && !c->isfullscreen)
			wins[i++] = c->win;
	for (c = last; c; c = c->fprev)
		if (c->state != Tiled && !c->isfullscreen && c->layer < 0)
			wins[i++] = c->win;
	for (c = last; c; c = c->fprev)
		if (c->state != Tiled && !c->isfullscreen && c->layer == 0)
			wins[i++] = c->win;
	for (c = last; c; c = c->fprev)
		if (c->state != Tiled && !c->isfullscreen && c->layer > 0)
			wins[i++] = c->win;
	for (c = last; c; c = c->fprev)
		if (c->isfullscreen)
			wins[i++] = c->win;
	XChangeProperty(dpy, root, atoms[NetClientListStacking], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *)wins, nwins);
	free(wins);
}

/* TODO set client size hints */
static void
updatesizehints(struct Client *c)
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
	c->isuserplaced = (size.flags & USPosition);
}

/* get client given a window */
static struct Client *
getclient(Window win)
{
	struct Client *c;

	for (c = clients; c; c = c->next)
		if (c->win == win || c->frame == win || c->curswin == win)
			return c;
	return NULL;
}

/* get monitor given coordinates */
static struct Monitor *
getmon(int x, int y)
{
	struct Monitor *mon;

	for (mon = mons; mon; mon = mon->next)
		if (x >= mon->mx && x <= mon->mx + mon->mw &&
		    y >= mon->my && y <= mon->my + mon->mh)
			return mon;
	return NULL;
}

/* get focused client */
static struct Client *
getfocused(struct Client *old)
{
	struct Client *c;

	if (old != NULL) {
		if (old->state == Tiled) {
			if (old->row->prev) {
				return old->row->prev->c;
			} else if (old->row->next) {
				return old->row->next->c;
			} else if (old->row->col->prev) {
				return old->row->col->prev->row->c;
			} else if (old->row->col->next) {
				return old->row->col->next->row->c;
			}
		} else if (old->state == Normal || old->state == Sticky) {
			for (c = focused; c; c = c->fnext) {
				if (c != old && ((c->state == Sticky && c->mon == selmon) ||
				    (c->state == Normal && c->desk == selmon->seldesk))) {
					return c;
				}
			}
		}
	}
	for (c = focused; c; c = c->fnext) {
		if (c != old && ((c->state == Sticky && c->mon == selmon) ||
		    ((c->state == Normal || c->state == Tiled) &&
		    c->desk == selmon->seldesk))) {
			return c;
		}
	}
	return NULL;
}

/* get focused fullscreen window in given monitor and desktop */
static struct Client *
getfullscreen(struct Monitor *mon, struct Desktop *desk)
{
	struct Client *c;

	for (c = focused; c; c = c->fnext)
		if (c->isfullscreen &&
		    (((c->state == Normal || c->state == Tiled) && c->desk == desk) ||
		     (c->state == Sticky && c->mon == mon)))
			return c;
	return NULL;
}

/* check if client is visible */
static int
clientisvisible(struct Client *c)
{
	if (c == NULL || c->state == Minimized)
		return 0;
	if (c->state == Sticky || (c->desk && c->desk == c->desk->mon->seldesk))
		return 1;
	return 0;
}

/* notify client of configuration changing */
static void
clientnotify(struct Client *c)
{
	XConfigureEvent ce;

	if (c == NULL)
		return;
	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

/* set client border color */
static void
clientbordercolor(struct Client *c, unsigned long color)
{
	if (c == NULL)
		return;
	XSetWindowBackground(dpy, c->frame, color);
	XSetForeground(dpy, gc, color);
	XFillRectangle(dpy, c->frame, gc, 0, 0, c->w + c->b * 2, c->h + c->b * 2);
}

/* set client border width */
static void
clientborderwidth(struct Client *c, int border)
{
	if (c == NULL)
		return;
	c->b = border;
}

/* commit floating client size and position */
static void
clientmoveresize(struct Client *c)
{
	if (c == NULL)
		return;
	XMoveResizeWindow(dpy, c->frame, c->x - c->b, c->y - c->b, c->w + c->b * 2, c->h + c->b * 2);
	XMoveResizeWindow(dpy, c->curswin, 0, 0, c->w + c->b * 2, c->h + c->b * 2);
	XMoveResizeWindow(dpy, c->win, c->b, c->b, c->w, c->h);
	clientnotify(c);
}

/* check if desktop is visible */
static int
deskisvisible(struct Desktop *desk)
{
	return desk->mon->seldesk == desk;
}

/* add desktop to monitor */
static void
deskadd(struct Monitor *mon)
{
	struct Desktop *desk, *lastdesk;

	desk = emalloc(sizeof *desk);
	desk->prev = NULL;
	desk->next = NULL;
	desk->nclients = 0;
	desk->col = NULL;
	desk->mon = mon;
	deskcount++;
	for (lastdesk = mon->desks; lastdesk && lastdesk->next; lastdesk = lastdesk->next)
		;
	if (lastdesk == NULL) {
		mon->desks = desk;
	} else {
		lastdesk->next = desk;
		desk->prev = lastdesk;
	}
}

/* delete desktop from monitor */
static void
deskdel(struct Desktop *desk)
{
	struct Column *col, *tcol;
	struct Row *row, *trow;

	if (desk == NULL)
		return;
	col = desk->col;
	while (col) {
		row = col->row;
		while (row) {
			trow = row;
			row = row->next;
			free(trow);
		}
		tcol = col;
		col = col->next;
		free(tcol);
	}
	if (desk->next)
		desk->next->prev = desk->prev;
	if (desk->prev)
		desk->prev->next = desk->next;
	else
		desk->mon->desks = desk->next;
	if (desk->mon->seldesk == desk)
		desk->mon->seldesk = desk->mon->desks;
	deskcount--;
	free(desk);
}

/* tile all clients in desktop */
static void
desktile(struct Desktop *desk)
{
	struct Monitor *mon;
	struct Column *col;
	struct Row *row;
	int recol = 0, rerow = 0;       /* whether to resize columns and rows */
	int sumw, sumh;
	int ncols, nrows;
	int x, y, w, h;

	mon = desk->mon;

	/* get number of columns and sum their widths with borders and gaps applied */
	sumw = 0;
	for (ncols = 0, col = desk->col; col; col = col->next, ncols++)
		sumw += col->w + config.border_width * 2;
	sumw += config.gapinner * (ncols - 1);
	if (ncols == 0)
		return;

	/* decide whether to recalculate columns widths */
	recol = (sumw != mon->gw);

	w = (mon->gw - config.gapinner * (ncols - 1) - config.border_width * 2 * ncols) / ncols;
	x = mon->gx;
	for (col = desk->col; col; col = col->next) {
		/* the last column gets the remaining width */
		if (!col->next)
			w = mon->gw + mon->gx - x - 2 * config.border_width;

		/* get number of clients in current column and sum their heights */
		sumh = 0;
		for (nrows = 0, row = col->row; row; row = row->next, nrows++)
			sumh += row->h + config.border_width * 2;
		sumh += config.gapinner * (nrows - 1);

		/* decide whether to recalculate client heights */
		rerow = (sumh != mon->gh);

		h = (mon->gh - config.gapinner * (nrows - 1) - config.border_width * 2 * nrows) / nrows;
		y = mon->gy;

		for (row = col->row; row; row = row->next) {
			if (row->c->isfullscreen)
				continue;

			/* the last client gets the remaining height */
			if (!row->next)
				h = mon->gh + mon->gy - y - 2 * config.border_width;

			/* set new width and heights */
			if (recol || !col->next)
				col->w = w;
			if (rerow || !row->next)
				row->h = h;

			/* if there is only one tiled window, borders or gaps can be ignored */
			if (nrows == 1 && ncols == 1) {
				if (config.ignoreborders) {
					col->w += 2 * config.border_width;
					row->h += 2 * config.border_width;
					clientborderwidth(row->c, 0);
				}
				if (config.ignoregaps) {
					x = mon->wx;
					y = mon->wy;
					col->w = mon->ww - ((!config.ignoreborders) ? 2 * config.border_width : 0);
					row->h = mon->wh - ((!config.ignoreborders) ? 2 * config.border_width : 0);
				}
			} else {
				clientborderwidth(row->c, config.border_width);
			}

			row->c->x = x + row->c->b;
			row->c->y = y + row->c->b;
			row->c->w = col->w;
			row->c->h = row->h;
			if (clientisvisible(row->c)) {
				clientmoveresize(row->c);
			}

			y += row->h + config.gapinner + config.border_width * 2;
		}

		x += col->w + config.gapinner + config.border_width * 2;
	}
}

/* check if monitor geometry is unique */
static int
monisuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}

/* add monitor */
static void
monadd(XineramaScreenInfo *info)
{
	struct Monitor *mon, *lastmon;

	mon = emalloc(sizeof *mon);
	mon->prev = NULL;
	mon->next = NULL;
	mon->desks = NULL;
	mon->seldesk = NULL;
	mon->mx = mon->wx = info->x_org;
	mon->my = mon->wy = info->y_org;
	mon->mw = mon->ww = info->width;
	mon->mh = mon->wh = info->height;
	mon->gx = mon->wx + config.gapouter;
	mon->gy = mon->wy + config.gapouter;
	mon->gw = mon->ww - config.gapouter * 2;
	mon->gh = mon->wh - config.gapouter * 2;
	deskadd(mon);
	mon->seldesk = mon->desks;
	for (lastmon = mons; lastmon && lastmon->next; lastmon = lastmon->next)
		;
	if (lastmon) {
		lastmon->next = mon;
		mon->prev = lastmon;
	} else {
		mons = mon;
	}
}

/* delete monitor and set monitor of clients on it to NULL */
static void
mondel(struct Monitor *mon)
{
	struct Client *c;

	while (mon->desks)
		deskdel(mon->desks);
	if (mon->next)
		mon->next->prev = mon->prev;
	if (mon->prev)
		mon->prev->next = mon->next;
	else
		mons = mon->next;
	for (c = clients; c; c = c->next) {
		if (c->mon == mon) {
			c->mon = NULL;
			c->desk = NULL;
		}
	}
	free(mon);
}

/* update the list of monitors */
static void
monupdate(void)
{
	void clientsendtodesk(struct Client *, struct Desktop *, int, int);
	XineramaScreenInfo *info = NULL;
	XineramaScreenInfo *unique = NULL;
	struct Monitor *mon;
	struct Monitor *tmp;
	struct Client *c;
	int delselmon = 0;
	int del, add;
	int i, j, n;
	int moncount;

	info = XineramaQueryScreens(dpy, &n);
	unique = ecalloc(n, sizeof *unique);
	
	/* only consider unique geometries as separate screens */
	for (i = 0, j = 0; i < n; i++)
		if (monisuniquegeom(unique, j, &info[i]))
			memcpy(&unique[j++], &info[i], sizeof *unique);
	XFree(info);
	moncount = j;

	/* look for monitors that do not exist anymore and delete them */
	mon = mons;
	while (mon) {
		del = 1;
		for (i = 0; i < moncount; i++) {
			if (unique[i].x_org == mon->mx && unique[i].y_org == mon->my &&
			    unique[i].width == mon->mw && unique[i].height == mon->mh) {
				del = 0;
				break;
			}
		}
		tmp = mon;
		mon = mon->next;
		if (del) {
			if (tmp == selmon)
				delselmon = 1;
			mondel(tmp);
		}
	}

	/* look for new monitors and add them */
	for (i = 0; i < moncount; i++) {
		add = 1;
		for (mon = mons; mon; mon = mon->next) {
			if (unique[i].x_org == mon->mx && unique[i].y_org == mon->my &&
			    unique[i].width == mon->mw && unique[i].height == mon->mh) {
				add = 0;
				break;
			}
		}
		if (add) {
			monadd(&unique[i]);
		}
	}
	if (delselmon)
		selmon = mons;

	/* send clients with do not belong to a window to selected desktop */
	for (c = clients; c; c = c->next) {
		if (c->state != Minimized && c->mon == NULL) {
			c->state = Normal;
			c->layer = 0;
			clientsendtodesk(c, selmon->seldesk, 1, 1);
		}
	}
	// monupdatearea();
	// panelupdate();
	free(unique);
}

/* allocate column in desktop */
static struct Column *
coladd(struct Desktop *desk, int end)
{
	struct Column *col, *lastcol;

	col = emalloc(sizeof *col);
	col->desk = desk;
	col->prev = NULL;
	col->next = NULL;
	col->row = NULL;
	col->w = 0;
	if (desk->col == NULL) {
		desk->col = col;
	} else if (end) {
		for (lastcol = desk->col; lastcol->next; lastcol = lastcol->next)
			;
		lastcol->next = col;
		col->prev = lastcol;
	} else {
		if (desk->col)
			desk->col->prev = col;
		col->next = desk->col;
		desk->col = col;
	}
	return col;
}

/* deallocate column */
static void
coldel(struct Column *col)
{
	if (col->next)
		col->next->prev = col->prev;
	if (col->prev)
		col->prev->next = col->next;
	else
		col->desk->col = col->next;
	free(col);
}

/* allocate row in column */
static struct Row *
rowadd(struct Column *col)
{
	struct Row *row, *lastrow;

	row = emalloc(sizeof *row);
	row->prev = NULL;
	row->next = NULL;
	row->col = col;
	row->c = None;
	row->h = 0;
	if (col->row == NULL) {
		col->row = row;
	} else {
		for (lastrow = col->row; lastrow->next; lastrow = lastrow->next)
			;
		lastrow->next = row;
		row->prev = lastrow;
	}
	return row;
}

/* deallocate row */
static void
rowdel(struct Row *row)
{
	if (row->next)
		row->next->prev = row->prev;
	if (row->prev)
		row->prev->next = row->next;
	else
		row->col->row = row->next;
	if (row->col->row == NULL)
		coldel(row->col);
	free(row);
}

#define DIV       15      /* number to divide the screen into grids */
#define WIDTH(x)  ((x)->fw + 2 * config.border_width)
#define HEIGHT(x) ((x)->fh + 2 * config.border_width)

/* find best position to place a client on screen */
static void
clientplace(struct Client *c, struct Desktop *desk)
{
	unsigned long grid[DIV][DIV] = {{0}, {0}};
	unsigned long lowest;
	struct Monitor *mon;
	struct Client *tmp;
	int center = 1;
	int i, j, k, w, h;
	int posi, posj;         /* position of the larger region */
	int lw, lh;             /* larger region width and height */

	if (desk == NULL || c == NULL || c->state == Tiled || c->isfullscreen || c->state == Minimized)
		return;

	mon = desk->mon;

	c->fw = min(c->fw, mon->gw - 2 * config.border_width);
	c->fh = min(c->fh, mon->gh - 2 * config.border_width);

	/* if the user placed the window, we should not re-place it */
	if (c->isuserplaced)
		return;

	if (c->fx < mon->gx || c->fx > mon->gx + mon->gw)
		c->fx = mon->gx;
	if (c->fy < mon->gy || c->fy > mon->gy + mon->gh)
		c->fy = mon->gy;

	/* if this is the first window in the desktop, just center it */
	for (tmp = clients; tmp; tmp = tmp->next) {
		if (((tmp->state == Sticky && tmp->mon == mon) || (tmp->state == Normal && tmp->desk == desk)) && tmp != c) {
			center = 0;
			break;
		}
	}
	if (center) {
		c->fx = mon->gx + mon->gw / 2 - WIDTH(c) / 2;
		c->fy = mon->gy + mon->gh / 2 - HEIGHT(c) / 2;
		return;
	}

	/* increment cells of grid a window is in */
	for (tmp = clients; tmp; tmp = tmp->next) {
		if (tmp != c && ((tmp->state == Sticky && tmp->mon == mon) || (tmp->state == Normal && tmp->desk == desk))) {
			for (i = 0; i < DIV; i++) {
				for (j = 0; j < DIV; j++) {
					int ha, hb, wa, wb;
					int ya, yb, xa, xb;
					ha = mon->gy + (mon->gh * i)/DIV;
					hb = mon->gy + (mon->gh * (i + 1))/DIV;
					wa = mon->gx + (mon->gw * j)/DIV;
					wb = mon->gx + (mon->gw * (j + 1))/DIV;
					ya = tmp->fy;
					yb = tmp->fy + HEIGHT(tmp);
					xa = tmp->fx;
					xb = tmp->fx + WIDTH(tmp);
					if (ya <= hb && ha <= yb && xa <= wb && wa <= xb) {
						grid[i][j]++;
						if (ya < ha && yb > hb)
							grid[i][j]++;
						if (xa < wa && xb > wb)
							grid[i][j]++;
					}
				}
			}
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
		c->fy = mon->gy;
	} else if (posi >= DIV - 1) {
		c->fy = mon->gy + mon->gh - HEIGHT(c);
	} else {
		int n, d, maxy;

		n = posi;
		d = DIV;
		maxy = mon->gy + mon->gh - HEIGHT(c);
		c->fy = (mon->gy + mon->gh * n)/d - HEIGHT(c);
		c->fy = max(mon->gy, c->fy);
		c->fy = min(c->fy, maxy);
	}

	/* calculate x */
	if (posj == 0) {
		c->fx = mon->gx;
	} else if (posj >= DIV - 1) {
		c->fx = mon->gx + mon->gw - WIDTH(c);
	} else {
		int n, d, maxx;

		n = posj;
		d = DIV;
		maxx = mon->gx + mon->gw - WIDTH(c);
		c->fx = (mon->gx + mon->gw * n)/d - WIDTH(c);
		c->fx = max(mon->gx, c->fx);
		c->fx = min(c->fx, maxx);
	}
}

/* get in which corner or side of window the cursor is in */
static enum Octant
clientoctant(struct Client *c, int x, int y)
{
	double tan;
	int wm, wa, wb;
	int hm, ha, hb;
	int w, h;

	if (c == NULL || c->state == Minimized)
		return SE;
	w = c->w;
	h = c->h;
	wm = w / 2;
	wa = min(wm, config.corner_width);
	wb = w - wa;
	hm = h / 2;
	ha = min(hm, config.corner_width);
	hb = h - ha;
	tan = (double)h/w;
	if (tan == 0.0)
		tan = 1.0;
	if (x >= wb && y >= hb)
		return SE;
	if (x <= wa && y >= hb)
		return SW;
	if (x >= wb && y <= ha)
		return NE;
	if (x <= wa && y <= ha)
		return NW;
	if (y >= hm) {
		h = y - hm;
		w = h / tan;
		if (x < wm - w) {
			return W;
		} if (x > wm + w) {
			return E;
		} else {
			return S;
		}
	} else {
		h = hm - y;
		w = h / tan;
		if (x < wm - w) {
			return W;
		} if (x > wm + w) {
			return E;
		} else {
			return N;
		}
	}
	return N;
}

/* check if new size is ok */
static int
clientchecksize(struct Client *c, int x, int y)
{
	int w, h;

	w = c->fw + x;
	h = c->fh + y;
	if (w < 1 || (c->minw && w < c->minw))
		return 0;
	if (c->maxw && w > c->maxw)
		return 0;
	if (h < 1 || (c->minh && h < c->minh))
		return 0;
	if (c->maxh && h > c->maxh)
		return 0;
	return 1;
}

/* apply size constraints */
static void
clientapplysize(struct Client *c)
{
	int w, h;

	if (c == NULL || c->isfixed)
		return;
	
	w = max(c->fw, 1);
	h = max(c->fh, 1);
	if (c->mina > 0 && c->maxa > 0) {
		if (c->maxa < (float)w/h) {
			if (c->w == max(w, h)) {
				h = w / c->maxa - 0.5;
			} else {
				w = h * c->maxa + 0.5;
			}
		} else if (c->mina < (float)h/w) {
			if (h == max(w, h)) {
				w = h / c->mina - 0.5;
			} else {
				h = w * c->mina + 0.5;
			}
		}
	}
	if (c->incw > 0 && c->basew < w) {
		w -= c->basew;
		w /= c->incw;
		w *= c->incw;
		w += c->basew;
	}
	if (c->inch > 0 && c->baseh < w) {
		h -= c->baseh;
		h /= c->inch;
		h *= c->inch;
		h += c->baseh;
	}
	w = max(c->minw, w);
	h = max(c->minh, h);
	if (c->maxw > 0)
		w = min(c->maxw, w);
	if (c->maxh > 0)
		h = min(c->maxh, h);
	c->w = w;
	c->h = h;
	c->x = c->fx;
	c->y = c->fy;
}

/* remove client from the focus list */
static void
clientdelfocus(struct Client *c)
{
	if (c->fnext) {
		c->fnext->fprev = c->fprev;
	}
	if (c->fprev) {
		c->fprev->fnext = c->fnext;
	} else if (focused == c) {
		focused = c->fnext;
	}
}

/* put client on beginning of focus list */
static void
clientaddfocus(struct Client *c)
{
	if (c == NULL || c->state == Minimized)
		return;
	clientdelfocus(c);
	c->fnext = focused;
	c->fprev = NULL;
	if (focused)
		focused->fprev = c;
	focused = c;
}

/* raise client */
static void
clientraise(struct Client *c)
{
	Window wins[2];

	if (c == NULL || c->state == Minimized)
		return;
	wins[1] = c->frame;
	if (c->trans != NULL)
		c = c->trans;
	if (c->isfullscreen)
		wins[0] = layerwin[LayerFullscreen];
	else if (c->state == Tiled)
		wins[0] = layerwin[LayerTiled];
	else if (c->layer < 0)
		wins[0] = layerwin[LayerBelow];
	else if (c->layer > 0)
		wins[0] = layerwin[LayerAbove];
	else
		wins[0] = layerwin[LayerTop];
	XRestackWindows(dpy, wins, sizeof wins);
	ewmhsetclientsstacking();
}

/* hide client */
static void
clienthide(struct Client *c, int hide)
{
	if (c == NULL)
		return;
	if (hide) {
		XUnmapWindow(dpy, c->frame);
		icccmwmstate(c->win, IconicState);
	} else {
		XMapWindow(dpy, c->frame);
		icccmwmstate(c->win, NormalState);
	}
}

/* hide all windows and show the desktop */
static void
clientshowdesk(int show)
{
	struct Client *c;

	showingdesk = show;
	for (c = clients; c; c = c->next) {
		if (c->state != Minimized &&
		   ((c->state == Sticky && c->mon == selmon) ||
		   (c->state != Sticky && c->desk == selmon->seldesk))) {
			clienthide(c, show);
		}
	}
	ewmhsetshowingdesktop(show);
}

/* stick a client to the monitor */
static void
clientstick(struct Client *c, int stick)
{
	void clientsendtodesk(struct Client *, struct Desktop *, int, int);

	if (c == NULL || c->isfullscreen || c->state == Minimized || c->state == Tiled)
		return;

	if (stick && c->state != Sticky) {
		c->state = Sticky;
		c->desk->nclients--;
		c->desk = NULL;
	} else if (!stick && c->state == Sticky) {
		c->state = Normal;
		clientsendtodesk(c, c->mon->seldesk, 0, 0);
	}
	ewmhsetwmdesktop();
}

/* tile/untile client */
static void
clienttile(struct Client *c, int tile)
{
	struct Monitor *mon;
	struct Desktop *desk;
	struct Column *col;
	struct Row *row;

	if (c == NULL || c->isfixed || c->state == Minimized || c->isfullscreen)
		return;

	desk = c->desk;

	if (tile && c->state != Tiled) {
		if (desk->col == NULL || desk->col->next == NULL) {
			col = coladd(desk, 1);
		} else {
			for (col = desk->col; col->next; col = col->next)
				;
		}
		row = rowadd(col);
		row->c = c;
		c->row = row;
		c->layer = 0;
		clientstick(c, 0);
		c->state = Tiled;
	} else if (!tile && c->state == Tiled) {
		clientstick(c, 0);
		c->state = Normal;
		rowdel(c->row);
		clientplace(c, c->desk);
		clientborderwidth(c, config.border_width);
		clientapplysize(c);
		if (clientisvisible(c)) {
			clientmoveresize(c);
		}
	}
	clientraise(c);
	for (mon = mons; mon; mon = mon->next) {
		if (mon->seldesk == desk) {
			desktile(desk);
		}
	}
}

/* minimize client */
static void
clientminimize(struct Client *c, int minimize)
{
	void clientfocus(struct Client *c);
	void clientsendtodesk(struct Client *, struct Desktop *, int, int);

	if (c == NULL)
		return;
	if (c->state == Tiled)
		clienttile(c, 0);
	if (minimize && c->state != Minimized) {
		c->desk->nclients--;
		c->desk = NULL;
		c->mon = NULL;
		c->state = Minimized;
		clientfocus(getfocused(c));
		ewmhsetwmdesktop();
	} else if (!minimize && c->state == Minimized) {
		c->state = Normal;
		clientsendtodesk(c, selmon->seldesk, 1, 1);
	}
	clienthide(c, minimize);
}

/* make client fullscreen */
static void
clientfullscreen(struct Client *c, int fullscreen)
{
	if (c == NULL || c->state == Minimized)
		return;
	if (c->state == Sticky)
		clientstick(c, 0);
	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		clientborderwidth(c, 0);
		c->x = c->mon->mx;
		c->y = c->mon->my;
		c->w = c->mon->mw;
		c->h = c->mon->mh;
		if (clientisvisible(c))
			clientmoveresize(c);
	} else if (!fullscreen && c->isfullscreen) {
		c->isfullscreen = 0;
		clientborderwidth(c, config.border_width);
		if (c->state == Tiled) {
			desktile(c->desk);
		} else if (clientisvisible(c)) {
			clientapplysize(c);
			clientmoveresize(c);
		}
	}
	clientraise(c);
}

/* raise client above others */
static void
clientabove(struct Client *c, int above)
{
	if (c == NULL || c->state == Tiled || c->isfullscreen || c->state == Minimized)
		return;
	c->layer = (above) ? 1 : 0;
	clientraise(c);
}

/* lower client below others */
static void
clientbelow(struct Client *c, int below)
{
	if (c == NULL || c->state == Tiled || c->isfullscreen || c->state == Minimized)
		return;
	c->layer = (below) ? -1 : 0;
	clientraise(c);
}

/* focus client */
void
clientfocus(struct Client *c)
{
	struct Client *prevfocused, *fullscreen;

	clientbordercolor(focused, colors.unfocused);
	if (c == NULL || c->state == Minimized) {
		XSetInputFocus(dpy, focuswin, RevertToParent, CurrentTime);
		ewmhsetactivewindow(None);
		return;
	}
	fullscreen = getfullscreen(c->mon, c->desk);
	if (fullscreen != NULL && fullscreen != c && fullscreen != c->trans)
		return;         /* we should not focus a client below a fullscreen client */
	prevfocused = focused;
	if (c->mon)
		selmon = c->mon;
	if (c->state != Sticky && c->state != Minimized)
		selmon->seldesk = c->desk;
	if (showingdesk)
		clientshowdesk(0);
	clientaddfocus(c);
	clientbordercolor(c, colors.focused);
	if (c->state == Minimized)
		clientminimize(c, 0);
	XSetInputFocus(dpy, c->win, RevertToParent, CurrentTime);
	ewmhsetstate(prevfocused);
	ewmhsetstate(c);
	ewmhsetactivewindow(c->win);
	ewmhsetclientsstacking();
	ewmhsetcurrentdesktop(getdesknum(selmon->seldesk));
}

/* send client to center of the window it is transient for */
static void
clientsendtransient(struct Client *c, struct Client *t)
{
	if (c == NULL || t == NULL)
		return;

	c->trans = t;
	c->fx = c->x = t->x + t->w / 2 - c->w / 2;
	c->fy = c->y = t->y + t->h / 2 - c->h / 2;
	c->mon = t->mon;
	c->desk = t->desk;
	c->desk->nclients++;
	if (t->state == Sticky)
		c->state = Sticky;
	clientmoveresize(c);
	clientraise(c);
	if (clientisvisible(t)) {
		clienthide(c, 0);
		clientfocus(c);
	}
	ewmhsetwmdesktop();
	ewmhsetnumberofdesktops();
}

/* send client to desk and place it */
void
clientsendtodesk(struct Client *c, struct Desktop *desk, int place, int focus)
{
	struct Monitor *mon;
	int focusother = 0;
	int hide = 0;

	if (c == NULL || desk == NULL || c->desk == desk || c->state == Minimized)
		return;

	if (c->state == Sticky)
		clientstick(c, 0);
	else if (c->state == Tiled)
		clienttile(c, 0);
	if (clientisvisible(c)) {
		hide = 1;
		for (mon = mons; mon; mon = mon->next) {
			if (mon->seldesk == desk) {
				hide = 0;
				break;
			}
		}
	}
	if (hide) {
		if (c->desk == selmon->seldesk)
			focusother = 1;
		clienthide(c, 1);
	}
	if (desk->nclients++ == 0 && desk->next == NULL)
		deskadd(desk->mon);
	if (c->desk && c->desk != desk)
		c->desk->nclients--;
	c->desk = desk;
	c->mon = desk->mon;
	clientraise(c);
	if (desk->next == NULL)
		deskadd(desk->mon);
	if (place) {
		clientplace(c, c->desk);
		clientapplysize(c);
		if (clientisvisible(c)) {
			clientmoveresize(c);
		}
	}
	if (!hide)
		clienthide(c, 0);
	if (focus)
		clientfocus(c);
	else if (focusother)
		clientfocus(getfocused(NULL));
	ewmhsetwmdesktop();
	ewmhsetnumberofdesktops();
	XSync(dpy, False);
}

#define MINSIZE 10              /* minimum size for tiled windows */

/* resize client x and y pixels out of octant o */
static void
clientincrresize(struct Client *c, enum Octant o, int x, int y)
{
	int origw, origh;

	if (c == NULL || c->isfixed || c->state == Minimized || c->isfullscreen)
		return;
	if (c->state == Tiled) {
		if (c->row->col->w + x < MINSIZE || c->row->h + y < MINSIZE)
			return;
		switch (o) {
		case NW:
			if (c->row->col->prev) {
				c->row->col->w += x;
				c->row->col->prev->w -= x;
			}
			if (c->row->prev) {
				c->row->h += y;
				c->row->prev->h -= y;
			}
			break;
		case NE:
			if (c->row->col->next) {
				c->row->col->w += x;
				c->row->col->next->w -= x;
			}
			if (c->row->prev) {
				c->row->h += y;
				c->row->prev->h -= y;
			}
			break;
		case SW:
			if (c->row->col->prev) {
				c->row->col->w += x;
				c->row->col->prev->w -= x;
			}
			if (c->row->next) {
				c->row->h += y;
				c->row->next->h -= y;
			}
			break;
		case SE:
			if (c->row->col->next) {
				c->row->col->w += x;
				c->row->col->next->w -= x;
			}
			if (c->row->next) {
				c->row->h += y;
				c->row->next->h -= y;
			}
			break;
		case N:
			if (c->row->prev) {
				c->row->h += y;
				c->row->prev->h -= y;
			}
			break;
		case S:
			if (c->row->next) {
				c->row->h += y;
				c->row->next->h -= y;
			}
			break;
		case W:
			if (c->row->col->prev) {
				c->row->col->w += x;
				c->row->col->prev->w -= x;
			}
			break;
		case E:
			if (c->row->col->next) {
				c->row->col->w += x;
				c->row->col->next->w -= x;
			}
			break;
		}
		desktile(c->desk);
	} else {
		if (!clientchecksize(c, x, y))
			return;
		origw = c->w;
		origh = c->h;
		c->fw += x;
		c->fh += y;
		clientapplysize(c);
		switch (o) {
		case NW:
			c->fx -= c->w - origw;
			c->fy -= c->h - origh;
			break;
		case NE:
			c->fy -= c->h - origh;
			break;
		case SW:
			c->fx -= c->w - origw;
			break;
		case SE:
			/* nothing */
			break;
		case N:
			c->fy -= c->h - origh;
			break;
		case S:
			/* nothing */
			break;
		case W:
			c->fx -= c->w - origw;
			break;
		case E:
			/* nothing */
			break;
		}
		c->x = c->fx;
		c->y = c->fy;
		clientmoveresize(c);
	}
}

/* move client x pixels to the right and y pixels down */
static void
clientincrmove(struct Client *c, int x, int y)
{
	struct Monitor *monto;
	struct Column *col = NULL;
	struct Row *row = NULL;

	if (c == NULL || c->state == Minimized || c->isfullscreen)
		return;
	if (c->state == Tiled) {
		row = c->row;
		if (x > 0) {
			if (row->col->next)
				col = row->col->next;
			else if (!(row->col->row == row && row->next == NULL))
				col = coladd(row->col->desk, 1);
			if (col) {
				rowdel(row);
				row = rowadd(col);
				row->c = c;
				c->row = row;
			}
		}
		if (x < 0) {
			if (row->col->prev)
				col = row->col->prev;
			else if (!(row->col->row == row && row->next == NULL))
				col = coladd(row->col->desk, 0);
			if (col) {
				rowdel(row);
				row = rowadd(col);
				row->c = c;
				c->row = row;
			}
		}
		if (y < 0) {
			if (row->prev) {
				row->c = row->prev->c;
				row->c->row = row;
				row->prev->c = c;
				row->prev->c->row = row->prev;
			}
		}
		if (y > 0) {
			if (row->next) {
				row->c = row->next->c;
				row->c->row = row;
				row->next->c = c;
				row->next->c->row = row->next;
			}
		}
		desktile(c->desk);
	} else {
		c->fx += x;
		c->fy += y;
		c->x = c->fx;
		c->y = c->fy;
		clientmoveresize(c);
		if (c->state != Sticky) {
			monto = getmon(c->fx + c->fw / 2, c->fy + c->fh / 2);
			if (monto && monto != c->mon) {
				clientsendtodesk(c, monto->seldesk, 0, 1);
			}
		}
	}
}

/* add client */
static void
clientadd(Window win, XWindowAttributes *wa)
{
	static XSetWindowAttributes swa = {
		.event_mask = EnterWindowMask | SubstructureNotifyMask | ExposureMask
		            | SubstructureRedirectMask | ButtonPressMask | FocusChangeMask
		            | PointerMotionMask,
	};
	struct Client *c, *f, *t;
	Window trans;
	unsigned long *values;
	int focus = 1;          /* whether to focus window */

	if ((values = getcardinalprop(win, atoms[NetWMUserTime], 1)) != NULL) {
		if (values[0] == 0)
			focus = 0;
		XFree(values);
	}
	c = emalloc(sizeof *c);
	c->fprev = c->fnext = NULL;
	c->mon = NULL;
	c->desk = NULL;
	c->row = NULL;
	c->isfullscreen = 0;
	c->isuserplaced = 0;
	c->trans = NULL;
	c->isfixed = 0;
	c->state = Normal;
	c->layer = 0;
	c->x = c->fx = wa->x;
	c->y = c->fy = wa->y;
	c->w = c->fw = wa->width;
	c->h = c->fh = wa->height;
	c->b = config.border_width;
	c->win = win;
	c->prev = NULL;
	if (clients)
		clients->prev = c;
	c->next = clients;
	clients = c;
	updatesizehints(c);
	swa.background_pixel = colors.focused;
	swa.cursor = cursor[CursNormal];
	c->frame = XCreateWindow(dpy, root, c->x - c->b, c->y - c->b,
	                         c->w + c->b * 2, c->h + c->b * 2, 0,
	                         CopyFromParent, CopyFromParent, CopyFromParent,
	                         CWEventMask | CWBackPixel | CWCursor, &swa);
	c->curswin = XCreateWindow(dpy, c->frame, 0, 0, c->w + c->b * 2, c->h + c->b * 2, 0,
	                           CopyFromParent, InputOnly, CopyFromParent, 0, NULL);
	XReparentWindow(dpy, c->win, c->frame, config.border_width, config.border_width);
	XSelectInput(dpy, win, EnterWindowMask | StructureNotifyMask
	                     | PropertyChangeMask | FocusChangeMask);
	XGrabButton(dpy, AnyButton, AnyModifier, win, False,
	            ButtonPressMask | ButtonReleaseMask,
	            GrabModeSync, GrabModeSync, None, None);
	XSetWindowBorderWidth(dpy, win, 0);
	XMapWindow(dpy, c->curswin);
	XMapWindow(dpy, c->win);
	icccmwmstate(c->win, NormalState);
	ewmhsetframeextents(c);
	ewmhsetallowedactions(win);
	ewmhsetclients();
	ewmhsetclientsstacking();
	clientnotify(c);
	clientraise(c);
	f = getfocused(NULL);
	if (focus && f != NULL && f->isfullscreen)
		focus = 0;
	if (!focus)
		clientbordercolor(c, colors.unfocused);
	if (XGetTransientForHint(dpy, win, &trans) && (t = getclient(trans))) {
		clientsendtransient(c, t);
	} else {
		clientsendtodesk(c, selmon->seldesk, 1, focus);
	}
}

/* delete client */
static void
clientdel(struct Client *c)
{
	struct Client *focus;

	focus = getfocused(c);
	clientdelfocus(c);
	if (c->next)
		c->next->prev = c->prev;
	if (c->prev)
		c->prev->next = c->next;
	else
		clients = c->next;
	if (c->state == Tiled) {
		rowdel(c->row);
	}
	if (c->state != Minimized && c->mon == selmon)
		clientfocus(focus);
	if (c->state != Minimized && c->state != Sticky)
		c->desk->nclients--;
	if (c->state == Tiled)
		desktile(c->desk);
	icccmwmstate(c->win, WithdrawnState);
	XReparentWindow(dpy, c->win, root, c->x, c->y);
	XDestroyWindow(dpy, c->frame);
	ewmhsetclients();
	ewmhsetclientsstacking();
	ewmhsetwmdesktop();
	free(c);
}

/* change desktop */
static void
deskchange(struct Desktop *desk)
{
	struct Monitor *mon;
	struct Desktop *d, *tmp;
	struct Client *c;
	int cursorx, cursory;
	int deleted = 0;
	Window da, db;          /* dummy variables */
	int dx, dy;             /* dummy variables */
	unsigned int du;        /* dummy variable */

	if (desk == NULL || desk == selmon->seldesk)
		return;
	desktile(desk);
	if (!deskisvisible(desk)) {
		/* hide clients of previous current desktop */
		for (c = clients; c; c = c->next) {
			if (c->desk == desk->mon->seldesk) {
				clienthide(c, 1);
			}
		}

		/* unhide clientsof new current desktop */
		for (c = clients; c; c = c->next) {
			if (c->desk == desk) {
				if (!c->isfullscreen && c->state == Normal) {
					clientapplysize(c);
					clientmoveresize(c);
				}
				clienthide(c, 0);
			}
		}
	}

	/* if moving between desks in the same monitor, we can delete a desk */
	for (mon = mons; mon; mon = mon->next) {
		d = mon->desks;
		while (d && d->next) {
			tmp = d;
			d = d->next;
			if (tmp->nclients == 0 && tmp != desk) {
				deskdel(tmp);
				deleted = 1;
			}
		}
	}

	/* if changing focus to a new monitor and the cursor isn't there, warp it */
	XQueryPointer(dpy, root, &da, &db, &cursorx, &cursory, &dx, &dy, &du);
	if (desk->mon != selmon && desk->mon != getmon(cursorx, cursory)) {
		XWarpPointer(dpy, None, root, 0, 0, 0, 0, desk->mon->mx + desk->mon->mw / 2,
		                                         desk->mon->my + desk->mon->mh / 2);
	}

	/* update current desktop */
	selmon = desk->mon;
	selmon->seldesk = desk;
	if (showingdesk)
		clientshowdesk(0);
	ewmhsetcurrentdesktop(getdesknum(desk));
	if (deleted) {
		ewmhsetnumberofdesktops();
		ewmhsetwmdesktop();
	}

	/* focus client on the new current desktop */
	clientfocus(getfocused(NULL));
}

/* configure client size and position */
static void
clientconfigure(struct Client *c, unsigned int valuemask, XWindowChanges *wc)
{
	int x, y, w, h;

	if (c == NULL || c->isfixed || c->state == Minimized || c->isfullscreen)
		return;
	if (valuemask & CWX)
		wc->x -= c->b;
	if (valuemask & CWY)
		wc->y -= c->b;
	if (c->state == Tiled) {
		x = y = w = h = 0;
		if (valuemask & CWX)
			x = wc->x - c->x;
		if (valuemask & CWY)
			y = wc->y - c->y;
		if (valuemask & CWWidth)
			w = wc->width - c->w;
		if (valuemask & CWHeight)
			h = wc->height - c->h;
		if (x || y)
			clientincrmove(c, x, y);
		if (w || h)
			clientincrresize(c, SW, w, h);
	} else {
		if (valuemask & CWX)
			c->fx = wc->x;
		if (valuemask & CWY)
			c->fy = wc->y;
		if (valuemask & CWWidth)
			c->fw = wc->width;
		if (valuemask & CWHeight)
			c->fh = wc->height;
		clientapplysize(c);
		if (clientisvisible(c)) {
			clientmoveresize(c);
		}
	}
}

/* send a WM_DELETE message to client */
static void
clientclose(struct Client *c)
{
	XEvent ev;

	ev.type = ClientMessage;
	ev.xclient.window = c->win;
	ev.xclient.message_type = atoms[WMProtocols];
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = atoms[WMDeleteWindow];
	ev.xclient.data.l[1] = CurrentTime;

	/*
	 * communicate with the given Client, kindly telling it to
	 * close itself and terminate any associated processes using
	 * the WM_DELETE_WINDOW protocol
	 */
	XSendEvent(dpy, c->win, False, NoEventMask, &ev);
}

/* scan for already existing windows and adopt them */
static void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				clientadd(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				clientadd(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

/* map and hide focus window */
static void
mapfocuswin(void)
{
	XMoveWindow(dpy, focuswin, -1, 0);
	XMapWindow(dpy, focuswin);
}

/* focus window when clicking on it, and activate moving/resizing */
static void
xeventbuttonpress(XEvent *e)
{
	XButtonPressedEvent *ev = &e->xbutton;
	struct Monitor *mon;
	struct Client *c;
	Cursor curs = None;
	int isborder;
	int focus = 0;
	int raise = 0;

	/* if user clicked in no window, focus the monitor below cursor */
	if ((c = getclient(ev->window)) == NULL) {
		mon = getmon(ev->x_root, ev->y_root);
		if (mon)
			deskchange(mon->seldesk);
		goto done;
	}

	/* check action performed by mouse */
	isborder = ev->window == c->frame && ev->subwindow != c->win;
	if (ev->state == config.modifier && ev->button == Button1)
		motionaction = Moving;
	else if (ev->state == config.modifier && ev->button == Button3)
		motionaction = Resizing;
	else if (isborder && ev->button == Button3)
		motionaction = Moving;
	else if (isborder && ev->button == Button1)
		motionaction = Resizing;
	else
		motionaction = NoAction;

	/* user is dragging window while clicking modifier or dragging window's border */
	if (motionaction != NoAction) {
		octant = clientoctant(c, ev->x_root - c->x, ev->y_root - c->y);
		if (motionaction == Moving) {
			curs = cursor[CursMove];
		} else {
			switch (octant) {
			case NW:
				curs = cursor[CursNW];
				break;
			case NE:
				curs = cursor[CursNE];
				break;
			case SW:
				curs = cursor[CursSW];
				break;
			case SE:
				curs = cursor[CursSE];
				break;
			case N:
				curs = cursor[CursN];
				break;
			case S:
				curs = cursor[CursS];
				break;
			case W:
				curs = cursor[CursW];
				break;
			case E:
				curs = cursor[CursE];
				break;
			}
		}

		XGrabPointer(dpy, c->win, False,
		             ButtonReleaseMask | Button1MotionMask | Button3MotionMask,
		             GrabModeAsync, GrabModeAsync, None, curs, CurrentTime);
		motionx = ev->x_root;
		motiony = ev->y_root;
	}

	/* focus client */
	if (ev->button == Button1 && config.focusbuttons & 1 << 0)
		focus = 1;
	else if (ev->button == Button2 && config.focusbuttons & 1 << 1)
		focus = 1;
	else if (ev->button == Button3 && config.focusbuttons & 1 << 2)
		focus = 1;
	else if (ev->button == Button4 && config.focusbuttons & 1 << 3)
		focus = 1;
	else if (ev->button == Button5 && config.focusbuttons & 1 << 4)
		focus = 1;
	if (focus)
		clientfocus(c);

	/* raise client */
	if (ev->button == Button1 && config.raisebuttons & 1 << 0)
		raise = 1;
	else if (ev->button == Button2 && config.raisebuttons & 1 << 1)
		raise = 1;
	else if (ev->button == Button3 && config.raisebuttons & 1 << 2)
		raise = 1;
	else if (ev->button == Button4 && config.raisebuttons & 1 << 3)
		raise = 1;
	else if (ev->button == Button5 && config.raisebuttons & 1 << 4)
		raise = 1;
	if (raise)
		clientraise(c);

done:
	XAllowEvents(dpy, ReplayPointer, CurrentTime);
}

/* interrupt moving/resizing action */
static void
xeventbuttonrelease(XEvent *e)
{
	(void)e;
	XUngrabPointer(dpy, CurrentTime);
	motionaction = NoAction;
	motionx = -1;
	motiony = -1;
}

/* handle client message event */
static void
xeventclientmessage(XEvent *e)
{
	XClientMessageEvent *ev = &e->xclient;
	XWindowChanges wc;
	unsigned value_mask = 0;
	struct Client *c;
	Cursor curs = None;
	int i;

	c = getclient(ev->window);
	if (ev->message_type == atoms[NetCurrentDesktop]) {
		deskchange(getdesk(ev->data.l[0]));
	} else if (ev->message_type == atoms[NetShowingDesktop]) {
		if (ev->data.l[0]) {
			clientshowdesk(1);
		} else {
			clientfocus(getfocused(NULL));
		}
	} else if (ev->message_type == atoms[NetRequestFrameExtents]) {
		if (c == NULL)
			return;
		ewmhsetframeextents(c);
	} else if (ev->message_type == atoms[NetWMState]) {
		if (c == NULL)
			return;
		/*
		 * ev->data.l[0] == 0: _NET_WM_STATE_REMOVE
		 * ev->data.l[0] == 1: _NET_WM_STATE_ADD
		 * ev->data.l[0] == 2: _NET_WM_STATE_TOGGLE
		 */
		if (((Atom)ev->data.l[1] == atoms[NetWMStateMaximizedVert] ||
		     (Atom)ev->data.l[1] == atoms[NetWMStateMaximizedHorz]) &&
		    ((Atom)ev->data.l[2] == atoms[NetWMStateMaximizedVert]  ||
		     (Atom)ev->data.l[2] == atoms[NetWMStateMaximizedHorz])) {
			clienttile(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && c->state != Tiled)));
		}
		for (i = 0; i < 2; i++) {
			if ((Atom)ev->data.l[i] == atoms[NetWMStateFullscreen])
				clientfullscreen(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !c->isfullscreen)));
			else if ((Atom)ev->data.l[i] == atoms[NetWMStateSticky])
				clientstick(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && c->state != Sticky)));
			else if ((Atom)ev->data.l[i] == atoms[NetWMStateHidden])
				clientminimize(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && c->state != Minimized)));
			else if ((Atom)ev->data.l[i] == atoms[NetWMStateAbove])
				clientabove(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && (c->layer <= 0))));
			else if ((Atom)ev->data.l[i] == atoms[NetWMStateBelow])
				clientbelow(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && (c->layer >= 0))));
			ewmhsetstate(c);
		}
	} else if (ev->message_type == atoms[NetActiveWindow]) {
		if (c == NULL)
			return;
		if (c->state == Minimized) {
			clientminimize(c, 0);
		} else {
			deskchange(c->desk);
			clientfocus(c);
			clientraise(c);
		}
	} else if (ev->message_type == atoms[NetCloseWindow]) {
		if (c == NULL)
			return;
		clientclose(c);
	} else if (ev->message_type == atoms[NetMoveresizeWindow]) {
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
		clientconfigure(c, value_mask, &wc);
	} else if (ev->message_type == atoms[NetWMDesktop]) {
		if (c == NULL)
			return;
		if (ev->data.l[0] == 0xFFFFFFFF)
			clientstick(c, 1);
		else if (c->state != Sticky && c->state != Minimized) {
			clientsendtodesk(c, getdesk(ev->data.l[0]), 1, 0);
		}
	} else if (ev->message_type == atoms[NetWMMoveresize]) {
		/*
		 * Client-side decorated Gtk3 windows emit this signal when being
		 * dragged by their GtkHeaderBar
		 */
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
		if (c == NULL)
			return;
		if (ev->data.l[2] == _NET_WM_MOVERESIZE_CANCEL) {
			XUngrabPointer(dpy, CurrentTime);
			motionaction = NoAction;
			motionx = -1;
			motiony = -1;
		} else {
			switch (ev->data.l[2]) {
			case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
				octant = NW;
				curs = cursor[CursNW];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_TOP:
				octant = N;
				curs = cursor[CursN];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
				octant = NE;
				curs = cursor[CursNE];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_RIGHT:
				octant = E;
				curs = cursor[CursE];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
				octant = SE;
				curs = cursor[CursSE];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
				octant = S;
				curs = cursor[CursS];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
				octant = SW;
				curs = cursor[CursSW];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_LEFT:
				octant = W;
				curs = cursor[CursW];
				motionaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_MOVE:
				curs = cursor[CursMove];
				motionaction = Moving;
				break;
			default:
				return;
			}
			motionx = ev->data.l[0];
			motiony = ev->data.l[1];
			XGrabPointer(dpy, c->win, False,
			             ButtonReleaseMask | Button1MotionMask | Button3MotionMask,
			             GrabModeAsync, GrabModeAsync, None, curs, CurrentTime);
		}
	}
}

/* handle configure notify event */
static void
xeventconfigurenotify(XEvent *e)
{
	XConfigureEvent *ev = &e->xconfigure;

	if (ev->window == root) {
		screenw = ev->width;
		screenh = ev->height;
		monupdate();
		ewmhsetnumberofdesktops();
		ewmhsetworkarea(screenw, screenh);
	}
}

/* handle configure request event */
static void
xeventconfigurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	struct Client *c;

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = ev->border_width;
	wc.sibling = ev->above;
	wc.stack_mode = ev->detail;
	if ((c = getclient(ev->window)) != NULL) {
		clientconfigure(c, ev->value_mask, &wc);
	} else {
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
}

/* forget about client */
static void
xeventdestroynotify(XEvent *e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	struct Client *c;

	if ((c = getclient(ev->window)) != NULL)
		clientdel(c);
}

/* focus window when cursor enter it (only if there is no focus button) */
static void
xevententernotify(XEvent *e)
{
	struct Client *c;

	if (config.focusbuttons)
		return;
	while (XCheckTypedEvent(dpy, EnterNotify, e))
		;
	if ((c = getclient(e->xcrossing.window)) != NULL)
		clientfocus(c);
}

/* handle focusin event */
static void
xeventfocusin(XEvent *e)
{
	(void)e;

	clientfocus(focused);
}

/* key press event on focuswin */
static void
xeventkeypress(XEvent *e)
{
	XKeyPressedEvent *ev = &e->xkey;

	if (ev->window == focuswin) {
		XSendEvent(dpy, root, False, KeyPressMask, e);
	}
}

/* manage window */
static void
xeventmaprequest(XEvent *e)
{
	XMapRequestEvent *ev = &e->xmaprequest;
	XWindowAttributes wa;
	Atom prop = None;
	Window wins[2];

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;

	wins[1] = ev->window;
	prop = getatomprop(ev->window, atoms[NetWMWindowType]);
	if (prop == atoms[NetWMWindowTypeToolbar] ||
	    prop == atoms[NetWMWindowTypeUtility] ||
	    prop == atoms[NetWMWindowTypeMenu]) {
		XMapWindow(dpy, ev->window);
	} else if (prop == atoms[NetWMWindowTypeDesktop]) {
		XMapWindow(dpy, ev->window);
		wins[0] = layerwin[LayerDesktop];
		XRestackWindows(dpy, wins, sizeof wins);
	} else if (prop == atoms[NetWMWindowTypeDock]) {
		XMapWindow(dpy, ev->window);
		wins[0] = layerwin[LayerBars];
		XRestackWindows(dpy, wins, sizeof wins);
	} else if (getclient(ev->window) == NULL) {
		clientadd(ev->window, &wa);
	}
}

/* run moving/resizing action */
static void
xeventmotionnotify(XEvent *e)
{
	XMotionEvent *ev = &e->xmotion;
	struct Client *c;
	int x, y;

	if ((c = getclient(ev->window)) == NULL)
		return;
	if (motionaction == NoAction && ev->subwindow == c->curswin) {
		switch (clientoctant(c, ev->x - c->b, ev->y - c->b)) {
		case NW:
			XDefineCursor(dpy, c->curswin, cursor[CursNW]);
			break;
		case NE:
			XDefineCursor(dpy, c->curswin, cursor[CursNE]);
			break;
		case SW:
			XDefineCursor(dpy, c->curswin, cursor[CursSW]);
			break;
		case SE:
			XDefineCursor(dpy, c->curswin, cursor[CursSE]);
			break;
		case N:
			XDefineCursor(dpy, c->curswin, cursor[CursN]);
			break;
		case S:
			XDefineCursor(dpy, c->curswin, cursor[CursS]);
			break;
		case W:
			XDefineCursor(dpy, c->curswin, cursor[CursW]);
			break;
		case E:
			XDefineCursor(dpy, c->curswin, cursor[CursE]);
			break;
		}
		return;
	} else if (motionaction == Resizing) {
		switch(octant) {
		case NW:
			x = motionx - ev->x_root;
			y = motiony - ev->y_root;
			if ((y > 0 && ev->y_root > c->y) || (y < 0 && ev->y_root < c->y) ||
			    (x > 0 && ev->x_root > c->x) || (x < 0 && ev->x_root < c->x))
				goto done;
			break;
		case NE:
			x = ev->x_root - motionx;
			y = motiony - ev->y_root;
			if ((y > 0 && ev->y_root > c->y) || (y < 0 && ev->y_root < c->y) ||
			    (x > 0 && ev->x_root < c->x + c->w) || (x < 0 && ev->x_root > c->x + c->w))
				goto done;
			break;
		case SW:
			x = motionx - ev->x_root;
			y = ev->y_root - motiony;
			if ((y > 0 && ev->y_root < c->y + c->h) || (y < 0 && ev->y_root > c->y + c->h) ||
			    (x > 0 && ev->x_root > c->x) || (x < 0 && ev->x_root < c->x))
				goto done;
			break;
		case SE:
			x = ev->x_root - motionx;
			y = ev->y_root - motiony;
			if ((y > 0 && ev->y_root < c->y + c->h) || (y < 0 && ev->y_root > c->y + c->h) ||
			    (x > 0 && ev->x_root < c->x + c->w) || (x < 0 && ev->x_root > c->x + c->w))
				goto done;
			break;
		case N:
			x = 0;
			y = motiony - ev->y_root;
			if (y == 0 || (y > 0 && ev->y_root > c->y) || (y < 0 && ev->y_root < c->y))
				goto done;
			break;
		case S:
			x = 0;
			y = ev->y_root - motiony;
			if (y == 0 || (y > 0 && ev->y_root < c->y + c->h) || (y < 0 && ev->y_root > c->y + c->h))
				goto done;
			break;
		case W:
			x = motionx - ev->x_root;
			y = 0;
			if (x == 0 || (x > 0 && ev->x_root > c->x) || (x < 0 && ev->x_root < c->x))
				goto done;
			break;
		case E:
			x = ev->x_root - motionx;
			y = 0;
			if (x == 0 || (x > 0 && ev->x_root < c->x + c->w) || (x < 0 && ev->x_root > c->x + c->w))
				goto done;
			break;
		}
		clientincrresize(c, octant, x, y);
	} else if (motionaction == Moving) {
		if (c->state == Tiled)
			return;
		x = ev->x_root - motionx;
		y = ev->y_root - motiony;
		clientincrmove(c, x, y);
	}
done:
	motionx = ev->x_root;
	motiony = ev->y_root;
}

/* forget about client */
static void
xeventunmapnotify(XEvent *e)
{
	XUnmapEvent *ev = &e->xunmap;
	struct Client *c;

	if ((c = getclient(ev->window)) != NULL && c->win == ev->window)
		clientdel(c);
}

/* clean clients and other structures */
static void
cleanclients(void)
{
	while (clients)
		clientdel(clients);
	while (mons)
		mondel(mons);
}

/* destroy dummy windows */
static void
cleandummywindows(void)
{
	int i;

	XDestroyWindow(dpy, wmcheckwin);
	XDestroyWindow(dpy, focuswin);
	for (i = 0; i < LayerLast; i++)
		XDestroyWindow(dpy, layerwin[i]);
}

/* free cursors */
static void
cleancursors(void)
{
	size_t i;

	for (i = 0; i < CursLast; i++)
		XFreeCursor(dpy, cursor[i]);
}

/* shod window manager */
int
main(void)
{
	XEvent ev;
	XSetWindowAttributes swa;
	void (*xevents[LASTEvent])(XEvent *) = {
		[ButtonPress]      = xeventbuttonpress,
		[ButtonRelease]    = xeventbuttonrelease,
		[ClientMessage]    = xeventclientmessage,
		[ConfigureNotify]  = xeventconfigurenotify,
		[ConfigureRequest] = xeventconfigurerequest,
		[DestroyNotify]    = xeventdestroynotify,
		[EnterNotify]      = xevententernotify,
		[FocusIn]          = xeventfocusin,
		[KeyPress]         = xeventkeypress,
		[MapRequest]       = xeventmaprequest,
		[MotionNotify]     = xeventmotionnotify,
		[UnmapNotify]      = xeventunmapnotify
	};

	/* open connection to server and set X variables */
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	screenw = DisplayWidth(dpy, screen);
	screenh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	gc = XCreateGC(dpy, root, 0, NULL);
	xerrorxlib = XSetErrorHandler(xerror);

	/* initialize resources database */
	XrmInitialize();
	if ((xrm = XResourceManagerString(dpy)) != NULL && (xdb = XrmGetStringDatabase(xrm)) != NULL)
		getresources();

	/* initialize */
	initsignal();
	initdummywindows();
	initcursors();
	initcolors();
	initatoms();

	/* Select SubstructureRedirect events on root window */
	swa.cursor = cursor[CursNormal];
	swa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
	               | SubstructureRedirectMask
	               | SubstructureNotifyMask
	               | StructureNotifyMask
	               | ButtonPressMask;
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &swa);

	/* Set focus to root window */
	XSetInputFocus(dpy, root, RevertToParent, CurrentTime);

	/* set up list of monitors */
	monupdate();
	selmon = mons;

	/* scan windows */
	scan();
	mapfocuswin();

	/* initialize ewmh hints */
	ewmhinit();
	ewmhsetnumberofdesktops();
	ewmhsetcurrentdesktop(0);
	ewmhsetworkarea(screenw, screenh);
	ewmhsetshowingdesktop(0);
	ewmhsetclients();
	ewmhsetclientsstacking();
	ewmhsetwmdesktop();
	ewmhsetactivewindow(None);

	/* run main event loop */
	while (running && !XNextEvent(dpy, &ev))
		if (xevents[ev.type])
			xevents[ev.type](&ev);

	/* clean up */
	cleandummywindows();
	cleancursors();
	cleanclients();

	/* close connection to server */
	XUngrabPointer(dpy, CurrentTime);
	XCloseDisplay(dpy);

	return 0;
}
