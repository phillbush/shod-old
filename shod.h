#define DOUBLECLICK  250        /* time in miliseconds of a double click */
#define NAMEMAXLEN   128        /* maximum length of window's name */

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
	Retabbing,
	Button,
	Moving,
	Resizing
};

/* decoration style */
enum {
	Focused,
	Unfocused,
	Urgent,
	StyleLast
};

/* atoms */
enum {
	/* utf8 */
	Utf8String,

	/* ICCCM atoms */
	WMDeleteWindow,
	WMTakeFocus,
	WMProtocols,
	WMState,

	/* EWMH atoms */
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

	ShodTabGroup,

	AtomLast
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
	CursNormal,
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

/* frame region */
enum {
	FrameNone,
	FrameButtonLeft,
	FrameButtonRight,
	FrameTitle,
	FrameBorder,
};

/* auto-tab behavior */
enum {
	NoAutoTab,
	TabFloating,
	TabTilingAlways,
	TabTilingMulti,
	TabAlways
};

/* window eight sections (aka octants) */
enum Octant {
	N  = (1 << 0),
	S  = (1 << 1),
	W  = (1 << 2),
	E  = (1 << 3),
	NW = (1 << 0) | (1 << 2),
	NE = (1 << 0) | (1 << 3),
	SW = (1 << 1) | (1 << 2),
	SE = (1 << 1) | (1 << 3),
};

/* tab structure */
struct Tab {
	struct Tab *prev, *next;
	struct Client *c;
	Window title;
	Window win;
	char *name;
	char *class;
	int ignoreunmap;
	int winw, winh;
	int tabx, tabw;
};

/* client structure */
struct Client {
	struct Client *prev, *next;
	struct Client *fprev, *fnext;
	struct Client *trans;
	struct Monitor *mon;
	struct Desktop *desk;
	struct Row *row;
	struct Tab *tabs;
	struct Tab *seltab;
	int ntabs;
	int ishidden, isuserplaced, isshaded, isfullscreen;
	int state;
	int saveh;              /* original height, used for shading */
	int rh;                 /* row height */
	int x, y, w, h, b, t;   /* current geometry */
	int fx, fy, fw, fh;     /* floating geometry */
	int tx, ty, tw, th;     /* tiled geometry */
	int layer;              /* stacking order */
	long shflags;
	Window curswin;
	Window frame;
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
	const char *theme_path;
	const char *font;

	int edge_width;
	int ignoregaps;
	int ignoretitle;
	int ignoreborders;
	int mergeborders;
	int hidetitle;
	int autotab;

	int gapinner;
	int gapouter;

	unsigned int modifier;
	unsigned int focusbuttons;
	unsigned int raisebuttons;
};

/* decoration sections pixmaps */
struct Decor {
	Pixmap bl;      /* button left */
	Pixmap tl;      /* title left end */
	Pixmap t;       /* title middle */
	Pixmap tr;      /* title right end */
	Pixmap br;      /* button right */
	Pixmap nw;      /* north west corner */
	Pixmap nf;      /* north first edge */
	Pixmap n;       /* north border */
	Pixmap nl;      /* north last edge */
	Pixmap ne;
	Pixmap wf;
	Pixmap w;
	Pixmap wl;
	Pixmap ef;
	Pixmap e;
	Pixmap el;
	Pixmap sw;
	Pixmap sf;
	Pixmap s;
	Pixmap sl;
	Pixmap se;
	unsigned long fg;
	unsigned long bg;
};
