#ifndef _XEVENT_H_
#define _XEVENT_H_


/* focus window when clicking on it, and activate moving/resizing */
static void xevent_buttonpress(XEvent *e);

/* interrupts moving/resizing action */
static void xevent_buttonrelease(XEvent *e);

/* handle client message event */
static void xevent_clientmessage(XEvent *e);

/* handle configure notify event */
static void xevent_configurenotify(XEvent *e);

/* handle configure request event */
static void xevent_configurerequest(XEvent *e);

/* forget about client */
static void xevent_destroynotify(XEvent *e);

/* focus window when cursor enter it, if fflag is set */
static void xevent_enternotify(XEvent *e);

static void xevent_focusin(XEvent *e);

/* handle map request event */
static void xevent_maprequest(XEvent *e);

/* run moving/resizing action */
static void xevent_motionnotify(XEvent *e);

/* forget about client */
static void xevent_unmapnotify(XEvent *e);

#endif /* _XEVENT_H_ */
