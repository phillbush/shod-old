#ifndef _WINLIST_H_
#define _WINLIST_H_

struct Winlist {
	Window *list;
	size_t num;
	size_t max;
};

extern struct Winlist winlist;

/* append win to winlist.list */
void winlist_add(Window win);

/* delete win from winlist.list */
void winlist_del(Window win);

/* move win from winlis.list[i] to winlist.list[winlist.num - 1] */
void winlist_focus(Window win);

#endif /* _WINLIST_H_ */
