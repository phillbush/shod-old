#include <X11/Xutil.h>
#include "shod.h"
#include "manage.h"
#include "client.h"
#include "menu.h"
#include "panel.h"
#include "dockapp.h"
#include "desktop.h"
#include "util.h"

void
manage(Window win)
{
	XWindowAttributes wa;
	XWMHints *wmhints = NULL;
	Atom prop = None;

	if (!XGetWindowAttributes(dpy, win, &wa))
		return;
	if (wa.override_redirect)
		return;

	prop = getatomprop(win, netatom[NetWMWindowType]);
	wmhints = XGetWMHints(dpy, win);

	if (wmhints && (wmhints->flags & (IconWindowHint | StateHint)) &&
		wmhints->initial_state == WithdrawnState)       /* window is dockapp */
		dockapp_add(win, &wa);
	else if (prop == netatom[NetWMWindowTypeToolbar] ||
		prop == netatom[NetWMWindowTypeUtility] ||
		prop == netatom[NetWMWindowTypeMenu])           /* window is menu */
		menu_add(win);
	else if (prop == netatom[NetWMWindowTypeDesktop])   /* window is desktop app */
		desktop_add(win);
	else if (prop == netatom[NetWMWindowTypeDock])      /* window is panel */
		panel_add(win);
	else                                                /* window is regular window */
		client_add(win, &wa);
}

void
unmanage(Window win)
{
	struct Dockapp *d;
	struct Client *c;
	struct Panel *p;

	if ((c = getclient(win)) != NULL)
		client_del(c, 1, 1);
	else if ((p = getpanel(win)) != NULL)
		panel_del(p);
	else if ((d = getdockapp(win)) != NULL)
		dockapp_del(d);
}
