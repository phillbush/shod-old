#include "shod.h"
#include "decor.h"
#include "util.h"

Window
decor_createwin(struct Client *c)
{
	XSetWindowAttributes swa;
	Window ret;

	swa.event_mask = EnterWindowMask | SubstructureNotifyMask
	               | SubstructureRedirectMask
	               | ButtonPressMask | FocusChangeMask;
	swa.background_pixel = config.focused;
	ret = XCreateWindow(dpy, root, c->ux, c->uy, c->uw, c->uh, 0,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWEventMask | CWBackPixel, &swa);
	XReparentWindow(dpy, c->win, ret, c->x, c->y);
	return ret;
}

/* add title and border to decoration */
void
decor_borderadd(struct Client *c)
{
	c->border = config.border_width;
	c->x = config.border_width;
	c->y = config.border_width;
	if (!config.ignoretitle)
		c->y += config.title_height;
}

/* remove title and border from decoration */
void
decor_borderdel(struct Client *c)
{
	c->border = 0;
	c->x = 0;
	c->y = 0;
}

/* draw unfocus decoration */
void
decor_drawfocus(struct Client *c)
{
	Pixmap pm;
	int w, h;

	getgeom(c, NULL, NULL, &w, &h);
	pm = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	XSetWindowBackground(dpy, c->dec, config.focused);
	XSetForeground(dpy, dc.gc, config.focused);
	XFillRectangle(dpy, pm, dc.gc, 0, 0, w, h);
	XCopyArea(dpy, pm, c->dec, dc.gc, 0, 0, w, h, 0, 0);
}

/* draw unfocus decoration */
void
decor_drawunfocus(struct Client *c)
{
	Pixmap pm;
	int w, h;

	getgeom(c, NULL, NULL, &w, &h);
	pm = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	XSetWindowBackground(dpy, c->dec, config.unfocused);
	XSetForeground(dpy, dc.gc, config.unfocused);
	XFillRectangle(dpy, pm, dc.gc, 0, 0, w, h);
	XCopyArea(dpy, pm, c->dec, dc.gc, 0, 0, w, h, 0, 0);
}

/* return nonzero if pointer is in title */
int
decor_istitle(struct Client *c, int x, int y)
{
	int w;

	if (config.ignoretitle)
		return 0;
	if (c->isfullscreen)
		return 0;
	getgeom(c, NULL, NULL, &w, NULL);
	if (x >= c->border && x <= w - c->border * 2 &&
	    y >= c->border && y <= c->border + config.title_height)
		return 1;
	return 0;
}

/* return nonzero if pointer is in border */
int
decor_isborder(struct Client *c, int x, int y)
{
	int w, h;

	if (c->isfullscreen)
		return 0;
	getgeom(c, NULL, NULL, &w, &h);
	if (x <= c->border || x >= w - c->border ||
	    y <= c->border || y >= h - c->border)
		return 1;
	return 0;
}
