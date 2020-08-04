#ifndef _CLIENT_H_
#define _CLIENT_H_

/* macros */
#define ISVISIBLE(c) (!((c)->state & ISMINIMIZED) && ((c)->state & ISSTICKY || (c)->ws == (c)->ws->mon->selws))

/* window states */
#define ISNORMAL     (1 << 0)
#define ISMAXIMIZED  (1 << 1)
#define ISSTICKY     (1 << 2)
#define ISMINIMIZED  (1 << 3)
#define ISFLOATING   (ISNORMAL | ISSTICKY)        /* Window isn't tiled nor fullscreen */
#define ISFREE       (ISSTICKY | ISMINIMIZED)     /* Window belongs to no workspace */
#define ISBOUND      (ISNORMAL | ISMAXIMIZED)     /* Window belongs to a workspace */

enum Quadrant {NW, NE, SW, SE};

/* get which monitor a given coordinate is in */
struct Monitor *getmon(int x, int y);

/* These functions get a client or dock from a given window */
struct Client *getclient(Window w);
struct Dock *getdock(Window win);

/* add or delete client/dock */
void dock_add(Window win);
void client_add(Window w, XWindowAttributes *wa);
void client_del(struct Client *c, int free, int delws);
void dock_del(struct Dock *d);

/* operations on clients */
void client_above(struct Client *c, int above);
void client_below(struct Client *c, int below);
struct Client *client_bestfocus(struct Client *c);
void client_close(struct Client *c);
void client_configure(struct Client *c, XWindowChanges wc, unsigned value);
void client_focus(struct Client *c);
void client_fullscreen(struct Client *c, int fullscreen);
void client_getgeom(struct Client *c, int *x_ret, int *y_ret, int *w_ret, int *h_ret);
void client_gotows(struct WS *ws, int wsnum);
void client_hide(struct Client *c, int hide);
int client_isborder(struct Client *c, int x, int y);
void client_maximize(struct Client *c, int maximize);
void client_minimize(struct Client *c, int minimize);
void client_move(struct Client *c, int x, int y);
void client_place(struct Client *c, struct WS *ws);
enum Quadrant client_quadrant(struct Client *c, int x, int y);
void client_raise(struct Client *c);
void client_resize(struct Client *c, enum Quadrant q, int x, int y);
void client_sendtows(struct Client *c, struct WS *ws, int new, int place, int move);
void client_setborder(struct Client *c, unsigned long color);
long client_setsizehints(struct Client *c);
void client_stick(struct Client *c, int stick);
void client_showdesktop(int n);
void client_tile(struct WS *ws, int recalc);
void client_unfocus(struct Client *c);

/* operations on docks */
void dock_updategaps(void);

#endif /* _CLIENT_H_ */
