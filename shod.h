#ifndef _SHOD_H_
#define _SHOD_H_

#include <X11/Xlib.h>
#include <X11/Xatom.h>

/* macros */
#define LEN(x) (sizeof (x) / sizeof (x[0]))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

/* window states */
#define ISNORMAL     (1 << 0)
#define ISMAXIMIZED  (1 << 1)
#define ISSTICKY     (1 << 2)
#define ISMINIMIZED  (1 << 3)
#define ISFLOATING   (ISNORMAL | ISSTICKY)        /* Window isn't tiled nor fullscreen */
#define ISFREE       (ISSTICKY | ISMINIMIZED)     /* Window belongs to no workspace */
#define ISBOUND      (ISNORMAL | ISMAXIMIZED)     /* Window belongs to a workspace */

/* EWMH atoms */
enum {
	NetSupported,
	NetClientList,
	NetClientListStacking,
	NetNumberOfDesktops,
	NetCurrentDesktop,
	NetActiveWindow,
	NetWMDesktop,
	NetWorkarea,
	NetSupportingWMCheck,
	NetShowingDesktop,
	NetCloseWindow,
	NetMoveresizeWindow,
	NetWMMoveresize,
	NetRequestFrameExtents,
	NetWMName,
	NetWMWindowType,
	NetWMWindowTypeDesktop,
	NetWMWindowTypeMenu,
	NetWMWindowTypeToolbar,
	NetWMWindowTypeDock,
	NetWMWindowTypeDialog,
	NetWMWindowTypeUtility,
	NetWMWindowTypeSplash,
	NetWMState,
	NetWMStateSticky,
	NetWMStateMaximizedVert,
	NetWMStateMaximizedHorz,
	NetWMStateHidden,
	NetWMStateFullscreen,
	NetWMStateAbove,
	NetWMStateBelow,
	NetWMStateFocused,
	NetWMAllowedActions,
	NetWMActionMove,
	NetWMActionResize,
	NetWMActionMinimize,
	NetWMActionStick,
	NetWMActionMaximizeHorz,
	NetWMActionMaximizeVert,
	NetWMActionFullscreen,
	NetWMActionChangeDesktop,
	NetWMActionClose,
	NetWMActionAbove,
	NetWMActionBelow,
	NetWMStrut,
	NetWMStrutPartial,
	NetWMUserTime,
	NetWMStateAttention,
	NetFrameExtents,
	NetDesktopViewport,
	NetLast
};

/* default atoms */
enum {
	WMDeleteWindow,
	WMTakeFocus,
	WMProtocols,
	WMState,
	WMLast
};

/* window layers */
enum {
	LayerDesktop,
	LayerDockapps,
	LayerTiled,
	LayerBelow,
	LayerBottom,
	LayerTop,
	LayerAbove,
	LayerLast
};

/* cursor types */
enum {CursMove, CursNW, CursNE, CursSW, CursSE, CursLast};

/* dock positions */
enum {DockTop, DockBottom, DockLeft, DockRight};

/* dock mode */
enum {DockBelow, DockAside};

/* configuration structure */
struct Config {
	const char *urgent_color;
	const char *focused_color;
	const char *unfocused_color;

	unsigned long urgent;
	unsigned long focused;
	unsigned long unfocused;

	unsigned int modifier;
	unsigned int focusbuttons;
	unsigned int raisebuttons;

	int gapinner, gaptop, gapbottom, gapleft, gapright;
	int border_width;
};

/* contains a list of monitors and of minimized clients */
struct WM {
	struct Monitor *mon;            /* growable array of monitors */
	struct Client *minimized;       /* list of minimized clients */

	int moncount;                   /* number of connected monitors */
	int wscount;                    /* number of workspaces in each monitor */
};

/* contains a list of workspaces and of sticky clients*/
struct Monitor {
	struct Monitor *prev, *next;
	struct WS *ws;
	struct WS *selws;
	struct Client *sticky;
	struct Client *focused;
	int mx, my, mw, mh;         /* Actual monitor size */
	int wx, wy, ww, wh;         /* Size considering gaps, panels and docks */
	int dx, dy, dw, dh;         /* Size considering panels and docks */
	int bl, br, bt, bb;         /* Size of bar on left, right, top and bottom */
};

/* contains a list of columns and of floating clients */
struct WS {
	struct WS *prev, *next;
	struct Monitor *mon;        /* monitor the workspace is connected to */
	struct Column *col;         /* list of columns */
	struct Client *floating;    /* list of unmaximized (stacked) clients */
	struct Client *focused;     /* focused client */
	size_t nclients;            /* number of clients in this workspace */
};

/* contains a list of tiled clients */
struct Column {
	struct Column *prev, *next;
	struct Client *row;
	struct WS *ws;
};

/* regular client structure */
struct Client {
	struct Client *prev, *next;
	struct Client *fprev, *fnext;
	struct Monitor *mon;
	struct WS *ws;
	struct Column *col;
	int ux, uy, uw, uh;         /* unmaximized (floating) geometry */
	int mx, my, mw, mh;         /* maximized (tiling) geometry */
	int basew, baseh;
	int minw, minh;
	int maxw, maxh;
	int incw, inch;
	int isfixed, isuserplaced, isfullscreen;
	int layer;
	unsigned char state;
	float mina, maxa;
	Window win;
};

/* panel client structure */
struct Panel {
	struct Panel *prev, *next;
	struct Monitor *mon;
	Window win;
	int left, right, top, bottom;
};

/* dockapp client structure */
struct Dockapp {
	struct Dockapp *prev, *next;
	Window parent;
	Window win;
	int w, h;
	unsigned pos;
};

/* dock whither dockapps are mapped */
struct Dock {
	char *xpmfile;
	Pixmap xpm;

	char **dockapps;
	size_t ndockapps;

	int mode;
	int position;
	int orientation;
	int size;
	int gapsides;
	int gapback;

	struct Dockapp *beg;
	struct Dockapp *end;
};

/* X stuff */
extern Display *dpy;
extern Window root, wmcheckwin;
extern Cursor cursor[CursLast];
extern Atom utf8string;
extern Atom wmatom[WMLast];
extern Atom netatom[NetLast];
extern Window layerwin[LayerLast];
extern Window focuswin;
extern int (*xerrorxlib)(Display *, XErrorEvent *);
extern int screen;
extern int screenw, screenh;

/* focused client, selected workspace, selected monitor, etc */
extern struct Client *focused;
extern struct WS *selws;
extern struct Monitor *selmon;

/* clients */
extern struct Panel *panels;
extern struct WM wm;

/* The dock */
extern struct Dock dock;

/* flags and arguments */
extern int gflag;   /* whether to ignore outer gaps when a single window is maximized */
extern int bflag;   /* whether to ignore borders when a single window is maximized */
extern char *darg;  /* string of dockapps to be ordered in the dock */

/* configuration */
extern struct Config config;

/* whether shod is running */
extern int running;

#endif /* _SHOD_H_ */
