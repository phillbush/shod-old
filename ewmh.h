#ifndef _EWMH_H_
#define _EWMH_H_

/* sets the properties all ewmh compliant window manager should set */
void ewmh_init(void);

/* set actions allowed to operate upon a window */
void ewmh_setallowedactions(Window win);

/* set the active window */
void ewmh_setactivewindow(Window w);

/* set the number of desktops */
void ewmh_setnumberofdesktops(void);

/* set the current desktop, it is between 0 and the number of desktops - 1 */
void ewmh_setcurrentdesktop(int wsnum);

/* send the size of the borders when a client requests it */
void ewmh_setframeextents(struct Client *c);

/* set the state of the window manager (whether it is showing the desktop) */
void ewmh_setshowingdesktop(int n);

/* set the states of a client */
void ewmh_setstate(struct Client *c);

void ewmh_setwmdesktop(void);

void ewmh_setworkarea(void);

void ewmh_setclients(void);

void ewmh_setclientsstacking(void);

#endif /* _EWMH_H_ */
