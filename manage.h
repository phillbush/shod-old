#ifndef _MANAGE_H_
#define _MANAGE_H_

/* make the wm manage a client, panel, dockapp, etc */
void manage(Window win);

/* make the wm forget about a client, panel, dockapp, etc */
void
unmanage(Window win);

#endif /* _MANAGE_H_ */
