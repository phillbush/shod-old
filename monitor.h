#ifndef _MONITOR_H_
#define _MONITOR_H_

/* update list of monitors */
void monitor_update(void);
struct Client *monitor_del(struct Monitor *mon, struct Client *append);

#endif /* _MONITOR_H_ */
