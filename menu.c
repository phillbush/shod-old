#include <X11/Xutil.h>
#include "shod.h"
#include "menu.h"

void
menu_add(Window win)
{
	XMapWindow(dpy, win);
}
