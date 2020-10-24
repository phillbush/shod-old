#include "shod.h"
#include "title.h"

Window
title_createwin(struct Client *c)
{
	Window wins[2];
	XSetWindowAttributes swa;
	Window ret;

	swa.event_mask = EnterWindowMask | StructureNotifyMask | ButtonPressMask | FocusChangeMask;
	swa.background_pixel = config.focused;
	ret = XCreateWindow(dpy, root, c->ux, c->uy + config.border_width - config.title_height,
	                    c->uw + config.border_width * 2, config.title_height, 0,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWEventMask | CWBackPixel, &swa);
	if (!config.ignoretitle)
		XMapWindow(dpy, ret);
	wins[0] = ret;
	wins[1] = c->win;
	XRestackWindows(dpy, wins, sizeof wins);
	return ret;
}
