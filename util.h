#ifndef _UTIL_H_
#define _UTIL_H_

/* return atom of atom property */
Atom getatomprop(Window win, Atom prop);

/* return array of cardinal property, we have to free it after using */
unsigned long *getcardinalprop(Window win, Atom atom, unsigned long size);

/* get which monitor a given coordinate is in */
struct Monitor *getmon(int x, int y);

/* get window's WM_STATE property */
long getstate(Window w);

#endif /* _UTIL_H_ */
