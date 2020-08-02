#ifndef _SHOD_H_
#define _SHOD_H_

#include <X11/Xlib.h>
#include <X11/Xatom.h>

/* macros */
#define LEN(x) (sizeof (x) / sizeof (x[0]))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))
#define WIDTH(x) ((x)->uw + 2 * config.border_width)
#define HEIGHT(x) ((x)->uh + 2 * config.border_width)

/* window states */
#define ISNORMAL     (1 << 0)
#define ISMAXIMIZED  (1 << 1)
#define ISSTICKY     (1 << 2)
#define ISMINIMIZED  (1 << 3)
#define ISVISIBLE    (ISNORMAL | ISMAXIMIZED | ISSTICKY)
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
	NetWorkarea,
	NetSupportingWMCheck,
	NetShowingDesktop,
	NetCloseWindow,
	NetMoveresizeWindow,
	NetWMMoveresize,
	NetRequestFrameExtents,
	NetWMName,
	NetWMDesktop,
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
	LayerTiled,
	LayerBelow,
	LayerBottom,
	LayerTop,
	LayerAbove,
	LayerLast
};

/* cursor types */
enum {CursMove, CursNW, CursNE, CursSW, CursSE, CursLast};

/* configuration structure */
struct Config {
	const char *urgent_color;
	const char *focused_color;
	const char *unfocused_color;

	unsigned long urgent;
	unsigned long focused;
	unsigned long unfocused;

	int gapinner, gaptop, gapbottom, gapleft, gapright;
	int border_width;
};

/* contains a list of monitors and of minimized clients */
struct WM {
	struct Monitor *mon;        /* growable array of monitors */
	struct Client *minimized;   /* list of minimized clients */
};

/* contains a list of workspaces and of sticky clients*/
struct Monitor {
	struct Monitor *prev, *next;
	struct WS *ws;
	struct WS *selws;
	struct Client *sticky;
	struct Client *focused;
    int mx, my, mw, mh;         /* Actual monitor size */
    int wx, wy, ww, wh;         /* Logical size, i.e. where we can place windows */
    int dx, dy, dw, dh;         /* dockless size, for maximized windows */
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

/* dock client structure */
struct Dock {
	struct Dock *prev, *next;
	struct Monitor *mon;
	Window win;
	int left, right, top, bottom;
};

/* X stuff */
extern Display *dpy;
extern Window root, wmcheckwin;
extern int (*xerrorxlib)(Display *, XErrorEvent *);
extern int screen;
extern int screenw, screenh;
extern Cursor cursor[CursLast];

/* mouse buttons and modifiers that control windows */
extern unsigned int modifier;
extern unsigned int focusbuttons;
extern unsigned int raisebuttons;

/* atoms */
extern Atom utf8string;
extern Atom wmatom[WMLast];
extern Atom netatom[NetLast];

/* dummy windows used to restack clients */
extern Window layerwin[LayerLast];

/* focused client, selected workspace, selected monitor, etc */
extern struct Client *focused;
extern struct WS *selws;
extern struct Monitor *selmon;

/* clients */
extern struct Dock *docks;
extern struct WM wm;

/* counters (number of monitors, workspaces, etc) */
extern int moncount, wscount;

/* flags */
extern int gflag;   /* whether to ignore outer gaps when a single window is maximized */
extern int bflag;   /* whether to ignore borders when a single window is maximized */

/* configuration */
extern struct Config config;

#endif /* _SHOD_H_ */
