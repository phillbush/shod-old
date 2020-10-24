include config.mk

SRCS = ${PROG}.c monitor.c workspace.c ewmh.c \
       manage.c client.c panel.c dockapp.c desktop.c \
       menu.c decor.c xevent.c winlist.c util.c
OBJS = ${SRCS:.c=.o}

all: ${PROG}

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${OBJS}: shod.h config.h
shod.o: monitor.h ewmh.h config.h xevent.h util.h manage.h
xevent.o: xevent.h client.h decor.h ewmh.h monitor.h workspace.h manage.h
manage.o: manage.h client.h panel.h dockapp.h desktop.h menu.h
panel.o: panel.h util.h monitor.h
dockapp.o: dockapp.h monitor.h
monitor.o: monitor.h workspace.h client.h dockapp.h
client.o: client.h winlist.h ewmh.h util.h decor.h
decor.o: decor.h
desktop.o: desktop.h
menu.o: menu.h
ewmh.o: ewmh.h winlist.h
workspace.o: workspace.h ewmh.h client.h
winlist.o: winlist.h
util.o: util.h

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm ${OBJS} ${PROG}

install: all
	install -D -m 755 ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}
	install -D -m 644 ${PROG}.1 ${DESTDIR}${MANPREFIX}/man1/${PROG}.1

uninstall:
	rm -f ${DESTDIR}/${PREFIX}/bin/${PROG}
	rm -f ${DESTDIR}/${MANPREFIX}/man1/${PROG}.1

.PHONY: all clean install uninstall
