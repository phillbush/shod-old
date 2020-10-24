#ifndef _DECOR_H_
#define _DECOR_H_

Window decor_createwin(struct Client *c);

void decor_borderadd(struct Client *c);
void decor_borderdel(struct Client *c);
void decor_drawfocus(struct Client *c);
void decor_drawunfocus(struct Client *c);
int decor_istitle(struct Client *c, int x, int y);
int decor_isborder(struct Client *c, int x, int y);

#endif /* _DECOR_H_ */
