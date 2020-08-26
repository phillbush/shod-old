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
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include "shod.h"
#include "init.h"
#include "monitor.h"
#include "ewmh.h"
#include "scan.h"
#include "xevent.h"

enum ParseButtons {Focus, Raise};

/* function declarations */
static void parsebuttons(const char *s, enum ParseButtons b);
static void parsemodifier(const char *s);
static void sigint(int);
static int xerror(Display *dpy, XErrorEvent *ee);
static void cleanup(void);
static void usage(void);

/* X stuff */
Display *dpy;
Window root, wmcheckwin;
int (*xerrorxlib)(Display *, XErrorEvent *);
int screen;
int screenw, screenh;
Cursor cursor[CursLast];

/* mouse buttons and modifiers that control windows */
unsigned int modifier = Mod1Mask;
unsigned int focusbuttons = 0;
unsigned int raisebuttons = ~0;

/* counters (number of monitors, workspaces, etc) */
int moncount = 0, wscount = 0;

/* atoms */
Atom utf8string;
Atom wmatom[WMLast];
Atom netatom[NetLast];

/* dummy windows used to restack clients */
Window layerwin[LayerLast];

/* focused client, selected workspace, selected monitor, etc */
struct Client *focused = NULL;
struct WS *selws = NULL;
struct Monitor *selmon = NULL;

/* clients */
struct Dock *docks = NULL;
struct WM wm = {NULL, NULL};

/* flags */
int gflag = 0;  /* whether to ignore outer gaps when a single window is maximized */
int bflag = 0;  /* whether to ignore borders when a single window is maximized */

/* whether shod is running */
int running = 1;

/* the config structure */
#include "config.h"

/* shod window manager */
int
main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "bf:gm:r:")) != -1) {
		switch (ch) {
		case 'b':
			bflag = 1;
			break;
		case 'f':
			parsebuttons(optarg, Focus);
			break;
		case 'g':
			gflag = 1;
			break;
		case 'm':
			parsemodifier(optarg);
			break;
		case 'r':
			parsebuttons(optarg, Raise);
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

	/* Select SubstructureRedirect events on root window */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask
	             | SubstructureNotifyMask | PropertyChangeMask
	             | StructureNotifyMask | ButtonPressMask);

	/* Set focus to root window */
	XSetInputFocus(dpy, root, RevertToParent, CurrentTime);

	/* setup */
	init_resources();
	init_colors();
	init_cursor();
	init_atoms();
	init_layerwindows();

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
	XCloseDisplay(dpy);
	warnx("bye");

	return 0;
}

/* parse buttons string */
static void
parsebuttons(const char *s, enum ParseButtons b)
{
	const char *origs;

	origs = s;
	if (b == Focus)
		focusbuttons = 0;
	else
		raisebuttons = 0;
	while (*s != '\0') {
		if (*s <= '0' || *s > '5')
			errx(1, "improper buttons string %s", origs);
		if (b == Focus)
			focusbuttons |= 1 << (*s - '0');
		else
			raisebuttons |= 1 << (*s - '0');
		s++;
	}
}

/* parse modifier string */
static void
parsemodifier(const char *s)
{
	if (strcasecmp(s, "Mod1") == 0)
		modifier = Mod1Mask;
	else if (strcasecmp(s, "Mod2") == 0)
		modifier = Mod2Mask;
	else if (strcasecmp(s, "Mod3") == 0)
		modifier = Mod3Mask;
	else if (strcasecmp(s, "Mod4") == 0)
		modifier = Mod4Mask;
	else if (strcasecmp(s, "Mod5") == 0)
		modifier = Mod5Mask;
	else
		errx(1, "improper modifier string %s", s);
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

/* clean up */
static void
cleanup(void)
{
	struct Monitor *mon;
	struct Client *c;
	struct Dock *d;
	size_t i;

	d = docks;
	while (d) {
		struct Dock *tmp;

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

	XUngrabPointer(dpy, CurrentTime);

	for (i = 0; i < CursLast; i++)
		XFreeCursor(dpy, cursor[i]);

	XDestroyWindow(dpy, wmcheckwin);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: shod [-bg] [-f buttons] [-m modifier] [-r buttons]\n");
	exit(1);
}
