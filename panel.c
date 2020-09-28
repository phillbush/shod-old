#include <err.h>
#include <stdlib.h>
#include <X11/Xutil.h>
#include "shod.h"
#include "panel.h"
#include "monitor.h"
#include "util.h"

struct Panel *
getpanel(Window win)
{
	struct Panel *p;

	for (p = panels; p; p = p->next)
		if (p->win == win)
			return p;
	return NULL;
}

void
panel_add(Window win)
{
	unsigned long *values;
	struct Panel *p;

	if ((p = malloc(sizeof *p)) == NULL)
		err(1, "malloc");

	p->left = 0;
	p->right = 0;
	p->top = 0;
	p->bottom = 0;

	/* get the space the panel reserves for itself on screen edges */
	if ((values = getcardinalprop(win, netatom[NetWMStrut], 4)) != NULL) {
		p->left = values[0];
		p->right = values[1];
		p->top = values[2];
		p->bottom = values[3];
		XFree(values);
	}

	if (panels == NULL) {
		p->prev = NULL;
		panels = p;
	} else {
		struct Panel *lastpanel;

		for (lastpanel = panels; lastpanel->next; lastpanel = lastpanel->next)
			;
		p->prev = lastpanel;
		lastpanel->next = p;
	}
	p->next = NULL;

	p->win = win;

	XSelectInput(dpy, p->win, EnterWindowMask | StructureNotifyMask
	             | PropertyChangeMask | FocusChangeMask);
	XGrabButton(dpy, Button1, AnyModifier, win, False, ButtonPressMask,
	            GrabModeSync, GrabModeSync, None, None);

	XMapWindow(dpy, p->win);

	monitor_updatearea();
}

void
panel_del(struct Panel *p)
{
	if (p == NULL)
		return;

	if (p->prev)    /* p is not at the beginning of the list */
		p->prev->next = p->next;
	else            /* p is at the beginning of the list */
		panels = p->next;
	if (p->next)
		p->next->prev = p->prev;
	free(p);

	monitor_updatearea();
}
