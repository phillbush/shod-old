#ifndef _MONITOR_H_
#define _MONITOR_H_

/* update list of monitors */
void monitor_update(void);

/* delete monitor and return its list of clients */
struct Client *monitor_del(struct Monitor *mon, struct Client *append);

/* update the gaps and usable area of the monitors */
void monitor_updatearea(void);

#endif /* _MONITOR_H_ */
