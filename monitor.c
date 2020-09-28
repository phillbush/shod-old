#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xinerama.h>
#include "shod.h"
#include "monitor.h"
#include "client.h"
#include "dockapp.h"
#include "workspace.h"

static int
monitor_isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}

static struct Monitor *
monitor_alloc(int x, int y, int w, int h)
{
	struct Monitor *mon;

	if ((mon = malloc(sizeof *mon)) == NULL)
		err(1, "malloc");

	mon->prev = NULL;
	mon->next = NULL;
	mon->ws = NULL;
	mon->sticky = NULL;
	mon->selws = NULL;
	mon->selws = NULL;
	mon->dx = mon->mx = x;
	mon->dy = mon->my = y;
	mon->dw = mon->mw = w;
	mon->dh = mon->mh = h;

	mon->wx = x + config.gapleft;
	mon->wy = y + config.gaptop;
	mon->ww = w - (config.gapleft + config.gapright);
	mon->wh = h - (config.gaptop + config.gapbottom);

	return mon;
}

/* delete monitor and return list of clients on it with append appended */
struct Client *
monitor_del(struct Monitor *mon, struct Client *append)
{
	struct WS *ws;
	struct Column *col;
	struct Client *c;
	struct Client *head;

	if (mon == NULL)
		errx(1, "trying to delete null monitor");

	head = append;
	c = mon->sticky;
	while (c) {
		struct Client *tmp;

		tmp = c->next;
		c->next = head;
		head = c;
		c = tmp;
	}
	ws = mon->ws;
	while (ws) {
		struct WS *tmp;

		c = ws->floating;
		while (c) {
			struct Client *tmp;

			tmp = c->next;
			c->next = head;
			head = c;
			c = tmp;
		}

		col = ws->col;
		while (col) {
			struct Column *tmp;

			c = col->row;
			while (c) {
				struct Client *tmp;

				tmp = c->next;
				c->next = head;
				head = c;
				c = tmp;
			}

			tmp = col->next;
			free(col);
			col = tmp;
		}

		tmp = ws->next;
		ws_del(ws);
		ws = tmp;
	}

	if (mon->next)
		mon->next->prev = mon->prev;
	if (mon->prev)
		mon->prev->next = mon->next;
	if (wm.mon == mon)
		wm.mon = mon->next;

	free(mon);

	return head;
}

static void
monitor_add(XineramaScreenInfo *info)
{
	struct Monitor *lastmon;
	struct Monitor *mon;

	mon = monitor_alloc(info->x_org, info->y_org, info->width, info->height);

	ws_add(mon);

	mon->selws = mon->ws;

	for (lastmon = wm.mon; lastmon && lastmon->next; lastmon = lastmon->next)
		;

	if (lastmon == NULL)
		wm.mon = mon;
	else {
		lastmon->next = mon;
		mon->prev = lastmon;
	}
}

/* update the list of monitors in the wm structure */
void
monitor_update(void)
{
	struct Monitor *mon;
	struct Client *c;
	struct Client *cmove;   /* list of clients to be moved to selmon */
	int delselmon = 0;      /* whether current selmon was deleted */
	XineramaScreenInfo *info = NULL;
	XineramaScreenInfo *unique = NULL;
	int i, j, n;

	info = XineramaQueryScreens(dpy, &n);
	if ((unique = calloc(n, sizeof *unique)) == NULL)
		err(1, "calloc");

	/* only consider unique geometries as separate screens */
	for (i = 0, j = 0; i < n; i++)
		if (monitor_isuniquegeom(unique, j, &info[i]))
			memcpy(&unique[j++], &info[i], sizeof *unique);
	XFree(info);
	wm.moncount = j;

	/* look for monitors that do not exist anymore and delete them */
	mon = wm.mon;
	cmove = NULL;
	while (mon) {
		struct Monitor *tmp;
		int del;

		del = 1;
		for (i = 0; i < wm.moncount; i++) {
			if (unique[i].x_org == mon->mx && unique[i].y_org == mon->my &&
			    unique[i].width == mon->mw && unique[i].height == mon->mh) {
			    del = 0;
			    break;
			}
		}
		tmp = mon;
		mon = mon->next;
		if (del) {
			if (tmp == wm.selmon)
				delselmon = 1;
			cmove = monitor_del(tmp, cmove);
		}
	}

	/* look for new monitors and add them */
	for (i = 0; i < wm.moncount; i++) {
		int add = 1;
		for (mon = wm.mon; mon; mon = mon->next) {
			if (unique[i].x_org == mon->mx && unique[i].y_org == mon->my &&
				unique[i].width == mon->mw && unique[i].height == mon->mh) {
				add = 0;
				break;
			}
		}
		if (add)
			monitor_add(&unique[i]);
	}

	if (delselmon) {
		wm.selmon = wm.mon;
		wm.selmon->selws = wm.selmon->selws;
	}

	c = cmove;
	while (c) {
		struct Client *tmp;

		tmp = c->next;
		c->prev = c->next = NULL;
		c->col = NULL;
		c->state = ISNORMAL;
		c->layer = 0;
		client_sendtows(c, wm.selmon->selws, 1, 1, 0);
		c = tmp;
	}

	monitor_updatearea();

	dockapp_redock();

	free(unique);
}

/* update the gaps and usable area of the monitors */
void
monitor_updatearea(void)
{
	struct Monitor *mon;
	struct Panel *p;
	int left, right, top, bottom;
	int pleft, pright, ptop, pbottom;
	int dleft, dright, dtop, dbottom;

	dleft = dright = dtop = dbottom = 0;
	pleft = pright = ptop = pbottom = 0;

	for (p = panels; p; p = p->next) {
		if (p->left > pleft)
			pleft = p->left;
		if (p->right > pright)
			pright = p->right;
		if (p->top > ptop)
			ptop = p->top;
		if (p->bottom > pbottom)
			pbottom = p->bottom;
	}
	if (dock.mode != DockBelow) {
		switch (dock.position) {
		case DockTop:
			dtop = dock.size;
			break;
		case DockBottom:
			dbottom = dock.size;
			break;
		case DockLeft:
			dleft = dock.size;
			break;
		case DockRight:
			dright = dock.size;
			break;
		}
	}

	for (mon = wm.mon; mon; mon = mon->next) {
		mon->bl = pleft;
		mon->br = pright;
		mon->bt = ptop;
		mon->bb = pbottom;

		top = ptop + ((mon == wm.mon) ? dtop : 0);
		bottom = pbottom + ((mon == wm.mon) ? dbottom : 0);
		left = pleft + ((mon == wm.mon) ? dleft : 0);
		right = pright + ((mon == wm.mon) ? dright : 0);

		mon->wx = mon->mx + (left + config.gapleft);
		mon->ww = mon->mw - (left + config.gapleft) - (right + config.gapright);
		mon->wy = mon->my + (top + config.gaptop);
		mon->wh = mon->mh - (top + config.gaptop) - (bottom + config.gapbottom);

		mon->dx = mon->mx + left;
		mon->dw = mon->mw - left - right;
		mon->dy = mon->my + top;
		mon->dh = mon->mh - top - bottom;
	}

	if (wm.selmon && wm.selmon->selws)
		client_tile(wm.selmon->selws, 0);
	dockapp_redock();
}
