/* See README file for copyright and license details. */

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/xpm.h>
#include "shod.h"
#include "ewmh.h"
#include "manage.h"
#include "monitor.h"
#include "util.h"
#include "xevent.h"

/* X stuff */
static XrmDatabase xdb;
static XrmValue xval;
static char *xrm;
Display *dpy;
Window root, wmcheckwin;
Cursor cursor[CursLast];
Atom utf8string;
Atom wmatom[WMLast];
Atom netatom[NetLast];
Window layerwin[LayerLast];     /* dummy windows used to restack clients */
Window focuswin;                /* dummy window to get focus */
int (*xerrorxlib)(Display *, XErrorEvent *);
int screen;
int screenw, screenh;

/* focused client, selected workspace, selected monitor, etc */
struct Client *focused;
struct WS *selws;
struct Monitor *selmon;

/* clients */
struct Panel *panels;
struct WM wm;

/* flags and arguments */
int gflag = 0;      /* whether to ignore outer gaps when a single window is maximized */
int bflag = 0;      /* whether to ignore borders when a single window is maximized */
char *darg = NULL;  /* string of dockapps to be ordered in the dock */

/* whether shod is running */
int running = 1;

/* the config structure */
#include "config.h"

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: shod [-bg] [-d dockapps] [-f buttons] [-m modifier] [-r buttons]\n");
	exit(1);
}

/* parse buttons string */
static void
parsebuttons(const char *s, unsigned int *buttons)
{
	const char *origs;

	origs = s;
	*buttons = 0;
	while (*s != '\0') {
		if (*s <= '0' || *s > '5')
			errx(1, "improper buttons string %s", origs);
		*buttons |= 1 << (*s - '0');
		s++;
	}
}

/* parse modifier string */
static void
parsemodifier(const char *s)
{
	if (strcasecmp(s, "Mod1") == 0)
		config.modifier = Mod1Mask;
	else if (strcasecmp(s, "Mod2") == 0)
		config.modifier = Mod2Mask;
	else if (strcasecmp(s, "Mod3") == 0)
		config.modifier = Mod3Mask;
	else if (strcasecmp(s, "Mod4") == 0)
		config.modifier = Mod4Mask;
	else if (strcasecmp(s, "Mod5") == 0)
		config.modifier = Mod5Mask;
	else
		errx(1, "improper modifier string %s", s);
}

/* get x resources and update variables in config.h */
void
getresources(void)
{
	long n;
	char *type;

	if (xrm == NULL || xdb == NULL)
		return;

	if (XrmGetResource(xdb, "shod.borderWidth", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.border_width = n;
	if (XrmGetResource(xdb, "shod.gapLeft", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapleft = n;
	if (XrmGetResource(xdb, "shod.gapRight", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapright = n;
	if (XrmGetResource(xdb, "shod.gapTop", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gaptop = n;
	if (XrmGetResource(xdb, "shod.gapBottom", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapbottom = n;
	if (XrmGetResource(xdb, "shod.gapInner", "*", &type, &xval) == True)
		if ((n = strtol(xval.addr, NULL, 10)) > 0)
			config.gapinner = n;
	if (XrmGetResource(xdb, "shod.urgent", "*", &type, &xval) == True)
		config.urgent_color = xval.addr;
	if (XrmGetResource(xdb, "shod.focused", "*", &type, &xval) == True)
		config.focused_color = xval.addr;
	if (XrmGetResource(xdb, "shod.unfocused", "*", &type, &xval) == True)
		config.unfocused_color = xval.addr;
	if (XrmGetResource(xdb, "shod.dock", "*", &type, &xval) == True) {
		XpmAttributes xa;
		Pixmap pi;

		xa.valuemask = 0;
		XpmReadFileToPixmap(dpy, root, xval.addr, &dock.xpm, &pi, &xa);
	}
}

/* get configuration from command-line */
static void
getoptions(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "bd:f:gm:r:")) != -1) {
		switch (ch) {
		case 'b':
			bflag = 1;
			break;
		case 'd':
			darg = optarg;
			break;
		case 'f':
			parsebuttons(optarg, &config.focusbuttons);
			break;
		case 'g':
			gflag = 1;
			break;
		case 'm':
			parsemodifier(optarg);
			break;
		case 'r':
			parsebuttons(optarg, &config.raisebuttons);
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();
}

/* get color from color string */
static unsigned long
ealloccolor(const char *s)
{
	XColor color;

	if(!XAllocNamedColor(dpy, DefaultColormap(dpy, screen), s, &color, &color))
		errx(1, "cannot allocate color: %s", s);
	return color.pixel;
}

/* initialize atom arrays */
void
initatoms(void)
{
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);

	/* ewmh supported atoms */
	netatom[NetSupported]               = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetClientList]              = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetClientListStacking]      = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
	netatom[NetNumberOfDesktops]        = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	netatom[NetCurrentDesktop]          = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	netatom[NetActiveWindow]            = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetWorkarea]                = XInternAtom(dpy, "_NET_WORKAREA", False);
	netatom[NetSupportingWMCheck]       = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetShowingDesktop]          = XInternAtom(dpy, "_NET_SHOWING_DESKTOP", False);
	netatom[NetCloseWindow]             = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
	netatom[NetMoveresizeWindow]        = XInternAtom(dpy, "_NET_MOVERESIZE_WINDOW", False);
	netatom[NetWMMoveresize]            = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
	netatom[NetRequestFrameExtents]     = XInternAtom(dpy, "_NET_REQUEST_FRAME_EXTENTS", False);
	netatom[NetWMName]                  = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMWindowType]            = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDesktop]     = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	netatom[NetWMWindowTypeDock]        = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	netatom[NetWMWindowTypeToolbar]     = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
	netatom[NetWMWindowTypeMenu]        = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	netatom[NetWMWindowTypeSplash]      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	netatom[NetWMWindowTypeDialog]      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetWMWindowTypeUtility]     = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	netatom[NetWMState]                 = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMStateSticky]           = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
	netatom[NetWMStateMaximizedVert]    = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	netatom[NetWMStateMaximizedHorz]    = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	netatom[NetWMStateHidden]           = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
	netatom[NetWMStateFullscreen]       = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMStateAbove]            = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
	netatom[NetWMStateBelow]            = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
	netatom[NetWMStateFocused]          = XInternAtom(dpy, "_NET_WM_STATE_FOCUSED", False);
	netatom[NetWMAllowedActions]        = XInternAtom(dpy, "_NET_WM_ALLOWED_ACTIONS", False);
	netatom[NetWMActionMove]            = XInternAtom(dpy, "_NET_WM_ACTION_MOVE", False);
	netatom[NetWMActionResize]          = XInternAtom(dpy, "_NET_WM_ACTION_RESIZE", False);
	netatom[NetWMActionMinimize]        = XInternAtom(dpy, "_NET_WM_ACTION_MINIMIZE", False);
	netatom[NetWMActionStick]           = XInternAtom(dpy, "_NET_WM_ACTION_STICK", False);
	netatom[NetWMActionMaximizeHorz]    = XInternAtom(dpy, "_NET_WM_ACTION_MAXIMIZE_HORZ", False);
	netatom[NetWMActionMaximizeVert]    = XInternAtom(dpy, "_NET_WM_ACTION_MAXIMIZE_VERT", False);
	netatom[NetWMActionFullscreen]      = XInternAtom(dpy, "_NET_WM_ACTION_FULLSCREEN", False);
	netatom[NetWMActionChangeDesktop]   = XInternAtom(dpy, "_NET_WM_ACTION_CHANGE_DESKTOP", False);
	netatom[NetWMActionClose]           = XInternAtom(dpy, "_NET_WM_ACTION_CLOSE", False);
	netatom[NetWMActionAbove]           = XInternAtom(dpy, "_NET_WM_ACTION_ABOVE", False);
	netatom[NetWMActionBelow]           = XInternAtom(dpy, "_NET_WM_ACTION_BELOW", False);
	netatom[NetWMStrut]                 = XInternAtom(dpy, "_NET_WM_STRUT", False);
	netatom[NetWMStrutPartial]          = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
	netatom[NetWMUserTime]              = XInternAtom(dpy, "_NET_WM_USER_TIME", False);
	netatom[NetWMStateAttention]        = XInternAtom(dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
	netatom[NetWMDesktop]               = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	netatom[NetFrameExtents]            = XInternAtom(dpy, "_NET_FRAME_EXTENTS", False);
	netatom[NetDesktopViewport]         = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);

	/* Some icccm atoms */
	wmatom[WMDeleteWindow] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
}

/* initialize colors and conf array */
void
initcolors(void)
{
	config.unfocused = ealloccolor(config.unfocused_color);
	config.focused = ealloccolor(config.focused_color);
	config.urgent = ealloccolor(config.urgent_color);
}

/* Initialize cursors */
void
initcursor(void)
{
	cursor[CursMove] = XCreateFontCursor(dpy, XC_fleur);
	cursor[CursNW] = XCreateFontCursor(dpy, XC_top_left_corner);
	cursor[CursNE] = XCreateFontCursor(dpy, XC_top_right_corner);
	cursor[CursSW] = XCreateFontCursor(dpy, XC_bottom_left_corner);
	cursor[CursSE] = XCreateFontCursor(dpy, XC_bottom_right_corner);
}

/* initialize the dock */
void
initdock(void)
{
	char *s, *p;
	size_t i;

	dock.dockapps = NULL;
	dock.ndockapps = 0;
	dock.beg = NULL;
	dock.end = NULL;
	dock.xpm = None;
	if (darg) {
		if ((s = strdup(darg)) == NULL)
			err(1, "strdup");
		dock.ndockapps = 1;
		for (p = darg; *p; p++)
			if (*p == ';')
				dock.ndockapps++;
		if ((dock.dockapps = calloc(dock.ndockapps, sizeof *(dock.dockapps))) == NULL)
			err(1, "calloc");
		for (i = 0, p = strtok(s, ";"); p && i < dock.ndockapps; i++, p = strtok(NULL, ";")) {
			if ((dock.dockapps[i] = strdup(p)) == NULL)
				err(1, "strdup");
		}
		free(s);
	}
}

/* create dummy windows used for controlling the layer of clients */
void
initdummywindows(void)
{
	int i;

	focuswin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	for (i = 0; i < LayerLast; i++)
		layerwin[i] = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
}

/* signal-catching function */
static void
sigint(int signo)
{
	(void)signo;
	running = 0;
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

/* scan for already existing clients */
void
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
				manage(wins[i]);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i]);
		}
		if (wins)
			XFree(wins);
	}
}

/* clean up */
static void
cleanup(void)
{
	struct Monitor *mon;
	struct Client *c;
	struct Panel *d;
	size_t i;

	d = panels;
	while (d) {
		struct Panel *tmp;

		tmp = d->next;
		free(d);
		d = tmp;
	}

	c = wm.minimized;
	while (c) {
		struct Client *tmp;

		tmp = c->next;
		free(c);
		c = tmp;
	}

	mon = wm.mon;
	while (mon) {
		struct Monitor *tmp;

		tmp = mon->next;
		c = monitor_del(mon, NULL);
		mon = tmp;

		while (c) {
			struct Client *tmp;

			tmp = c->next;
			free(c);
			c = tmp;
		}
	}
	if (dock.ndockapps > 0) {
		for (i = 0; i < dock.ndockapps; i++)
			free(dock.dockapps[i]);
		free(dock.dockapps);
	}

	XUngrabPointer(dpy, CurrentTime);

	for (i = 0; i < CursLast; i++)
		XFreeCursor(dpy, cursor[i]);

	XDestroyWindow(dpy, focuswin);
	for (i = 0; i < LayerLast; i++)
		XDestroyWindow(dpy, layerwin[i]);
	XDestroyWindow(dpy, wmcheckwin);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

/* shod window manager */
int
main(int argc, char *argv[])
{

	if (signal(SIGINT, sigint) == SIG_ERR)
		err(1, "signal");

	/* open connection to server and set X variables */
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		warnx("warning: no locale support");
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	screenw = DisplayWidth(dpy, screen);
	screenh = DisplayHeight(dpy, screen);
	xerrorxlib = XSetErrorHandler(xerror);
	XrmInitialize();
	if ((xrm = XResourceManagerString(dpy)) != NULL)
		xdb = XrmGetStringDatabase(xrm);

	/* get configuration */
	getresources();
	getoptions(argc, argv);

	/* Select SubstructureRedirect events on root window */
	XSelectInput(dpy, root, SubstructureRedirectMask
	             | SubstructureNotifyMask | PropertyChangeMask
	             | StructureNotifyMask | ButtonPressMask);

	/* Set focus to root window */
	XSetInputFocus(dpy, root, RevertToParent, CurrentTime);

	/* setup */
	initdock();
	initcolors();
	initcursor();
	initatoms();
	initdummywindows();

	/* initialize wm structure with a list of monitors */
	monitor_update();
	selmon = wm.mon;
	selws = wm.mon->ws;
	selmon->selws = wm.mon->ws;

	ewmh_init();
	ewmh_setnumberofdesktops();
	ewmh_setcurrentdesktop(0);
	ewmh_setworkarea();
	ewmh_setshowingdesktop(0);
	ewmh_setclients();
	ewmh_setclientsstacking();
	ewmh_setwmdesktop();
	ewmh_setactivewindow(None);

	/* scan existing windows and adopt them */
	scan();

	xevent_run();

	cleanup();

	/* close connection to server */
	XrmDestroyDatabase(xdb);
	XCloseDisplay(dpy);

	warnx("bye");
	return 0;
}
