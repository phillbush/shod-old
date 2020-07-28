#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "shod.h"
#include "winlist.h"

/* array of windows in focus order */
struct Winlist winlist = {NULL, 0, 0};

/* append win to winlist.list */
void
winlist_add(Window win)
{
	if (winlist.list == NULL) {                 /* first time */
		winlist.num = 0;
		winlist.max = 1;
		if ((winlist.list = reallocarray(winlist.list, 1, sizeof *winlist.list)) == NULL)
			err(1, "reallocarray");
	} else if (winlist.num >= winlist.max) {    /* grow */
		winlist.max *= 2;
		if ((winlist.list = reallocarray(winlist.list, winlist.max, sizeof *winlist.list)) == NULL)
			err(1, "reallocarray");
	}

	winlist.list[winlist.num++] = win;
}

/* delete win from winlist.list */
void
winlist_del(Window win)
{
	size_t i;

	if (winlist.num == 0)
		return;

	/* find winlist.list[i] == win*/
	for (i = 0; i < winlist.num; i++) {
		if (winlist.list[i] == win) {
			memmove(winlist.list+i, winlist.list+i+1, (winlist.num-i+1) * (sizeof *winlist.list));
			winlist.num--;
			return;
		}
	}
}

/* move win from winlis.list[i] to winlist.list[winlist.num - 1] */
void
winlist_focus(Window win)
{
	size_t i;

	if (winlist.num == 0)
		return;

	if (winlist.list[winlist.num - 1] == win)
		return;

	/* find winlist.list[i] == win */
	for (i = 0; i < winlist.num; i++) {
		if (winlist.list[i] == win) {
			memmove(winlist.list+i, winlist.list+i+1, (winlist.num-i+1) * (sizeof *winlist.list));
			winlist.list[winlist.num - 1] = win;
			return;
		}
	}
}
