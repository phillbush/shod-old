/* window states */
enum {
	Normal,         /* floating non-sticky window */
	Sticky,         /* floating sticky window */
	Tiled,          /* tiled window */
	Minimized       /* hidden window */
};

/* motion action */
enum {
	NoAction,
	Moving,
	Resizing
};

/* border style */
enum {
	BorderSolid,
	StyleLast
};

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
	NetWMStateShaded,
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

/* ICCCM atoms */
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
	LayerBars,
	LayerTiled,
	LayerBelow,
	LayerTop,
	LayerAbove,
	LayerFullscreen,
	LayerLast
};

/* cursor types */
enum {
	CursMove,
	CursNW,
	CursNE,
	CursSW,
	CursSE,
	CursN,
	CursS,
	CursW,
	CursE,
	CursLast
};

/* window eight sections (aka octants) */
enum Octant {
	NW,
	NE,
	SW,
	SE,
	N,
	S,
	W,
	E,
};

/* color pixels for borders */
struct Colors {
	unsigned long urgent;
	unsigned long focused;
	unsigned long unfocused;
};

/* client structure */
struct Client {
	struct Client *prev, *next;
	struct Client *fprev, *fnext;
	struct Monitor *mon;
	struct Desktop *desk;
	struct Row *row;
	int isfixed, isuserplaced, isfullscreen;
	int state;
	int rh;                 /* row height */
	int x, y, w, h;         /* current geometry */
	int fx, fy, fw, fh;     /* floating geometry */
	int tx, ty, tw, th;     /* tiled geometry */
	int layer;              /* stacking order */
	int basew, baseh;       /* TODO */
	int minw, minh;         /* TODO */
	int maxw, maxh;         /* TODO */
	int incw, inch;         /* TODO */
	long shflags;
	float mina, maxa;       /* TODO */
	Window win;
};

/* row in a column of tiled windows */
struct Row {
	struct Row *prev, *next;
	struct Column *col;
	struct Client *c;
	int h;          /* row height */
};

/* column of tiled windows */
struct Column {
	struct Column *prev, *next;
	struct Desktop *desk;
	struct Row *row;
	int w;          /* column width */
};

/* desktop of a monitor */
struct Desktop {
	struct Desktop *prev, *next;
	struct Monitor *mon;
	struct Column *col;
	size_t nclients;
};

/* data of a monitor */
struct Monitor {
	struct Monitor *prev, *next;
	struct Desktop *desks;
	struct Desktop *seldesk;
	int mx, my, mw, mh;     /* screen size */
	int wx, wy, ww, wh;     /* window area */
	int gx, gy, gw, gh;     /* window area with gaps */
};

/* configuration set in config.h */
struct Config {
	const char *urgent_color;
	const char *focused_color;
	const char *unfocused_color;

	int border_style;               /* TODO */
	int border_width;
	int corner_width;               /* TODO */
	int ignoregaps;
	int ignoreborders;

	int gapinner;
	int gapouter;

	unsigned int modifier;
	unsigned int focusbuttons;
	unsigned int raisebuttons;
};
