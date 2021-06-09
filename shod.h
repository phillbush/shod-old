#define INDIRECT_SOURCE 1
#define IGNOREUNMAP     6       /* number of unmap notifies to ignore while scanning existing clients */
#define DIV             15      /* number to divide the screen into grids */
#define DOUBLECLICK     250     /* time in miliseconds of a double click */
#define NAMEMAXLEN      1024    /* maximum length of window's name */
#define RULEMINSIZ      23      /* length of "shod.instance..desktop" + 1 for \0 */
#define WIDTH(x)  ((x)->fw + 2 * c->b)
#define HEIGHT(x) ((x)->fh + 2 * c->b + c->t)

/* internal window states */
enum {
	Normal,         /* floating non-sticky window */
	Sticky,         /* floating sticky window */
	Tiled,          /* tiled window */
	Minimized,      /* hidden window */
};

/* role prefixes */
enum {
	TITLE       = 0,
	INSTANCE    = 1,
	CLASS       = 2,
	ROLE        = 3,
	LAST_PREFIX = 4
};

/* role sufixes */
enum {
	DESKTOP     = 0,
	STATE       = 1,
	AUTOTAB     = 2,
	POSITION    = 3,
	LAST_SUFFIX = 4
};

/* EWMH window state actions */
enum {
	STICK,
	MAXIMIZE,
	SHADE,
	HIDE,
	FULLSCREEN,
	ABOVE,
	BELOW,
	FOCUS
};

/* state flag */
enum {
	REMOVE = 0,
	ADD    = 1,
	TOGGLE = 2
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
	FOCUSED,
	UNFOCUSED,
	URGENT,
	STYLE_LAST
};

/* decoration state */
enum {
	/* the first decoration state is used both for focused tab and for unpressed borders */
	UNPRESSED     = 0,
	TAB_FOCUSED   = 0,

	PRESSED       = 1,
	TAB_PRESSED   = 1,

	/* the third decoration state is used for unfocused tab, transient borders, and merged borders */
	TAB_UNFOCUSED = 2,
	TRANSIENT     = 2,
	MERGE_BORDERS = 2,

	DECOR_LAST    = 3
};

/* moveresize action */
enum {
	MOVERESIZE_SIZE_TOPLEFT     = 0,
	MOVERESIZE_SIZE_TOP         = 1,
	MOVERESIZE_SIZE_TOPRIGHT    = 2,
	MOVERESIZE_SIZE_RIGHT       = 3,
	MOVERESIZE_SIZE_BOTTOMRIGHT = 4,
	MOVERESIZE_SIZE_BOTTOM      = 5,
	MOVERESIZE_SIZE_BOTTOMLEFT  = 6,
	MOVERESIZE_SIZE_LEFT        = 7,
	MOVERESIZE_MOVE             = 8,   /* movement only */
	MOVERESIZE_SIZE_KEYBOARD    = 9,   /* size via keyboard */
	MOVERESIZE_MOVE_KEYBOARD    = 10,  /* move via keyboard */
	MOVERESIZE_CANCEL           = 11,  /* cancel operation */
};

/* atoms */
enum {
	/* utf8 */
	Utf8String,

	/* ICCCM atoms */
	WMDeleteWindow,
	WMWindowRole,
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
	NetWMWindowTypePrompt,
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
	CURSOR_NORMAL,
	CURSOR_MOVE,
	CURSOR_NW,
	CURSOR_NE,
	CURSOR_SW,
	CURSOR_SE,
	CURSOR_N,
	CURSOR_S,
	CURSOR_W,
	CURSOR_E,
	CURSOR_PIRATE,
	CURSOR_LAST
};

/* frame region */
enum {
	FrameNone = 0,
	FrameButtonLeft = 1,
	FrameButtonRight = 2,
	FrameTitle = 3,
	FrameBorder = 4,
};

/* auto-tab behavior */
enum {
	NoAutoTab,
	TabFloating,
	TabTilingAlways,
	TabTilingMulti,
	TabAlways,
};

/* window eight sections (aka octants) */
enum Octant {
	C  = 0,
	N  = (1 << 0),
	S  = (1 << 1),
	W  = (1 << 2),
	E  = (1 << 3),
	NW = (1 << 0) | (1 << 2),
	NE = (1 << 0) | (1 << 3),
	SW = (1 << 1) | (1 << 2),
	SE = (1 << 1) | (1 << 3),
};

/* transient window structure */
struct Transient {
	struct Transient *prev, *next;
	struct Tab *t;
	Window frame;
	Window win;
	Pixmap pix;
	int x, y, w, h;
	int maxw, maxh;
	int pw, ph;
	int ignoreunmap;
};

/* prompt structure, used only when calling promptisvalid() */
struct Prompt {
	Window win, frame;
};

/* tab structure */
struct Tab {
	struct Tab *prev, *next;
	struct Client *c;
	struct Transient *trans;
	Window title;
	Window frame;
	Window win;
	Pixmap pix;
	char *name;
	char *class;
	int ignoreunmap;
	int winw, winh;         /* window geometry */
	int x, w;               /* tab geometry */
	int pw;                 /* pixmap width */
};

/* client structure */
struct Client {
	struct Client *prev, *next;
	struct Client *fprev, *fnext;
	struct Client *rprev, *rnext;
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
	int pw, ph;             /* pixmap width and height */
	int fx, fy, fw, fh;     /* floating geometry */
	int tx, ty, tw, th;     /* tiled geometry */
	int layer;              /* stacking order */
	long shflags;
	Window curswin;
	Window frame;
	Pixmap pix;
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
	struct Monitor *mon;
	struct Column *col;
	int n;                  /* desktop number */
};

/* data of a monitor */
struct Monitor {
	struct Monitor *prev, *next;
	struct Desktop *desks;
	struct Desktop *seldesk;
	int mx, my, mw, mh;     /* screen size */
	int wx, wy, ww, wh;     /* window area */
	int gx, gy, gw, gh;     /* window area with gaps */
	int n;                  /* monitor number */
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

	int ignoreindirect;

	int gapinner;
	int gapouter;

	int ndesktops;

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
	Pixmap nw;      /* northwest corner */
	Pixmap nf;      /* north first edge */
	Pixmap n;       /* north border */
	Pixmap nl;      /* north last edge */
	Pixmap ne;      /* northeast corner */
	Pixmap wf;      /* west first edge */
	Pixmap w;       /* west border */
	Pixmap wl;      /* west last edge */
	Pixmap ef;      /* east first edge */
	Pixmap e;       /* east border */
	Pixmap el;      /* east last edge */
	Pixmap sw;      /* southwest corner */
	Pixmap sf;      /* south first edge */
	Pixmap s;       /* south border */
	Pixmap sl;      /* south last edge */
	Pixmap se;      /* southeast corner */
	unsigned long fg;
	unsigned long bg;
};

/* union returned by getclient */
struct Winres {
	struct Client *c;
	struct Tab *t;
	struct Transient *trans;
};

/* rectangle */
struct Outline {
	int x, y, w, h;
	int diffx, diffy;
};

/* window rules read from X resources */
struct Rules {
	int desk;
	int state;
	int autotab;
	int x, y, w, h;
};
