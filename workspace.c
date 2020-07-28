#include <err.h>
#include <stdlib.h>
#include "shod.h"
#include "workspace.h"
#include "client.h"

/* allocate a new workspace */
static struct WS *
ws_alloc(void)
{
	struct WS *ws;

	if ((ws = malloc(sizeof *ws)) == NULL)
		err(1, "malloc");

	ws->prev = NULL;
	ws->next = NULL;
	ws->col = NULL;
	ws->floating = NULL;
	ws->focused = NULL;
	ws->nclients = 0;

	return ws;
}

/* add a workspace */
void
ws_add(struct Monitor *mon)
{
	struct WS *ws, *lastws;

	ws = ws_alloc();
	wscount++;

	for (lastws = mon->ws; lastws && lastws->next; lastws = lastws->next)
		;

	if (lastws == NULL) {
		mon->ws = ws;
	} else {
		lastws->next = ws;
		ws->prev = lastws;
	}

	ws->mon = mon;
}

/* delete a workspace */
void
ws_del(struct WS *ws)
{
	if (ws == NULL)
		return;

	if (ws->prev)
		ws->prev->next = ws->next;
	else
		ws->mon->ws = ws->next;
	if (ws->next)
		ws->next->prev = ws->prev;

	wscount--;

	free(ws);
}

/* get a workspace from a desktop index */
struct WS *
getws(long desk)
{
	struct Monitor *mon;
	struct WS *ws;

	if (desk < 0 || desk >= wscount)
		return NULL;

	for (mon = wm.mon; mon; mon = mon->next) {
		for (ws = mon->ws; ws; ws = ws->next) {
			if (desk == 0) {
				return ws;
			} else {
				desk--;
			}
		}
	}

	return NULL;
}

/* get a desktop index from a workspace */
int
getwsnum(struct WS *ws)
{
	struct Monitor *mon;
	struct WS *tmp;
	int desk = 0;

	for (mon = wm.mon; mon; mon = mon->next) {
		for (tmp = mon->ws; tmp; tmp = tmp->next) {
			if (ws == tmp) {
				goto done;
			} else {
				desk++;
			}
		}
	}

done:
	return desk;
}
