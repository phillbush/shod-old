#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include "shod.h"
#include "init.h"

/* get color from color string */
static unsigned long
getcolor(const char *s)
{
	XColor color;

	if(!XAllocNamedColor(dpy, DefaultColormap(dpy, screen), s, &color, &color))
		errx(1, "cannot allocate color: %s", s);
	return color.pixel;
}

/* initialize atom arrays */
void
init_atoms(void)
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
init_colors(void)
{
	config.unfocused = getcolor(config.unfocused_color);
	config.focused = getcolor(config.focused_color);
	config.urgent = getcolor(config.urgent_color);
}

/* Initialize cursors */
void
init_cursor(void)
{
	cursor[CursMove] = XCreateFontCursor(dpy, XC_fleur);
	cursor[CursNW] = XCreateFontCursor(dpy, XC_top_left_corner);
	cursor[CursNE] = XCreateFontCursor(dpy, XC_top_right_corner);
	cursor[CursSW] = XCreateFontCursor(dpy, XC_bottom_left_corner);
	cursor[CursSE] = XCreateFontCursor(dpy, XC_bottom_right_corner);
}

/* create dummy windows used for controlling the layer of clients */
void
init_layerwindows(void)
{
	int i;

	for (i = 0; i < LayerLast; i++)
		layerwin[i] = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
}

/* get x resources and update variables in config.h */
void
init_resources(void)
{
	char *xrm;
	long n;
	char *type;
	XrmDatabase xdb;
	XrmValue xval;

	XrmInitialize();
	if ((xrm = XResourceManagerString(dpy)) == NULL)
		return;

	xdb = XrmGetStringDatabase(xrm);
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
		config.urgent_color = strdup(xval.addr);
	if (XrmGetResource(xdb, "shod.focused", "*", &type, &xval) == True)
		config.focused_color = strdup(xval.addr);
	if (XrmGetResource(xdb, "shod.unfocused", "*", &type, &xval) == True)
		config.unfocused_color = strdup(xval.addr);

	XrmDestroyDatabase(xdb);
}
