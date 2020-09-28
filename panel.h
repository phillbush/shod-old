#ifndef _PANEL_H_
#define _PANEL_H_

/* get panel given a window */
struct Panel *getpanel(Window win);

/* adopt a panel (aka dock or bar) */
void panel_add(Window win);

/* delete a panel */
void panel_del(struct Panel *d);

#endif /* _PANEL_H_ */
