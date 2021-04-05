#include <err.h>
#include <locale.h>
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
#include <X11/xpm.h>
#include <X11/extensions/Xinerama.h>
#include "shod.h"
#include "theme.xpm"

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
static struct Decor decor[StyleLast][3];
static XFontSet fontset;
static Cursor cursor[CursLast];
static int edge;        /* size of the decoration edge */
static int corner;      /* size of the decoration corner */
static int border;      /* size of the decoration border */
static int center;      /* size of the decoration center */
static int button;      /* size of the title bar and the buttons */
static int minsize;     /* minimum size of a window */

/* mouse manipulation variables */
static struct Outline outline;
static int mousex_root = -1, mousey_root = -1;
static int mousex = -1, mousey = -1;
static int mouseaction = NoAction;
static int pressed;
static enum Octant octant = SE;
static struct Client *target = NULL;
static struct Tab *movetab = NULL;      /* tab being boved */

/* dummy windows */
static Window wmcheckwin;
static Window focuswin;                 /* dummy window to get focus */
static Window layerwin[LayerLast];      /* dummy windows used to restack clients */

/* windows, desktops, monitors */
static struct Client *clients = NULL;
static struct Client *focused = NULL;
static struct Monitor *selmon = NULL;
static struct Monitor *mons = NULL;
static unsigned long deskcount = 0;
static int showingdesk = 0;
static XSetWindowAttributes clientswa = {
	.event_mask = EnterWindowMask | SubstructureNotifyMask | ExposureMask
		    | SubstructureRedirectMask | ButtonPressMask | FocusChangeMask
		    | PointerMotionMask
};

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

/* call strndup checking for error */
static char *
estrndup(const char *s, size_t maxlen)
{
	char *p;

	if ((p = strndup(s, maxlen)) == NULL)
		err(1, "strndup");
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

/* get window name */
char *
getwinname(Window win)
{
	XTextProperty tprop;
	char **list = NULL;
	char *name = NULL;
	unsigned char *p = NULL;
	unsigned long size, dl;
	int di;
	Atom da;

	if (XGetWindowProperty(dpy, win, atoms[NetWMName], 0L, NAMEMAXLEN, False, atoms[Utf8String],
	                       &da, &di, &size, &dl, &p) == Success && p) {
		name = estrndup((char *)p, NAMEMAXLEN);
		XFree(p);
	} else if (XGetWMName(dpy, win, &tprop) &&
		   XmbTextPropertyToTextList(dpy, &tprop, &list, &di) == Success &&
		   di > 0 && list && *list) {
		name = estrndup(*list, NAMEMAXLEN);
		XFreeStringList(list);
		XFree(tprop.value);
	}
	return name;
}

/* get configuration from X resources */
static void
getresources(void)
{
	XrmValue xval;
	long n;
	char *type;

	if (XrmGetResource(xdb, "shod.gapInner", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapinner = n;
	if (XrmGetResource(xdb, "shod.gapOuter", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapouter = n;
	if (XrmGetResource(xdb, "shod.hideTitle", "*", &type, &xval) == True)
		config.hidetitle = (strcasecmp(xval.addr, "true") == 0 ||
		                    strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.ignoreGaps", "*", &type, &xval) == True)
		config.ignoregaps = (strcasecmp(xval.addr, "true") == 0 ||
		                     strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.ignoreTitle", "*", &type, &xval) == True)
		config.ignoretitle = (strcasecmp(xval.addr, "true") == 0 ||
		                      strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.ignoreBorders", "*", &type, &xval) == True)
		config.ignoreborders = (strcasecmp(xval.addr, "true") == 0 ||
		                        strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.mergeBorders", "*", &type, &xval) == True)
		config.mergeborders = (strcasecmp(xval.addr, "true") == 0 ||
		                       strcasecmp(xval.addr, "on") == 0);
	if (XrmGetResource(xdb, "shod.theme", "*", &type, &xval) == True)
		config.theme_path = xval.addr;
	if (XrmGetResource(xdb, "shod.font", "*", &type, &xval) == True)
		config.font = xval.addr;
	if (XrmGetResource(xdb, "shod.autoTab", "*", &type, &xval) == True) {
		if (strcasecmp(xval.addr, "floating") == 0) {
			config.autotab = TabFloating;
		} else if (strcasecmp(xval.addr, "tilingAlways") == 0) {
			config.autotab = TabTilingAlways;
		} else if (strcasecmp(xval.addr, "tilingMulti") == 0) {
			config.autotab = TabTilingMulti;
		} else if (strcasecmp(xval.addr, "always") == 0) {
			config.autotab = TabAlways;
		} else {
			config.autotab = NoAutoTab;
		}
	}
}

/* get window's WM_STATE property */
static long
getstate(Window w)
{
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom da;
	int di;

	if (XGetWindowProperty(dpy, w, atoms[WMState], 0L, 2L, False, atoms[WMState],
		&da, &di, &n, &extra, (unsigned char **)&p) != Success)
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

/* initialize font set */
static void
initfontset(void)
{
	char **dp, *ds;
	int di;

	if ((fontset = XCreateFontSet(dpy, config.font, &dp, &di, &ds)) == NULL)
		errx(1, "XCreateFontSet: could not create fontset");
	XFreeStringList(dp);
}

/* initialize cursors */
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
		[ShodTabGroup]               = "_SHOD_TAB_GROUP"
	};

	XInternAtoms(dpy, atomnames, AtomLast, False, atoms);
}

/* create and copy pixmap */
static Pixmap
copypixmap(Pixmap src, int sx, int sy, int w, int h)
{
	Pixmap pix;

	pix = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	XCopyArea(dpy, src, pix, gc, sx, sy, w, h, 0, 0);
	return pix;
}

/* initialize decoration pixmap */
static void
settheme(void)
{
	XGCValues val;
	XpmAttributes xa;
	XImage *img;
	Pixmap pix;
	struct Decor *d;
	unsigned int size;       /* size of each square in the .xpm file */
	unsigned int x, y;
	unsigned int i, j;
	int status;

	memset(&xa, 0, sizeof xa);
	if (config.theme_path)  /* if the we have specified a file, read it instead */
		status = XpmReadFileToImage(dpy, config.theme_path, &img, NULL, &xa);
	else                    /* else use the default theme */
		status = XpmCreateImageFromData(dpy, theme, &img, NULL, &xa);
	if (status != XpmSuccess)
		errx(1, "could not load theme");

	/* create Pixmap from XImage */
	pix = XCreatePixmap(dpy, root, img->width, img->height, img->depth);
	val.foreground = 1;
	val.background = 0;
	XChangeGC(dpy, gc, GCForeground | GCBackground, &val);
	XPutImage(dpy, pix, gc, img, 0, 0, 0, 0, img->width, img->height);

	/* check whether the theme has the correct proportions and hotspots */
	size = 0;
	if (xa.valuemask & (XpmSize | XpmHotspot) &&
	    xa.width % 3 == 0 && xa.height % 3 == 0 && xa.height == xa.width &&
	    (xa.width / 3) % 2 == 1 && (xa.height / 3) % 2 == 1 &&
	    xa.x_hotspot < ((xa.width / 3) - 1) / 2) {
		size = xa.width / 3;
		border = xa.x_hotspot;
		button = xa.y_hotspot;
		corner = border + button;
		edge = (size - 1) / 2 - corner;
		center = size - border * 2;
		minsize = max(1, center);
	}
	if (size == 0) {
		XDestroyImage(img);
		XFreePixmap(dpy, pix);
		errx(1, "theme in wrong format");
	}

	/* destroy pixmap into decoration parts and copy them into the decor array */
	y = 0;
	for (i = 0; i < StyleLast; i++) {
		x = 0;
		for (j = 0; j < 3; j++) {
			d = &decor[i][j];
			d->bl = copypixmap(pix, x + border, y + border, button, button);
			d->tl = copypixmap(pix, x + border + button, y + border, edge, button);
			d->t  = copypixmap(pix, x + border + button + edge, y + border, 1, button);
			d->tr = copypixmap(pix, x + border + button + edge + 1, y + border, edge, button);
			d->br = copypixmap(pix, x + border + button + 2 * edge + 1, y + border, button, button);
			d->nw = copypixmap(pix, x, y, corner, corner);
			d->nf = copypixmap(pix, x + corner, y, edge, border);
			d->n  = copypixmap(pix, x + corner + edge, y, 1, border);
			d->nl = copypixmap(pix, x + corner + edge + 1, y, edge, border);
			d->ne = copypixmap(pix, x + size - corner, y, corner, corner);
			d->wf = copypixmap(pix, x, y + corner, border, edge);
			d->w  = copypixmap(pix, x, y + corner + edge, border, 1);
			d->wl = copypixmap(pix, x, y + corner + edge + 1, border, edge);
			d->ef = copypixmap(pix, x + size - border, y + corner, border, edge);
			d->e  = copypixmap(pix, x + size - border, y + corner + edge, border, 1);
			d->el = copypixmap(pix, x + size - border, y + corner + edge + 1, border, edge);
			d->sw = copypixmap(pix, x, y + size - corner, corner, corner);
			d->sf = copypixmap(pix, x + corner, y + size - border, edge, border);
			d->s  = copypixmap(pix, x + corner + edge, y + size - border, 1, border);
			d->sl = copypixmap(pix, x + corner + edge + 1, y + size - border, edge, border);
			d->se = copypixmap(pix, x + size - corner, y + size - corner, corner, corner);
			d->fg = XGetPixel(img, x + size / 2, y + corner + edge);
			d->bg = XGetPixel(img, x + size / 2, y + border + button / 2);
			x += size;
		}
		y += size;
	}

	XDestroyImage(img);
	XFreePixmap(dpy, pix);
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
shodgroup(void)
{
	struct Client *c;
	struct Tab *t;
	struct Transient *trans;

	for (c = clients; c; c = c->next) {
		if (c->seltab) {
			for (t = c->tabs; t; t = t->next) {
				XChangeProperty(dpy, t->win, atoms[ShodTabGroup], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&c->seltab->win, 1);
				for (trans = t->trans; trans; trans = trans->next) {
					XChangeProperty(dpy, trans->win, atoms[ShodTabGroup], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&c->seltab->win, 1);
				}
			}
		}
	}
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
ewmhsetframeextents(Window win, int b, int t)
{
	unsigned long data[4];

	data[0] = data[1] = data[3] = b;
	data[2] = b + t;

	XChangeProperty(dpy, win, atoms[NetFrameExtents], XA_CARDINAL, 32,
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
	struct Transient *trans;
	struct Tab *t;
	Atom data[6];
	int n = 0;

	if (c == NULL)
		return;
	if (c == getfocused(NULL))
		data[n++] = atoms[NetWMStateFocused];
	if (c->isfullscreen)
		data[n++] = atoms[NetWMStateFullscreen];
	if (c->isshaded)
		data[n++] = atoms[NetWMStateShaded];
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
	for (t = c->tabs; t; t = t->next) {
		XChangeProperty(dpy, t->win, atoms[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)data, n);
		for (trans = t->trans; trans; trans = trans->next) {
			XChangeProperty(dpy, trans->win, atoms[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)data, n);
		}
	}
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
	struct Tab *t;
	struct Transient *trans;

	for (c = clients; c; c = c->next) {
		for (t = c->tabs; t; t = t->next) {
			if (c->state == Sticky || c->state == Minimized) {
				ewmhsetdesktop(t->win, 0xFFFFFFFF);
			} else {
				ewmhsetdesktop(t->win, getdesknum(c->desk));
			}
			for (trans = t->trans; trans; trans = trans->next) {
				if (c->state == Sticky || c->state == Minimized) {
					ewmhsetdesktop(trans->win, 0xFFFFFFFF);
				} else {
					ewmhsetdesktop(trans->win, getdesknum(c->desk));
				}
			}
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
	struct Tab *t;
	struct Transient *trans;
	Window *wins = NULL;
	size_t i = 0, nwins = 0;

	for (c = clients; c; c = c->next) {
		for (t = c->tabs; t; t = t->next) {
			for (trans = t->trans; trans; trans = trans->next) {
				nwins++;
			}
			nwins++;
		}
	}
	if (nwins)
		wins = ecalloc(nwins, sizeof *wins);
	for (c = clients; c; c = c->next) {
		for (t = c->tabs; t; t = t->next) {
			wins[i++] = t->win;
			for (trans = t->trans; trans; trans = trans->next) {
				wins[i++] = trans->win;
			}
		}
	}
	XChangeProperty(dpy, root, atoms[NetClientList], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *)wins, i);
	free(wins);
}

static void
ewmhsetclientsstacking(void)
{
	struct Client *c, *last;
	struct Tab *t;
	struct Transient *trans;
	Window *wins = NULL;
	size_t i = 0, nwins = 0;

	last = NULL;
	for (c = focused; c; c = c->fnext) {
		last = c;
		for (t = c->tabs; t; t = t->next) {
			for (trans = t->trans; trans; trans = trans->next) {
				nwins++;
			}
			nwins++;
		}
	}
	if (nwins)
		wins = ecalloc(nwins, sizeof *wins);
	for (c = last; c; c = c->fprev) {
		for (t = c->tabs; t; t = t->next) {
			if (c->state == Tiled && !c->isfullscreen) {
				wins[i++] = t->win;
				for (trans = t->trans; trans; trans = trans->next) {
					wins[i++] = trans->win;
				}
			}
		}
	}
	for (c = last; c; c = c->fprev) {
		for (t = c->tabs; t; t = t->next) {
			if (c->state != Tiled && !c->isfullscreen && c->layer < 0) {
				wins[i++] = t->win;
				for (trans = t->trans; trans; trans = trans->next) {
					wins[i++] = trans->win;
				}
			}
		}
	}
	for (c = last; c; c = c->fprev) {
		for (t = c->tabs; t; t = t->next) {
			if (c->state != Tiled && !c->isfullscreen && c->layer == 0) {
				wins[i++] = t->win;
				for (trans = t->trans; trans; trans = trans->next) {
					wins[i++] = trans->win;
				}
			}
		}
	}
	for (c = last; c; c = c->fprev) {
		for (t = c->tabs; t; t = t->next) {
			if (c->state != Tiled && !c->isfullscreen && c->layer > 0) {
				wins[i++] = t->win;
				for (trans = t->trans; trans; trans = trans->next) {
					wins[i++] = trans->win;
				}
			}
		}
	}
	for (c = last; c; c = c->fprev) {
		for (t = c->tabs; t; t = t->next) {
			if (c->isfullscreen) {
				wins[i++] = t->win;
				for (trans = t->trans; trans; trans = trans->next) {
					wins[i++] = trans->win;
				}
			}
		}
	}
	XChangeProperty(dpy, root, atoms[NetClientListStacking], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *)wins, i);
	free(wins);
}

static void
ewmhupdate(void)
{
	shodgroup();
	ewmhsetclients();
	ewmhsetclientsstacking();
	ewmhsetwmdesktop();
}

/* get pointer to client, tab or transient structure given a window */
static struct Winres
getwin(Window win)
{
	struct Winres res;
	struct Client *c;
	struct Tab *t;
	struct Transient *trans;

	res.c = NULL;
	res.t = NULL;
	res.trans = NULL;
	for (c = clients; c; c = c->next) {
		if (win == c->frame || win == c->curswin) {
			res.c = c;
			goto done;
		}
		for (t = c->tabs; t; t = t->next) {
			if (win == t->win || win == t->frame || win == t->title) {
				res.c = c;
				res.t = t;
				goto done;
			}
			for (trans = t->trans; trans; trans = trans->next) {
				if (win == trans->win || win == trans->frame) {
					res.c = c;
					res.t = t;
					res.trans = trans;
					goto done;
				}
			}
		}
	}
done:
	return res;
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

/* compute position and width of tabs of a client */
static void
calctabs(struct Client *c)
{
	struct Transient *trans;
	struct Tab *t;
	int i, x;

	x = 0;
	for (i = 0, t = c->tabs; t; t = t->next, i++) {
		t->tabw = max(1, ((i + 1) * (c->w - 2 * button) / c->ntabs) - (i * (c->w - 2 * button) / c->ntabs));
		t->tabx = x;
		x += t->tabw;
		for (trans = t->trans; trans; trans = trans->next) {
			trans->w = max(minsize, min(trans->maxw, c->w - 2 * border));
			trans->h = max(minsize, min(trans->maxh, (c->isshaded ? c->saveh : c->h) - 2 * border));
			trans->x = c->w / 2 - trans->w / 2;
			trans->y = c->h / 2 - trans->h / 2;
		}
	}
}

/* delete transient window from tab */
static void
transdel(struct Transient *trans, int updateprops)
{
	struct Tab *t;

	t = trans->t;
	if (trans->next)
		trans->next->prev = trans->prev;
	if (trans->prev)
		trans->prev->next = trans->next;
	else
		t->trans = trans->next;
	XReparentWindow(dpy, trans->win, root, 0, 0);
	XDestroyWindow(dpy, trans->frame);
	if (updateprops)
		ewmhupdate();
	free(trans);
}

/* decorate transient window */
static void
transdecorate(struct Transient *trans, int style)
{
	XGCValues val;

	val.fill_style = FillSolid;
	val.foreground = decor[style][2].bg;
	XChangeGC(dpy, gc, GCFillStyle | GCForeground, &val);
	XFillRectangle(dpy, trans->frame, gc, border, border, trans->w, trans->h);

	val.fill_style = FillTiled;
	val.tile = decor[style][2].w;
	val.ts_x_origin = 0;
	val.ts_y_origin = 0;
	XChangeGC(dpy, gc, GCFillStyle | GCTile | GCTileStipYOrigin | GCTileStipXOrigin, &val);
	XFillRectangle(dpy, trans->frame, gc, 0, border, border, trans->h + border);

	val.fill_style = FillTiled;
	val.tile = decor[style][2].e;
	val.ts_x_origin = border + trans->w;
	val.ts_y_origin = 0;
	XChangeGC(dpy, gc, GCFillStyle | GCTile | GCTileStipYOrigin | GCTileStipXOrigin, &val);
	XFillRectangle(dpy, trans->frame, gc, border + trans->w, border, border, trans->h + border);

	val.fill_style = FillTiled;
	val.tile = decor[style][2].n;
	val.ts_x_origin = 0;
	val.ts_y_origin = 0;
	XChangeGC(dpy, gc, GCFillStyle | GCTile | GCTileStipYOrigin | GCTileStipXOrigin, &val);
	XFillRectangle(dpy, trans->frame, gc, border, 0, trans->w + 2 * border, border);

	val.fill_style = FillTiled;
	val.tile = decor[style][2].s;
	val.ts_x_origin = 0;
	val.ts_y_origin = border + trans->h;
	XChangeGC(dpy, gc, GCFillStyle | GCTile | GCTileStipYOrigin | GCTileStipXOrigin, &val);
	XFillRectangle(dpy, trans->frame, gc, border, border + trans->h, trans->w + 2 * border, border);

	XCopyArea(dpy, decor[style][2].nw, trans->frame, gc, 0, 0, corner, corner, 0, 0);
	XCopyArea(dpy, decor[style][2].ne, trans->frame, gc, 0, 0, corner, corner, trans->w + 2 * border - corner, 0);
	XCopyArea(dpy, decor[style][2].sw, trans->frame, gc, 0, 0, corner, corner, 0, trans->h + 2 * border - corner);
	XCopyArea(dpy, decor[style][2].se, trans->frame, gc, 0, 0, corner, corner, trans->w + 2 * border - corner, trans->h + 2 * border - corner);
}

/* detach tab from client */
static void
tabdetach(struct Tab *t, int x, int y)
{
	t->winw = t->c->fw;
	t->winh = t->c->fh;
	if (t->c->seltab == t) {
		if (t->prev) {
			t->c->seltab = t->prev;
		} else {
			t->c->seltab = t->next;
		}
	}
	t->c->ntabs--;
	t->ignoreunmap = 2;
	XReparentWindow(dpy, t->title, root, x, y);
	if (t->next)
		t->next->prev = t->prev;
	if (t->prev)
		t->prev->next = t->next;
	else
		t->c->tabs = t->next;
	t->next = NULL;
	t->prev = NULL;
	calctabs(t->c);
}

/* move a detached tab */
static void
tabmove(struct Tab *t, int x, int y)
{
	XMoveWindow(dpy, t->title, x, y);
}

/* delete tab from client */
static void
tabdel(struct Tab *t, int updateprops)
{
	struct Client *c;

	c = t->c;
	while (t->trans)
		transdel(t->trans, 0);
	tabdetach(t, 0, 0);
	XReparentWindow(dpy, t->win, root, c->x, c->y);
	XDestroyWindow(dpy, t->title);
	XDestroyWindow(dpy, t->frame);
	if (updateprops)
		ewmhupdate();
	free(t->name);
	free(t->class);
	free(t);
}

/* update tab title */
static void
tabupdatetitle(struct Tab *t)
{
	free(t->name);
	t->name = getwinname(t->win);
}

/* update tab class */
static void
tabupdateclass(struct Tab *t)
{
	XClassHint chint;

	if (XGetClassHint(dpy, t->win, &chint)) {
		free(t->class);
		t->class = strndup(chint.res_class, NAMEMAXLEN);
		XFree(chint.res_class);
		XFree(chint.res_name);
	}
}

/* add tab into client */
static struct Tab *
tabadd(Window win, int ignoreunmap)
{
	struct Tab *t;

	t = emalloc(sizeof *t);
	t->prev = NULL;
	t->next = NULL;
	t->name = NULL;
	t->class = NULL;
	t->c = NULL;
	t->trans = NULL;
	t->title = None;
	t->ignoreunmap = ignoreunmap;
	t->frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 0,
	                         CopyFromParent, CopyFromParent, CopyFromParent,
	                         CWEventMask, &clientswa);
	t->win = win;
	XReparentWindow(dpy, t->win, t->frame, 0, 0);
	tabupdatetitle(t);
	tabupdateclass(t);
	icccmwmstate(win, NormalState);
	ewmhsetallowedactions(win);
	return t;
}

/* focus a tab */
static void
tabfocus(struct Tab *t)
{
	if (t == NULL)
		return;
	t->c->seltab = t;
	XRaiseWindow(dpy, t->frame);
	if (t->c->isshaded) {
		XSetInputFocus(dpy, t->c->frame, RevertToParent, CurrentTime);
	} else if (t->trans) {
		XRaiseWindow(dpy, t->trans->frame);
		XSetInputFocus(dpy, t->trans->win, RevertToParent, CurrentTime);
	} else {
		XSetInputFocus(dpy, t->win, RevertToParent, CurrentTime);
	}
	shodgroup();
	ewmhsetstate(t->c);
	ewmhsetactivewindow(t->win);
}

/* decorate tab */
static void
tabdecorate(struct Tab *t, int style)
{
	XGCValues val;
	XRectangle box, dr;
	struct Decor *d;
	size_t len;
	int x, y;

	if (t->c && t != t->c->seltab)
		d = &decor[style][2];
	else if (t->c && t->c == target && mouseaction == Moving && pressed == FrameTitle)
		d = &decor[style][1];
	else
		d = &decor[style][0];
	val.tile = d->t;
	val.ts_x_origin = 0;
	val.ts_y_origin = 0;
	val.fill_style = FillTiled;
	XChangeGC(dpy, gc, GCTile | GCTileStipYOrigin | GCTileStipXOrigin | GCFillStyle, &val);
	XCopyArea(dpy, d->tl, t->title, gc, 0, 0, edge, button, 0, 0);
	XFillRectangle(dpy, t->title, gc, edge, 0, t->tabw - edge, button);
	XCopyArea(dpy, d->tr, t->title, gc, 0, 0, edge, button, t->tabw - edge, 0);
	if (t->name != NULL) {
		len = strlen(t->name);
		val.fill_style = FillSolid;
		val.foreground = d->fg;
		XChangeGC(dpy, gc, GCFillStyle | GCForeground, &val);
		XmbTextExtents(fontset, t->name, len, &dr, &box);
		x = (t->tabw - box.width) / 2 - box.x;
		y = (button - box.height) / 2 - box.y;
		XmbDrawString(dpy, t->title, fontset, gc, x, y, t->name, len);
	}
}

/* get decoration style (and state) of client */
static int
clientgetstyle(struct Client *c)
{
	return (c && c == getfocused(NULL)) ? Focused : Unfocused;
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
	struct Tab *t;
	struct Transient *trans;
	XConfigureEvent ce;

	if (c == NULL)
		return;
	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.y = c->y;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = False;
	for (t = c->tabs; t; t = t->next) {
		for (trans = t->trans; trans; trans = trans->next) {
			ce.x = trans->x;
			ce.width = trans->w;
			ce.height = trans->h;
			ce.event = trans->win;
			ce.window = trans->win;
			XSendEvent(dpy, trans->win, False, StructureNotifyMask, (XEvent *)&ce);
		}
		ce.x = c->x;
		ce.width = c->w;
		ce.height = c->isshaded ? c->saveh : c->h;
		ce.event = t->win;
		ce.window = t->win;
		XSendEvent(dpy, t->win, False, StructureNotifyMask, (XEvent *)&ce);
	}
}

/* draw decoration on the frame window */
static void
clientdecorate(struct Client *c, int style, int decorateall)
{
	XGCValues val;
	struct Tab *t;
	struct Decor *d;        /* unpressed decoration */
	struct Decor *dp;       /* pressed decoration */
	int origin;
	int w, h;
	int fullw, fullh;
	int j;

	if (c == NULL)
		return;
	j = 0;
	if ((c->state & Tiled) && config.mergeborders)
		j = 2;
	d = &decor[style][j];
	dp = (c == target && mouseaction == Resizing && j == 0) ? &decor[style][1] : d;
	origin = c->b - border;
	fullw = c->w + c->b * 2;
	fullh = c->h + c->b * 2 + c->t;
	w = fullw - corner * 2 - origin * 2;
	h = fullh - corner * 2 - origin * 2;
	val.fill_style = FillTiled;
	XChangeGC(dpy, gc, GCFillStyle, &val);

	/* draw borders */
	if (w > 0) {
		val.tile = (octant == N) ? dp->n : d->n;
		val.ts_x_origin = origin;
		val.ts_y_origin = origin;
		XChangeGC(dpy, gc, GCTile | GCTileStipYOrigin | GCTileStipXOrigin, &val);
		XFillRectangle(dpy, c->frame, gc, origin + corner, 0, w, c->b);

		val.tile = (octant == S) ? dp->s : d->s;
		val.ts_x_origin = origin;
		val.ts_y_origin = fullh - c->b;
		XChangeGC(dpy, gc, GCTile | GCTileStipYOrigin | GCTileStipXOrigin , &val);
		XFillRectangle(dpy, c->frame, gc, origin + corner, fullh - c->b, w, c->b);
	}

	if (h > 0) {
		val.tile = (octant == W) ? dp->w : d->w;
		val.ts_x_origin = origin;
		val.ts_y_origin = origin;
		XChangeGC(dpy, gc, GCTile | GCTileStipYOrigin | GCTileStipXOrigin , &val);
		XFillRectangle(dpy, c->frame, gc, 0, origin + corner, c->b, h);

		val.tile = (octant == E) ? dp->e : d->e;
		val.ts_x_origin = fullw - c->b;
		val.ts_y_origin = origin;
		XChangeGC(dpy, gc, GCTile | GCTileStipYOrigin | GCTileStipXOrigin , &val);
		XFillRectangle(dpy, c->frame, gc, fullw - c->b, origin + corner, c->b, h);
	}

	/* draw corners and border ends */
	XCopyArea(dpy, (octant == N) ? dp->nf : d->nf, c->frame, gc, 0, 0, edge, border, origin + corner, origin);
	XCopyArea(dpy, (octant == W) ? dp->wf : d->wf, c->frame, gc, 0, 0, border, edge, origin, origin + corner);
	XCopyArea(dpy, (octant == N) ? dp->nl : d->nl, c->frame, gc, 0, 0, edge, border, origin + corner + w - edge, origin);
	XCopyArea(dpy, (octant == E) ? dp->ef : d->ef, c->frame, gc, 0, 0, border, edge, origin + border + c->w, origin + corner);
	XCopyArea(dpy, (octant == S) ? dp->sf : d->sf, c->frame, gc, 0, 0, edge, border, origin + corner, origin + border + c->h);
	XCopyArea(dpy, (octant == W) ? dp->wl : d->wl, c->frame, gc, 0, 0, border, edge, origin, origin + corner + h - edge);
	XCopyArea(dpy, (octant == S) ? dp->sl : d->sl, c->frame, gc, 0, 0, edge, border, origin + corner + w - edge, origin + border + c->h);
	XCopyArea(dpy, (octant == E) ? dp->el : d->el, c->frame, gc, 0, 0, border, edge, origin + border + c->w, origin + corner + h - edge);
	XCopyArea(dpy, (octant == NW || (octant == SW && c->isshaded)) ? dp->nw : d->nw, c->frame, gc, 0, corner/2, corner, corner/2+1, origin, origin + corner/2);
	XCopyArea(dpy, (octant == NE || (octant == SE && c->isshaded)) ? dp->ne : d->ne, c->frame, gc, 0, corner/2, corner, corner/2+1, fullw - corner - origin, origin + corner/2);
	XCopyArea(dpy, (octant == SW || (octant == NW && c->isshaded)) ? dp->sw : d->sw, c->frame, gc, 0, 0, corner, corner/2, origin, fullh - corner - origin);
	XCopyArea(dpy, (octant == SE || (octant == NE && c->isshaded)) ? dp->se : d->se, c->frame, gc, 0, 0, corner, corner/2, fullw - corner - origin, fullh - corner - origin);
	XCopyArea(dpy, (octant == NW || (octant == SW && c->isshaded)) ? dp->nw : d->nw, c->frame, gc, 0, 0, corner, corner/2, origin, origin);
	XCopyArea(dpy, (octant == NE || (octant == SE && c->isshaded)) ? dp->ne : d->ne, c->frame, gc, 0, 0, corner, corner/2, fullw - corner - origin, origin);
	XCopyArea(dpy, (octant == SW || (octant == NW && c->isshaded)) ? dp->sw : d->sw, c->frame, gc, 0, corner/2, corner, corner/2+1, origin, fullh - corner - origin + corner/2);
	XCopyArea(dpy, (octant == SE || (octant == NE && c->isshaded)) ? dp->se : d->se, c->frame, gc, 0, corner/2, corner, corner/2+1, fullw - corner - origin, fullh - corner - origin + corner/2);

	/* draw title and buttons */
	val.foreground = d->bg;
	val.fill_style = FillSolid;
	XChangeGC(dpy, gc, GCFillStyle | GCForeground, &val);
	XFillRectangle(dpy, c->frame, gc, c->b, c->b, c->w, c->h + c->t);
	if (c->t > 0) {
		dp = (c == target && mouseaction == Button && (pressed == FrameButtonLeft || pressed == FrameButtonRight)) ? &decor[style][1] : &decor[style][0];
		XCopyArea(dpy, dp->bl, c->frame, gc, 0, 0, button, button, c->b, c->b);
		XCopyArea(dpy, dp->br, c->frame, gc, 0, 0, button, button, fullw - button - c->b, c->b);
	}
	if (decorateall) {
		for (t = c->tabs; t; t = t->next) {
			if (t->trans) {
				transdecorate(t->trans, style);
			}
			if (c->t > 0) {
				tabdecorate(t, style);
			}
		}
	}
}

/* set client border width */
static void
clientborderwidth(struct Client *c, int border)
{
	if (c == NULL)
		return;
	c->b = border;
}

/* set client title bar width */
static void
clienttitlewidth(struct Client *c, int title)
{
	if (c == NULL)
		return;
	c->t = title;
}

/* move and resize tabs within frame */
static void
clientretab(struct Client *c)
{
	struct Transient *trans;
	struct Tab *t;
	int i;

	for (i = 0, t = c->tabs; t; t = t->next, i++) {
		if (c->isshaded) {
			XMoveResizeWindow(dpy, t->frame, c->b, 2 * c->b + c->t, c->w, c->saveh);
		} else {
			XMoveResizeWindow(dpy, t->frame, c->b, c->b + c->t, c->w, c->h);
		}
		for (trans = t->trans; trans; trans = trans->next) {
			XMoveResizeWindow(dpy, trans->frame, trans->x - border, trans->y - border, trans->w + 2 * border, trans->h + 2 * border);
			XMoveResizeWindow(dpy, trans->win, border, border, trans->w, trans->h);
		}
		XResizeWindow(dpy, t->win, c->w, (c->isshaded ? c->saveh : c->h));
		if (c->t > 0) {
			XMapWindow(dpy, t->title);
			XMoveResizeWindow(dpy, t->title, c->b + button + t->tabx, c->b, t->tabw, c->t);
		} else {
			XUnmapWindow(dpy, t->title);
		}
	}
}

/* commit floating client size and position */
static void
clientmoveresize(struct Client *c)
{
	if (c == NULL)
		return;
	calctabs(c);
	XMoveResizeWindow(dpy, c->frame, c->x - c->b, c->y - c->b - c->t, c->w + c->b * 2, c->h + c->b * 2 + c->t);
	XMoveResizeWindow(dpy, c->curswin, 0, 0, c->w + c->b * 2, c->h + c->b * 2 + c->t);
	clientretab(c);
	if (mouseaction == NoAction) {
		clientnotify(c);
	}
}

/* print client geometry */
static void
clientprintgeometry(struct Client *c)
{
	(void)printf("%dx%d%+d%+d\n", c->w + 2 * c->b, c->h + 2 * c->b + c->t, c->x - c->b, c->y - c->b - c->t);
	(void)fflush(stdout);
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
	int ncols, nrows, nshaded;
	int x, y, w, h;
	int b, g, t;                    /* border and gaps for tiled windows */

	mon = desk->mon;
	b = (config.mergeborders ? (border + 1) / 2 : border);
	g = (config.mergeborders ? 0 : config.gapinner);
	t = (config.hidetitle ? 0 : button);

	/* get number of columns and sum their widths with borders and gaps applied */
	sumw = 0;
	for (ncols = 0, col = desk->col; col; col = col->next, ncols++)
		sumw += col->w + b * 2;
	sumw += g * (ncols - 1);
	if (ncols == 0)
		return;

	/* decide whether to recalculate columns widths */
	recol = (sumw != mon->gw);

	w = (mon->gw - g * (ncols - 1) - b * 2 * ncols) / ncols;
	x = mon->gx;
	for (col = desk->col; col; col = col->next) {
		/* the last column gets the remaining width */
		if (!col->next)
			w = mon->gw + mon->gx - x - 2 * b;

		/* get number of clients in current column and sum their heights */
		sumh = 0;
		for (nshaded = nrows = 0, row = col->row; row; row = row->next, nrows++) {
			sumh += row->c->h + b * 2 + t;
			if (row->c->isshaded)
				nshaded++;
		}
		sumh += g * (nrows - 1);

		/* decide whether to recalculate client heights */
		rerow = (sumh != mon->gh);

		if (nshaded < nrows)
			h = (mon->gh - g * (nrows - nshaded) - b * 2 * nrows - t * nrows) / (nrows - nshaded);
		else
			h = 0;
		y = mon->gy;

		for (row = col->row; row; row = row->next) {
			if (row->c->isfullscreen)
				continue;

			/* the last client gets the remaining height */
			if (!row->next)
				h = mon->gh + mon->gy - y - t - 2 * b;

			/* set new width and heights */
			if (recol || !col->next)
				col->w = w;
			if (rerow || !row->next)
				row->h = h;

			/* if there is only one tiled window, borders or gaps can be ignored */
			clientborderwidth(row->c, b);
			if (!row->c->isshaded)
				clienttitlewidth(row->c, (config.hidetitle ? 0 : button));
			if (nrows == 1 && ncols == 1) {
				if (config.ignoreborders) {
					col->w += 2 * b;
					row->h += 2 * b - t;
					clientborderwidth(row->c, 0);
				}
				if (config.ignoregaps) {
					x = mon->wx;
					y = mon->wy;
					col->w = mon->ww - ((!config.ignoreborders) ? 2 * b : 0);
					row->h = mon->wh - ((!config.ignoreborders) ? 2 * b : 0) - t;
				}
				if (config.ignoretitle && !row->c->isshaded && row->c->ntabs == 1) {
					clienttitlewidth(row->c, 0);
					row->h += t;
				}
			}

			if (row->c->isshaded)
				row->h = 0;

			row->c->x = x + row->c->b;
			row->c->y = y + row->c->b + row->c->t;
			row->c->w = col->w;
			row->c->h = row->h;
			if (clientisvisible(row->c)) {
				clientmoveresize(row->c);
			}

			y += row->h + g + b * 2 + t;
		}

		x += col->w + g + b * 2;
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

/* get the retion position x,y (relative to frame, not the content) is in the frame */
static int
frameregion(struct Client *c, Window win, int x, int y)
{
	struct Tab *t;

	for (t = c->tabs; t; t = t->next)
		if (win == t->title)
			return FrameTitle;
	if (win != c->frame)
		return FrameNone;
	if (x >= c->b && x < c->b + button && y >= c->b && y < c->b + button)
		return FrameButtonLeft;
	if (x >= c->b + c->w - button && x < c->b + c->w && y >= c->b && y < c->b + button)
		return FrameButtonRight;
	if (x >= c->b + button && y >= c->b && x < c->b + c->w - button && y < c->b + button)
		return FrameTitle;
	if (x < c->b || y < c->b || x >= c->b + c->w || y >= c->b + c->h)
		return FrameBorder;
	return FrameNone;
}

/* get in which corner or side of window the cursor is in */
static enum Octant
frameoctant(struct Client *c, Window win, int x, int y)
{
	double tan;
	int wm, hm;
	int w, h;
	int b;

	if (c == NULL || c->state == Minimized)
		return SE;
	if (win == c->frame || win == c->curswin) {
		x -= c->b;
		y -= c->b + c->t;
	}
	w = c->w;
	h = c->h;
	b = corner - c->b;
	if (x >= w - b && y >= h - b)
		return SE;
	if (x >= w - b && y <= 0)
		return NE;
	if (x <= b && y >= h - b)
		return SW;
	if (x <= b && y <= 0)
		return NW;
	if (x < 0)
		return W;
	if (y < 0)
		return N;
	if (x >= w)
		return E;
	if (y >= h)
		return S;
	wm = w / 2;
	hm = h / 2;
	if (c->state == Tiled) {
		tan = (double)h/w;
		if (tan == 0.0)
			tan = 1.0;
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
	} else {
		if (x >= wm && y >= hm) {
			return SE;
		}
		if (x >= wm && y <= hm) {
			return NE;
		}
		if (x <= wm && y >= hm) {
			return SW;
		}
		if (x <= wm && y <= hm) {
			return NW;
		}
	}
	return SE;
}

#define DIV       15      /* number to divide the screen into grids */
#define WIDTH(x)  ((x)->fw + 2 * c->b)
#define HEIGHT(x) ((x)->fh + 2 * c->b + c->t)

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
	int origw, origh;

	if (desk == NULL || c == NULL || c->state == Tiled || c->isfullscreen || c->state == Minimized)
		return;

	mon = desk->mon;

	/* if window is bigger than monitor, resize it while maintaining proportion */
	origw = w = c->fw + 2 * c->b;
	origh = h = c->fh + 2 * c->b + c->t;
	w = min(w, mon->gw);
	h = min(h, mon->gh);
	if (origw - c->fw < origh - c->fh)
		h = (origh * w) / origw;
	else
		w = (origw * h) / origh;
	c->fw = max(minsize, w - (2 * c->b));
	c->fh = max(minsize, h - (2 * c->b + c->t));

	/* if the user placed the window, we should not re-place it */
	if (c->isuserplaced)
		return;

	if (c->fx < mon->gx + c->b || c->fx > mon->gx + c->b + mon->gw)
		c->fx = mon->gx + c->b;
	if (c->fy < mon->gy + c->b || c->fy > mon->gy + c->b + mon->gh)
		c->fy = mon->gy + c->b + c->t;

	/* if this is the first window in the desktop, just center it */
	for (tmp = clients; tmp; tmp = tmp->next) {
		if (((tmp->state == Sticky && tmp->mon == mon) || (tmp->state == Normal && tmp->desk == desk)) && tmp != c) {
			center = 0;
			break;
		}
	}
	if (center) {
		c->fx = mon->gx + c->b + mon->gw / 2 - WIDTH(c) / 2;
		c->fy = mon->gy + c->b + c->t + mon->gh / 2 - HEIGHT(c) / 2;
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
		c->fy = mon->gy + c->b + c->t;
	} else if (posi >= DIV - 1) {
		c->fy = mon->gy + c->b + c->t + mon->gh - HEIGHT(c);
	} else {
		int n, d, maxy;

		n = posi;
		d = DIV;
		maxy = mon->gy + c->b + c->t + mon->gh - HEIGHT(c);
		c->fy = (mon->gy + mon->gh * n)/d - HEIGHT(c);
		c->fy = max(mon->gy + c->b + c->t, c->fy);
		c->fy = min(c->fy, maxy);
	}

	/* calculate x */
	if (posj == 0) {
		c->fx = mon->gx + c->b;
	} else if (posj >= DIV - 1) {
		c->fx = mon->gx + c->b + mon->gw - WIDTH(c);
	} else {
		int n, d, maxx;

		n = posj;
		d = DIV;
		maxx = mon->gx + c->b + mon->gw - WIDTH(c);
		c->fx = (mon->gx + mon->gw * n)/d - WIDTH(c);
		c->fx = max(mon->gx + c->b, c->fx);
		c->fx = min(c->fx, maxx);
	}
}

/* apply size constraints */
static void
clientapplysize(struct Client *c)
{
	if (c == NULL)
		return;
	c->w = max(c->fw, minsize);
	if (!c->isshaded)
		c->h = max(c->fh, minsize);
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
	struct Tab *t;

	if (c == NULL)
		return;
	c->ishidden = hide;
	if (hide) {
		XUnmapWindow(dpy, c->frame);
		for (t = c->tabs; t; t = t->next) {
			icccmwmstate(t->win, IconicState);
		}
	} else {
		XMapWindow(dpy, c->frame);
		for (t = c->tabs; t; t = t->next) {
			icccmwmstate(t->win, NormalState);
		}
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
	ewmhsetstate(c);
}

/* tile/untile client */
static void
clienttile(struct Client *c, int tile)
{
	struct Monitor *mon;
	struct Desktop *desk;
	struct Column *col;
	struct Row *row;

	if (c == NULL || c->state == Minimized || c->isfullscreen)
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
		clientborderwidth(c, border);
		clienttitlewidth(c, (config.hidetitle ? 0 : button));
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
	ewmhsetstate(c);
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
	ewmhsetstate(c);
	clienthide(c, minimize);
}

/* raise client above others */
static void
clientabove(struct Client *c, int above)
{
	if (c == NULL || c->state == Tiled || c->isfullscreen || c->state == Minimized)
		return;
	c->layer = (above) ? 1 : 0;
	clientraise(c);
	ewmhsetstate(c);
}

/* lower client below others */
static void
clientbelow(struct Client *c, int below)
{
	if (c == NULL || c->state == Tiled || c->isfullscreen || c->state == Minimized)
		return;
	c->layer = (below) ? -1 : 0;
	clientraise(c);
	ewmhsetstate(c);
}

/* focus client */
void
clientfocus(struct Client *c)
{
	struct Client *prevfocused, *fullscreen;

	clientdecorate(focused, Unfocused, 1);
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
	clientdecorate(c, Focused, 1);
	if (c->state == Minimized)
		clientminimize(c, 0);
	ewmhsetstate(prevfocused);
	tabfocus(c->seltab);
	ewmhsetclientsstacking();
	ewmhsetcurrentdesktop(getdesknum(selmon->seldesk));
}

/* shade client */
static void
clientshade(struct Client *c, int shade)
{
	if (c == NULL || c->state == Minimized)
		return;
	if (shade && !c->isshaded) {
		clienttitlewidth(c, button);
		if (config.hidetitle)
			c->y += button;
		c->isshaded = 1;
		c->saveh = c->h;
		c->h = 0;
	} else if (!shade && c->isshaded) {
		clienttitlewidth(c, (config.hidetitle ? 0 : button));
		if (config.hidetitle)
			c->y -= button;
		c->isshaded = 0;
		c->h = c->saveh;
	}
	if (clientisvisible(c))
		clientmoveresize(c);
	if (c->state == Tiled)
		desktile(c->desk);
	if (c == getfocused(NULL))
		clientfocus(c);
	ewmhsetstate(c);
}

/* make client fullscreen */
static void
clientfullscreen(struct Client *c, int fullscreen)
{
	if (c == NULL || c->state == Minimized)
		return;
	if (c->state == Sticky)
		clientstick(c, 0);
	if (c->isshaded)
		clientshade(c, 0);
	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		clientborderwidth(c, 0);
		clienttitlewidth(c, 0);
		c->x = c->mon->mx;
		c->y = c->mon->my;
		c->w = c->mon->mw;
		c->h = c->mon->mh;
		if (clientisvisible(c))
			clientmoveresize(c);
	} else if (!fullscreen && c->isfullscreen) {
		c->isfullscreen = 0;
		clienttitlewidth(c, (config.hidetitle ? 0 : button));
		clientborderwidth(c, border);
		if (c->state == Tiled) {
			desktile(c->desk);
		} else if (clientisvisible(c)) {
			clientapplysize(c);
			clientmoveresize(c);
		}
	}
	clientraise(c);
	ewmhsetstate(c);
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

/* resize client x and y pixels out of octant o */
static void
clientincrresize(struct Client *c, enum Octant o, int x, int y)
{
	int origx, origy, origw, origh;

	if (c == NULL || c->state == Minimized || c->isfullscreen)
		return;
	if (c->state == Tiled) {
		if (o & N && c->row->prev) {
			if (c->row->prev->h - y < minsize)
				y = c->row->prev->h - minsize;
			if (c->row->h + y < minsize)
				return;
			c->row->h += y;
			c->row->prev->h -= y;
		}
		if (o & S && c->row->next) {
			if (c->row->next->h - y < minsize)
				y = c->row->next->h - minsize;
			if (c->row->h + y < minsize)
				return;
			c->row->h += y;
			c->row->next->h -= y;
		}
		if (o & W && c->row->col->prev) {
			if (c->row->col->prev->w - x < minsize)
				x = c->row->col->prev->w - minsize;
			if (c->row->col->w + x < minsize)
				return;
			c->row->col->w += x;
			c->row->col->prev->w -= x;
		}
		if (o & E && c->row->col->next) {
			if (c->row->col->next->w - x < minsize)
				x = c->row->col->next->w - minsize;
			if (c->row->col->w + x < minsize)
				return;
			c->row->col->w += x;
			c->row->col->next->w -= x;
		}
		desktile(c->desk);
	} else {
		if (c->fw + x < minsize || c->fh + y < minsize)
			return;
		origx = c->x;
		origy = c->y;
		origw = c->w;
		origh = c->h;
		c->fw += x;
		if (!c->isshaded)
			c->fh += y;
		clientapplysize(c);
		if (o & N)
			c->fy -= c->h - origh;
		if (o & W)
			c->fx -= c->w - origw;
		c->x = c->fx;
		c->y = c->fy;
		clientmoveresize(c);
		if (origx != c->x || origy != c->y || origw != c->w || origh != c->h) {
			clientprintgeometry(c);
		}
	}
}

/* move client x pixels to the right and y pixels down */
static void
clientincrmove(struct Client *c, int x, int y)
{
	struct Monitor *monto;
	struct Column *col = NULL;
	struct Row *row = NULL;
	int tmp;

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
				tmp = row->h;
				row->h = row->prev->h;
				row->prev->h = tmp;
			}
		}
		if (y > 0) {
			if (row->next) {
				row->c = row->next->c;
				row->c->row = row;
				row->next->c = c;
				row->next->c->row = row->next;
				tmp = row->h;
				row->h = row->next->h;
				row->next->h = tmp;
			}
		}
		desktile(c->desk);
	} else {
		c->fx += x;
		c->fy += y;
		c->x = c->fx;
		c->y = c->fy;
		clientmoveresize(c);
		clientprintgeometry(c);
		if (c->state != Sticky) {
			monto = getmon(c->fx + c->fw / 2, c->fy + c->fh / 2);
			if (monto && monto != c->mon) {
				clientsendtodesk(c, monto->seldesk, 0, 1);
			}
		}
	}
}

/* add client */
static struct Client *
clientadd(int x, int y, int w, int h, int isuserplaced)
{
	struct Client *c;

	c = emalloc(sizeof *c);
	c->fprev = c->fnext = NULL;
	c->mon = NULL;
	c->desk = NULL;
	c->row = NULL;
	c->isfullscreen = 0;
	c->isuserplaced = isuserplaced;
	c->isshaded = 0;
	c->trans = NULL;
	c->ishidden = 0;
	c->state = Normal;
	c->layer = 0;
	c->x = c->fx = x;
	c->y = c->fy = y;
	c->w = c->fw = w;
	c->h = c->fh = h;
	c->saveh = c->h;
	c->b = border;
	c->t = (config.hidetitle ? 0 : button);
	c->seltab = NULL;
	c->tabs = NULL;
	c->ntabs = 0;
	c->prev = NULL;
	c->frame = XCreateWindow(dpy, root, c->x - c->b, c->y - c->b,
	                         c->w + c->b * 2, c->h + c->b * 2 + c->t, 0,
	                         CopyFromParent, CopyFromParent, CopyFromParent,
	                         CWEventMask, &clientswa);
	c->curswin = XCreateWindow(dpy, c->frame, 0, 0, c->w + c->b * 2, c->h + c->b * 2 + c->t, 0,
	                           CopyFromParent, InputOnly, CopyFromParent, 0, NULL);
	if (clients)
		clients->prev = c;
	c->next = clients;
	clients = c;
	XMapWindow(dpy, c->curswin);
	return c;
}

/* delete client */
static void
clientdel(struct Client *c, int updateprops)
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
	while (c->tabs)
		tabdel(c->tabs, updateprops);
	XDestroyWindow(dpy, c->frame);
	XDestroyWindow(dpy, c->curswin);
	free(c);
}

/* add tab into client w*/
static void
clienttab(struct Client *c, struct Tab *t, int pos)
{
	struct Client *oldc;
	struct Tab *tmp, *prev;
	int i;

	oldc = t->c;
	t->c = c;
	c->seltab = t;
	c->ntabs++;
	if (pos == 0 || c->tabs == NULL) {
		t->prev = NULL;
		t->next = c->tabs;
		if (c->tabs)
			c->tabs->prev = t;
		c->tabs = t;
	} else {
		for (i = 0, prev = tmp = c->tabs; tmp && (pos < 0 || i < pos); tmp = tmp->next, i++)
			prev = tmp;
		if (prev->next)
			prev->next->prev = t;
		t->next = prev->next;
		t->prev = prev;
		prev->next = t;
	}
	calctabs(c);
	if (t->title == None) {
		t->title = XCreateWindow(dpy, c->frame, c->b + button + t->tabx, c->b, t->tabw, button, 0,
		                         CopyFromParent, CopyFromParent, CopyFromParent,
		                         CWEventMask, &clientswa);
	} else {
		XReparentWindow(dpy, t->title, c->frame, c->b, c->b);
	}
	XReparentWindow(dpy, t->frame, c->frame, c->b, c->b + c->t);
	XMapWindow(dpy, t->title);
	XMapWindow(dpy, t->frame);
	XMapSubwindows(dpy, t->frame);
	tabfocus(t);
	if (clientisvisible(c)) {
		tabfocus(t);
	}
	if (oldc) {     /* deal with the frame this tab came from */
		if (oldc->ntabs == 0) {
			clientdel(oldc, 0);
		} else if (oldc->state == Tiled) {
			desktile(oldc->desk);
		}
	}
	shodgroup();
	ewmhsetframeextents(t->win, c->b, c->t);
	ewmhsetclients();
	ewmhsetclientsstacking();
	ewmhsetwmdesktop();
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

	if (c == NULL || c->state == Minimized || c->isfullscreen)
		return;
	if (valuemask & CWX)
		wc->x -= c->b;
	if (valuemask & CWY)
		wc->y -= c->b + c->t;
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

/* check if new client size is ok */
static int
clientvalidsize(struct Client *c, enum Octant o, int dx, int dy)
{
	if (dy != 0) {
		if (c->state == Tiled) {
			if (o & N) {
				return c->row->prev &&
				       c->row->prev->h - dy >= minsize &&
				       c->row->h + dy >= minsize;
			} else if (o & S) {
				return c->row->next &&
				       c->row->next->h - dy >= minsize &&
				       c->row->h + dy >= minsize;
			}
		} else {
			return c->h + dy >= minsize;
		}
	} else if (dx != 0) {
		if (c->state == Tiled) {
			if (o & W) {
				return c->row->col->prev &&
				       c->row->col->prev->w - dx >= minsize &&
				       c->row->col->w + dx >= minsize;
			} else if (o & E) {
				return c->row->col->next &&
				       c->row->col->next->w - dx >= minsize &&
				       c->row->col->w + dx >= minsize;
			}
		} else {
			return c->w + dx >= minsize;
		}
	}
	return 0;
}

/* configure transient window */
static void
transconfigure(struct Transient *trans, unsigned int valuemask, XWindowChanges *wc)
{
	if (trans == NULL)
		return;
	if (valuemask & CWWidth) {
		trans->maxw = wc->width;
	}
	if (valuemask & CWHeight) {
		trans->maxh = wc->height;
	}
	if (clientisvisible(trans->t->c)) {
		clientmoveresize(trans->t->c);
	}
}

/* check if there is a titlebar of a client under cursor; return client */
static struct Client *
getclientbytitle(int x, int y, int *pos)
{
	struct Client *c;

	for (c = clients; c; c = c->next) {
		if (clientisvisible(c) && y >= c->y - c->t - c->b && y < c->y && x >= c->x && x < c->x + c->w) {
			*pos = (1 + (2 * c->ntabs * (x - c->x)) / c->w) / 2;
			return c;
		}
	}
	return NULL;
}

/* check whether to place new window in tab rather than in new frame*/
static int
autotab(struct Tab *t)
{
	/* auto tab should be anabled */
	if (config.autotab == NoAutoTab)
		return 0;

	/* there should be a focused frame */
	if (!focused || focused != getfocused(NULL))
		return 0;

	/* focused frame must be unshaded with title bar visible */
	if (focused->isfullscreen || focused->state == Minimized)
		return 0;

	/* classes must match */
	if (!t->class || !focused->seltab->class || strcmp(t->class, focused->seltab->class) != 0)
		return 0;

	switch (config.autotab) {
	case TabFloating:
		return focused->state != Tiled;
	case TabTilingAlways:
		return focused->state == Tiled;
	case TabTilingMulti:
		return focused->state == Tiled && (focused->desk->col->next || focused->desk->col->row->next);
	case TabAlways:
		return 1;
	}
	return 0;
}

/* send a WM_DELETE message to client */
static void
windowclose(Window win)
{
	XEvent ev;

	ev.type = ClientMessage;
	ev.xclient.window = win;
	ev.xclient.message_type = atoms[WMProtocols];
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = atoms[WMDeleteWindow];
	ev.xclient.data.l[1] = CurrentTime;

	/*
	 * communicate with the given Client, kindly telling it to
	 * close itself and terminate any associated processes using
	 * the WM_DELETE_WINDOW protocol
	 */
	XSendEvent(dpy, win, False, NoEventMask, &ev);
}

/* prepare window to be managed */
static void
preparewin(Window win)
{
	XSelectInput(dpy, win, EnterWindowMask | StructureNotifyMask
	                     | PropertyChangeMask | FocusChangeMask);
	XGrabButton(dpy, AnyButton, AnyModifier, win, False,
	            ButtonPressMask | ButtonReleaseMask,
	            GrabModeSync, GrabModeSync, None, None);
	XSetWindowBorderWidth(dpy, win, 0);
}

/* create client for tab */
static void
manage(struct Client *c, struct Tab *t)
{
	struct Client *f;
	unsigned long *values;
	int focus = 1;          /* whether to focus window */

	clienttab(c, t, 0);
	clientnotify(c);
	clientraise(c);
	f = getfocused(NULL);
	if ((values = getcardinalprop(t->win, atoms[NetWMUserTime], 1)) != NULL) {
		if (values[0] == 0)
			focus = 0;
		XFree(values);
	}
	if (focus && f != NULL && f->isfullscreen)
		focus = 0;
	if (!focus)
		clientdecorate(c, Unfocused, 1);
	clientsendtodesk(c, selmon->seldesk, 1, focus);
}

/* add transient window into tab */
static void
managetrans(struct Tab *t, Window win, int maxw, int maxh, int ignoreunmap)
{
	struct Transient *trans;

	trans = emalloc(sizeof *trans);
	trans->prev = NULL;
	trans->next = NULL;
	trans->t = t;
	trans->x = 0;
	trans->y = 0;
	trans->w = 0;
	trans->h = 0;
	trans->maxw = maxw;
	trans->maxh = maxh;
	trans->ignoreunmap = ignoreunmap;
	trans->frame = XCreateWindow(dpy, root, 0, 0, maxw, maxh, 0,
	                             CopyFromParent, CopyFromParent, CopyFromParent,
	                             CWEventMask, &clientswa);
	trans->win = win;
	XReparentWindow(dpy, trans->frame, t->frame, 0, 0);
	XReparentWindow(dpy, trans->win, trans->frame, 0, 0);
	if (t->trans)
		t->trans->prev = trans;
	trans->next = t->trans;
	t->trans = trans;
	icccmwmstate(win, NormalState);
	if (clientisvisible(t->c)) {
		clientdecorate(t->c, clientgetstyle(t->c), 1);
		clientmoveresize(t->c);
	}
	XMapRaised(dpy, trans->frame);
	XMapRaised(dpy, trans->win);
	tabfocus(t);
}

/* delete tab (and its client if it is the only tab) */
static void
unmanage(struct Tab *t)
{
	struct Client *c;

	c = t->c;
	tabdel(t, 1);
	calctabs(c);
	if (c->ntabs == 0) {
		clientdel(c, 1);
	} else {
		clientdecorate(c, clientgetstyle(c), 1);
		clientmoveresize(c);
		if (c == getfocused(NULL)) {
			tabfocus(c->seltab);
		}
	}
}

/* scan for already existing windows and adopt them */
static void
scan(void)
{
	struct Winres res;
	struct Client *c;
	struct Tab *t;
	unsigned int i, num;
	Window d1, d2, transwin, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState) {
				preparewin(wins[i]);
				t = tabadd(wins[i], 2);
				c = clientadd(wa.x, wa.y, wa.width, wa.height, 0);
				manage(c, t);
			}
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &transwin) &&
			   (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)) {
				preparewin(wins[i]);
				res = getwin(transwin);
				if (res.t != NULL) {
					managetrans(res.t, transwin, wa.width, wa.height, 2);
				} else {
					t = tabadd(wins[i], 2);
					c = clientadd(wa.x, wa.y, wa.width, wa.height, 0);
					manage(c, t);
				}
			}
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

/* draw outline while resizing */
static void
outlinedraw(void)
{
	static struct Outline oldoutline = {0, 0, 0, 0, 0, 0};
	XGCValues val;
	XRectangle rects[4];

	val.function = GXinvert;
	val.subwindow_mode = IncludeInferiors;
	val.foreground = 1;
	val.fill_style = FillSolid;
	XChangeGC(dpy, gc, GCFunction | GCSubwindowMode | GCForeground | GCFillStyle, &val);
	if (oldoutline.w != 0 && oldoutline.h != 0) {
		rects[0].x = oldoutline.x + 1;
		rects[0].y = oldoutline.y;
		rects[0].width = oldoutline.w - 2;
		rects[0].height = 1;
		rects[1].x = oldoutline.x;
		rects[1].y = oldoutline.y;
		rects[1].width = 1;
		rects[1].height = oldoutline.h;
		rects[2].x = oldoutline.x + 1;
		rects[2].y = oldoutline.y + oldoutline.h - 1;
		rects[2].width = oldoutline.w - 2;
		rects[2].height = 1;
		rects[3].x = oldoutline.x + oldoutline.w - 1;
		rects[3].y = oldoutline.y;
		rects[3].width = 1;
		rects[3].height = oldoutline.h;
		XFillRectangles(dpy, root, gc, rects, 4);
	}
	if (outline.w != 0 && outline.h != 0) {
		rects[0].x = outline.x + 1;
		rects[0].y = outline.y;
		rects[0].width = outline.w - 2;
		rects[0].height = 1;
		rects[1].x = outline.x;
		rects[1].y = outline.y;
		rects[1].width = 1;
		rects[1].height = outline.h;
		rects[2].x = outline.x + 1;
		rects[2].y = outline.y + outline.h - 1;
		rects[2].width = outline.w - 2;
		rects[2].height = 1;
		rects[3].x = outline.x + outline.w - 1;
		rects[3].y = outline.y;
		rects[3].width = 1;
		rects[3].height = outline.h;
		XFillRectangles(dpy, root, gc, rects, 4);
	}
	oldoutline = outline;
	val.function = GXcopy;
	val.subwindow_mode = ClipByChildren;
	XChangeGC(dpy, gc, GCFunction | GCSubwindowMode, &val);
}

/* ungrab pointer and set mouse action to NoAction */
static void
ungrab(void)
{
	XUngrabPointer(dpy, CurrentTime);
	mouseaction = NoAction;
	mousex_root = -1;
	mousey_root = -1;
	mousex = -1;
	mousey = -1;
	movetab = NULL;
	target = NULL;
	pressed = FrameNone;
}

/* handle mouse operation, focusing and raising */
static void
xeventbuttonpress(XEvent *e)
{
	struct Winres res;
	static Time lasttime = 0;
	static struct Client *lastc = NULL;
	XButtonPressedEvent *ev = &e->xbutton;
	struct Monitor *mon;
	struct Client *c;
	struct Tab *t;
	Window grabwin = None;
	Cursor curs = None;
	unsigned int mask;
	int region;
	int focus = 0;
	int raise = 0;

	res = getwin(ev->window);
	c = res.c;
	t = res.t;

	/* if user clicked in no window, focus the monitor below cursor */
	if (c == NULL) {
		mon = getmon(ev->x_root, ev->y_root);
		if (mon)
			deskchange(mon->seldesk);
		return;
	}

	/* get region and octant of the frame the mouse pointer is in */
	region = frameregion(c, ev->window, ev->x, ev->y);
	octant = frameoctant(c, ev->window, ev->x, ev->y);

	/* get action performed by mouse */
	if (ev->button == Button1 && (region == FrameButtonLeft || region == FrameButtonRight)) {
		mouseaction = Button;
	} else if (ev->state == config.modifier && ev->button == Button1) {
		mouseaction = Moving;
	} else if (ev->state == config.modifier && ev->button == Button3) {
		mouseaction = Resizing;
	} else if (region == FrameBorder && ev->button == Button3) {
		mouseaction = Moving;
	} else if (region == FrameBorder && ev->button == Button1) {
		mouseaction = Resizing;
	} else if (region == FrameTitle && ev->button == Button3) {
		mouseaction = Retabbing;
	} else if (region == FrameTitle && ev->button == Button1) {
		tabfocus(t);
		if (lastc == c && ev->time - lasttime < DOUBLECLICK) {
			clientshade(c, !c->isshaded);
			lasttime = 0;
			lastc = NULL;
			return;
		}
		pressed = FrameTitle;
		lastc = c;
		lasttime = ev->time;
		mouseaction = Moving;
	} else {
		mouseaction = NoAction;
	}

	/* operate on basis of mouse action */
	switch (mouseaction) {
	case Button:
		pressed = region;
		grabwin = c->frame;
		mask = ButtonReleaseMask;
		curs = cursor[CursNormal];
		break;
	case Retabbing:
		if (t == NULL || t->title != ev->window) {
			mouseaction = NoAction;
			break;
		}
		mask = ButtonReleaseMask | Button3MotionMask;
		grabwin = t->title;
		movetab = t;
		mousex = ev->x;
		mousey = ev->y;
		tabdetach(t, ev->x_root - mousex, ev->y_root - mousey);
		tabfocus(c->seltab);
		clientretab(c);
		curs = cursor[CursNormal];
		break;
	case Moving:
		mask = ButtonReleaseMask | Button1MotionMask | Button3MotionMask;
		grabwin = c->frame;
		curs = cursor[CursMove];
		break;
	case Resizing:
		if (c->state == Tiled &&
		    ((c->row->col->next == NULL && octant & E) ||
		     (c->row->col->prev == NULL && octant & W) ||
		     (c->row->prev == NULL && octant & N) ||
		     (c->row->next == NULL && octant & S))) {
			mouseaction = NoAction;
			break;
		}
		outline.x = c->x - c->b;
		outline.y = c->y - c->b - c->t;
		outline.w = c->w + 2 * c->b;
		outline.h = c->h + 2 * c->b + c->t;
		outline.diffx = 0;
		outline.diffy = 0;
		mask = ButtonReleaseMask | Button1MotionMask | Button3MotionMask;
		grabwin = c->frame;
		switch (octant) {
		case NW:
			curs = c->isshaded ? cursor[CursW] : cursor[CursNW];
			mousex = ev->x_root - c->x + c->b;
			mousey = ev->y_root - c->y + c->b + c->t;
			break;
		case NE:
			curs = c->isshaded ? cursor[CursE] : cursor[CursNE];
			mousex = c->x + c->w + c->b - ev->x_root;
			mousey = ev->y_root - c->y + c->b + c->t;
			break;
		case SW:
			curs = c->isshaded ? cursor[CursW] : cursor[CursSW];
			mousex = ev->x_root - c->x + c->b;
			mousey = c->y + c->h + c->b - ev->y_root;
			break;
		case SE:
			curs = c->isshaded ? cursor[CursE] : cursor[CursSE];
			mousex = c->x + c->w + c->b - ev->x_root;
			mousey = c->y + c->h + c->b - ev->y_root;
			break;
		case N:
			curs = cursor[CursN];
			mousey = ev->y_root - c->y + c->b + c->t;
			break;
		case S:
			curs = cursor[CursS];
			mousey = c->y + c->h + c->b - ev->y_root;
			break;
		case W:
			curs = cursor[CursW];
			mousex = ev->x_root - c->x + c->b;
			break;
		case E:
			curs = cursor[CursE];
			mousex = c->x + c->w + c->b - ev->x_root;
			break;
		}
		break;
	}

	/* if performing a mouse action, set global variables and grab the pointer */
	if (mouseaction != NoAction) {
		target = c;
		mousex_root = ev->x_root;
		mousey_root = ev->y_root;
		XGrabPointer(dpy, grabwin, False, mask, GrabModeAsync, GrabModeAsync, None, curs, CurrentTime);
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
	else
		clientdecorate(c, clientgetstyle(c), 1);

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

	XAllowEvents(dpy, ReplayPointer, CurrentTime);
}

/* interrupt moving/resizing action */
static void
xeventbuttonrelease(XEvent *e)
{
	XButtonReleasedEvent *ev = &e->xbutton;
	struct Client *c;
	int region;
	int pos;

	if (mouseaction == Retabbing && movetab != NULL) {
		c = getclientbytitle(ev->x_root, ev->y_root, &pos);
		if (c != NULL) {
			clienttab(c, movetab, pos);
		} else {
			c = clientadd(ev->x_root, ev->y_root, movetab->winw, movetab->winh, 0);
			manage(c, movetab);
		}
	} else {
		c = target;
		if (c != NULL && ev->window == c->frame && mouseaction == Button && target == c) {
			region = frameregion(c, ev->window, ev->x, ev->y);
			if (pressed == region) {
				switch (region) {
				case FrameButtonLeft:
					clientminimize(c, 1);
					break;
				case FrameButtonRight:
					if (c->seltab) {
						windowclose(c->seltab->win);
					}
					break;
				}
			}
		}
	}
	if (c) {
		clientnotify(c);
	}
	if (mouseaction == Resizing) {
		clientincrresize(c, octant, outline.diffx, outline.diffy);
		outline.x = 0;
		outline.y = 0;
		outline.w = 0;
		outline.h = 0;
		outline.diffx = 0;
		outline.diffy = 0;
		outlinedraw();
	}
	ungrab();
	if (c != NULL) {
		clientdecorate(c, clientgetstyle(c), 1);
		if (clientisvisible(c)) {
			clientmoveresize(c);
		}
	}
}

/* handle client message event */
static void
xeventclientmessage(XEvent *e)
{
	XClientMessageEvent *ev = &e->xclient;
	XWindowChanges wc;
	unsigned value_mask = 0;
	struct Winres res;
	struct Client *c;
	Cursor curs = None;
	int i;

	res = getwin(ev->window);
	c = res.c;
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
		ewmhsetframeextents(ev->window, c->b, c->t);
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
		for (i = 1; i < 3; i++) {
			if ((Atom)ev->data.l[i] == atoms[NetWMStateFullscreen])
				clientfullscreen(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !c->isfullscreen)));
			else if ((Atom)ev->data.l[i] == atoms[NetWMStateShaded])
				clientshade(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !c->isshaded)));
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
			if (res.t != NULL)
				c->seltab = res.t;
			deskchange(c->desk);
			clientfocus(c);
			clientraise(c);
		}
	} else if (ev->message_type == atoms[NetCloseWindow]) {
		if (c == NULL)
			return;
		windowclose(ev->window);
	} else if (ev->message_type == atoms[NetMoveresizeWindow] && mouseaction == NoAction) {
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
		if (res.trans != NULL) {
			transconfigure(res.trans, value_mask, &wc);
		} else {
			clientconfigure(c, value_mask, &wc);
		}
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
			ungrab();
		} else {
			switch (ev->data.l[2]) {
			case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
				octant = NW;
				curs = cursor[CursNW];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_TOP:
				octant = N;
				curs = cursor[CursN];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
				octant = NE;
				curs = cursor[CursNE];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_RIGHT:
				octant = E;
				curs = cursor[CursE];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
				octant = SE;
				curs = cursor[CursSE];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
				octant = S;
				curs = cursor[CursS];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
				octant = SW;
				curs = cursor[CursSW];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_SIZE_LEFT:
				octant = W;
				curs = cursor[CursW];
				mouseaction = Resizing;
				break;
			case _NET_WM_MOVERESIZE_MOVE:
				curs = cursor[CursMove];
				mouseaction = Moving;
				break;
			default:
				return;
			}
			target = c;
			mousex_root = ev->data.l[0];
			mousey_root = ev->data.l[1];
			if (octant & W)
				mousex = ev->data.l[0] - c->x + c->b;
			else if (octant & E)
				mousex = c->x + c->w + c->b - ev->data.l[0];
			else
				mousex = 0;
			if (octant & N)
				mousey = ev->data.l[1] - c->y + c->b + c->t;
			else if (octant & S)
				mousey = c->y + c->h + c->b - ev->data.l[1];
			else
				mousey = 0;
			XGrabPointer(dpy, c->frame, False,
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
	struct Winres res;

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = ev->border_width;
	wc.sibling = ev->above;
	wc.stack_mode = ev->detail;
	res = getwin(ev->window);
	if (res.trans != NULL && mouseaction == NoAction) {
		transconfigure(res.trans, ev->value_mask, &wc);
	} else if (res.c != NULL && mouseaction == NoAction) {
		clientconfigure(res.c, ev->value_mask, &wc);
	} else if (res.c == NULL){
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
}

/* forget about client */
static void
xeventdestroynotify(XEvent *e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	struct Winres res;

	res = getwin(ev->window);
	if (res.trans && ev->window == res.trans->win) {
		transdel(res.trans, 1);
	} else if (res.t && ev->window == res.t->win) {
		unmanage(res.t);
	}
}

/* focus window when cursor enter it (only if there is no focus button) */
static void
xevententernotify(XEvent *e)
{
	struct Winres res;

	if (config.focusbuttons)
		return;
	while (XCheckTypedEvent(dpy, EnterNotify, e))
		;
	res = getwin(e->xcrossing.window);
	if (res.c != NULL)
		clientfocus(res.c);
}

/* redraw window decoration */
static void
xeventexpose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;
	struct Winres res;

	if (ev->count == 0) {
		if (movetab && (ev->window == movetab->title ||
		    (movetab->trans && ev->window == movetab->trans->frame))) {
			tabdecorate(movetab, clientgetstyle(movetab->c));
		} else {
			res = getwin(ev->window);
			if (res.c && ev->window == res.c->frame) {
				clientdecorate(res.c, clientgetstyle(res.c), 0);
			} else if (res.t && ev->window == res.t->title) {
				tabdecorate(res.t, clientgetstyle(res.c));
			} else if (res.trans && ev->window == res.trans->frame) {
				transdecorate(res.trans, clientgetstyle(res.c));
			}
		}
	}
}

/* handle focusin event */
static void
xeventfocusin(XEvent *e)
{
	(void)e;

	clientfocus(getfocused(NULL));
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
	struct Winres res;
	struct Client *c;
	struct Tab *t;
	struct Tab *transfor = NULL;
	XMapRequestEvent *ev = &e->xmaprequest;
	XWindowAttributes wa;
	XSizeHints size;
	Atom prop = None;
	Window win, wins[2];
	int isuserplaced = 0;
	long dl;


	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	wins[1] = ev->window;
	prop = getatomprop(ev->window, atoms[NetWMWindowType]);
	if (prop == atoms[NetWMWindowTypeDesktop]) {
		XMapWindow(dpy, ev->window);
		wins[0] = layerwin[LayerDesktop];
		XRestackWindows(dpy, wins, sizeof wins);
	} else if (prop == atoms[NetWMWindowTypeDock]) {
		XMapWindow(dpy, ev->window);
		wins[0] = layerwin[LayerBars];
		XRestackWindows(dpy, wins, sizeof wins);
	} else {
		res = getwin(ev->window);
		if (res.c != NULL)
			return;
		preparewin(ev->window);
		if (XGetTransientForHint(dpy, ev->window, &win)) {
			res = getwin(win);
			transfor = res.t;
		}
		if (XGetWMNormalHints(dpy, ev->window, &size, &dl) && (size.flags & USPosition)) {
			isuserplaced = 1;
		}
		if (transfor != NULL) {
			managetrans(transfor, ev->window, wa.width, wa.height, 0);
			return;
		}
		t = tabadd(ev->window, 0);
		if (!isuserplaced && autotab(t)) {
			clienttab(focused, t, -1);
			clientdecorate(focused, Focused, 1);
			clientmoveresize(focused);
			if (focused->state == Tiled) {
				desktile(focused->desk);
			}
		} else {
			t = tabadd(ev->window, 0);
			c = clientadd(wa.x, wa.y, wa.width, wa.height, isuserplaced);
			manage(c, t);
		}
	}
}

/* run mouse action */
static void
xeventmotionnotify(XEvent *e)
{
	XMotionEvent *ev = &e->xmotion;
	struct Winres res;
	int x = 0, y = 0;
	int b;

	switch (mouseaction) {
	case NoAction:
		res = getwin(ev->window);
		if (res.c == NULL || ev->subwindow != res.c->curswin)
			return;
		if (frameregion(res.c, ev->window, ev->x, ev->y) == FrameBorder) {
			switch (frameoctant(res.c, ev->window, ev->x, ev->y)) {
			case NW:
				XDefineCursor(dpy, res.c->curswin, (res.c->isshaded ? cursor[CursW] : cursor[CursNW]));
				break;
			case NE:
				XDefineCursor(dpy, res.c->curswin, (res.c->isshaded ? cursor[CursE] : cursor[CursNE]));
				break;
			case SW:
				XDefineCursor(dpy, res.c->curswin, (res.c->isshaded ? cursor[CursW] : cursor[CursSW]));
				break;
			case SE:
				XDefineCursor(dpy, res.c->curswin, (res.c->isshaded ? cursor[CursE] : cursor[CursSE]));
				break;
			case N:
				XDefineCursor(dpy, res.c->curswin, cursor[CursN]);
				break;
			case S:
				XDefineCursor(dpy, res.c->curswin, cursor[CursS]);
				break;
			case W:
				XDefineCursor(dpy, res.c->curswin, cursor[CursW]);
				break;
			case E:
				XDefineCursor(dpy, res.c->curswin, cursor[CursE]);
				break;
			}
		} else {
			XDefineCursor(dpy, res.c->curswin, cursor[CursNormal]);
		}
		break;
	case Retabbing:
		if (movetab == NULL) {
			mouseaction = NoAction;
			XUngrabPointer(dpy, CurrentTime);
			return;
		}
		tabmove(movetab, ev->x_root - mousex, ev->y_root - mousey);
		break;
	case Resizing:
		if (mousex > outline.w)
			mousex = 0;
		if (mousey > outline.h)
			mousey = 0;
		if (octant & W &&
		    ((ev->x_root < mousex_root && mousex > ev->x_root - outline.x) ||
		     (ev->x_root > mousex_root && mousex < ev->x_root - outline.x))) {
			x = mousex_root - ev->x_root;
			if (clientvalidsize(target, octant, outline.diffx + x, 0)) {
				outline.x -= x;
				outline.w += x;
				outline.diffx += x;
			}
		} else if (octant & E &&
		    ((ev->x_root > mousex_root && mousex > outline.x + outline.w - ev->x_root) ||
		     (ev->x_root < mousex_root && mousex < outline.x + outline.w - ev->x_root))) {
			x = ev->x_root - mousex_root;
			if (clientvalidsize(target, octant, outline.diffx + x, 0)) {
				outline.w += x;
				outline.diffx += x;
			}
		}
		if (octant & N &&
		    ((ev->y_root < mousey_root && mousey > ev->y_root - outline.y) ||
		     (ev->y_root > mousey_root && mousey < ev->y_root - outline.y))) {
			y = mousey_root - ev->y_root;
			if (clientvalidsize(target, octant, 0, outline.diffy + y)) {
				outline.y -= y;
				outline.h += y;
				outline.diffy += y;
			}
		} else if (octant & S &&
		    ((ev->y_root > mousey_root && mousey > outline.y + outline.h - ev->y_root) ||
		     (ev->y_root < mousey_root && mousey < outline.y + outline.h - ev->y_root))) {
			y = ev->y_root - mousey_root;
			if (clientvalidsize(target, octant, 0, outline.diffy + y)) {
				outline.h += y;
				outline.diffy += y;
			}
		}
		outlinedraw();
		mousex_root = ev->x_root;
		mousey_root = ev->y_root;
		break;
	case Moving:
		if (target->state == Tiled) {
			b = target->b + target->t + config.gapinner;
			if (ev->x_root > target->x + target->w + (target->row->col->next ? b : 0))
				x = +1;
			else if (ev->x_root < target->x - (target->row->col->prev ? b : 0))
				x = -1;
			else
				x = 0;
			if (ev->y_root > target->y + target->h + (target->row->next ? b : 0))
				y = +1;
			else if (ev->y_root < target->y - (target->row->prev ? b : 0))
				y = -1;
			else
				y = 0;
		} else {
			x = ev->x_root - mousex_root;
			y = ev->y_root - mousey_root;
		}
		clientincrmove(target, x, y);
		mousex_root = ev->x_root;
		mousey_root = ev->y_root;
		break;
	}
}

/* update client properties */
static void
xeventpropertynotify(XEvent *e)
{
	XPropertyEvent *ev = &e->xproperty;
	struct Winres res;

	res = getwin(ev->window);
	if (res.t == NULL)
		return;
	if (ev->atom == XA_WM_NAME || ev->atom == atoms[NetWMName]) {
		tabupdatetitle(res.t);
		clientdecorate(res.t->c, clientgetstyle(res.t->c), 1);
	} else if (ev->atom == XA_WM_CLASS) {
		tabupdateclass(res.t);
	}
}

/* forget about client */
static void
xeventunmapnotify(XEvent *e)
{
	XUnmapEvent *ev = &e->xunmap;
	struct Winres res;

	res = getwin(ev->window);
	if (res.trans && ev->window == res.trans->win) {
		if (res.trans->ignoreunmap) {
			res.trans->ignoreunmap--;
		} else {
			transdel(res.trans, 1);
		}
	} else if (res.t && ev->window == res.t->win) {
		if (res.t->ignoreunmap) {
			res.t->ignoreunmap--;
		} else {
			unmanage(res.t);
		}
	}
}

/* clean clients and other structures */
static void
cleanclients(void)
{
	while (clients) {
		if (clients->ishidden)
			clienthide(clients, 0);
		if (clients->isshaded)
			clientshade(clients, 0);
		clientdel(clients, 1);
	}
	while (mons) {
		mondel(mons);
	}
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

/* free pixmaps */
static void
cleanpixmaps(void)
{
	int i, j;

	for (i = 0; i < StyleLast; i++) {
		for (j = 0; i < 2; i++) {
			XFreePixmap(dpy, decor[i][j].bl);
			XFreePixmap(dpy, decor[i][j].tl);
			XFreePixmap(dpy, decor[i][j].t);
			XFreePixmap(dpy, decor[i][j].tr);
			XFreePixmap(dpy, decor[i][j].br);
			XFreePixmap(dpy, decor[i][j].nw);
			XFreePixmap(dpy, decor[i][j].nf);
			XFreePixmap(dpy, decor[i][j].n);
			XFreePixmap(dpy, decor[i][j].nl);
			XFreePixmap(dpy, decor[i][j].ne);
			XFreePixmap(dpy, decor[i][j].wf);
			XFreePixmap(dpy, decor[i][j].w);
			XFreePixmap(dpy, decor[i][j].wl);
			XFreePixmap(dpy, decor[i][j].ef);
			XFreePixmap(dpy, decor[i][j].e);
			XFreePixmap(dpy, decor[i][j].el);
			XFreePixmap(dpy, decor[i][j].sw);
			XFreePixmap(dpy, decor[i][j].sf);
			XFreePixmap(dpy, decor[i][j].s);
			XFreePixmap(dpy, decor[i][j].sl);
			XFreePixmap(dpy, decor[i][j].se);
		}
	}
}

/* free fontset */
static void
cleanfontset(void)
{
	XFreeFontSet(dpy, fontset);
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
		[Expose]           = xeventexpose,
		[FocusIn]          = xeventfocusin,
		[KeyPress]         = xeventkeypress,
		[MapRequest]       = xeventmaprequest,
		[MotionNotify]     = xeventmotionnotify,
		[PropertyNotify]   = xeventpropertynotify,
		[UnmapNotify]      = xeventunmapnotify
	};

	/* open connection to server and set X variables */
	if (!setlocale(LC_ALL, "") || !XSupportsLocale())
		warnx("warning: no locale support");
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	screenw = DisplayWidth(dpy, screen);
	screenh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	gc = XCreateGC(dpy, root, 0, NULL);
	xerrorxlib = XSetErrorHandler(xerror);

	/* initialize resources database */
	config.theme_path = NULL;
	XrmInitialize();
	if ((xrm = XResourceManagerString(dpy)) != NULL && (xdb = XrmGetStringDatabase(xrm)) != NULL)
		getresources();

	/* initialize */
	initsignal();
	initdummywindows();
	initfontset();
	initcursors();
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

	/* setup theme */
	settheme();

	/* scan windows */
	scan();
	mapfocuswin();

	/* run main event loop */
	while (running && !XNextEvent(dpy, &ev))
		if (xevents[ev.type])
			(*xevents[ev.type])(&ev);

	/* clean up */
	cleandummywindows();
	cleancursors();
	cleanclients();
	cleanpixmaps();
	cleanfontset();

	/* close connection to server */
	XUngrabPointer(dpy, CurrentTime);
	XCloseDisplay(dpy);

	return 0;
}
