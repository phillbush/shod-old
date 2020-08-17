#ifndef _WORKSPACE_H_
#define _WORKSPACE_H_

int getwsnum(struct WS *ws);
struct WS *getws(long desk);
void ws_add(struct Monitor *mon);
void ws_del(struct WS *ws);
int ws_isvisible(struct WS *ws);

#endif /* _WORKSPACE_H_ */
