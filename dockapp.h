#ifndef _DOCKAPP_H_
#define _DOCKAPP_H_

/* get dockapp given a window */
struct Dockapp *getdockapp(Window win);

/* add dockapp win, whose XWMHints are wmhints */
void dockapp_add(Window win, XWindowAttributes *wa);

/* delete dockapp d */
void dockapp_del(struct Dockapp *d);

/* rearrange the dockapps on the dock */
void dockapp_redock(void);

#endif /* _DOCKAPP_H_ */
