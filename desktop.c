#include <X11/Xutil.h>
#include "shod.h"
#include "desktop.h"

void
desktop_add(Window win)
{
	Window wins[2];

	XMapWindow(dpy, win);

	wins[0] = layerwin[LayerDesktop];
	wins[1] = win;
	XRestackWindows(dpy, wins, sizeof wins);
}
