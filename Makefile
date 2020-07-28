include config.mk

SRCS = ${PROG}.c init.c monitor.c workspace.c ewmh.c winlist.c client.c scan.c xevent.c
OBJS = ${SRCS:.c=.o}

all: ${PROG}

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${OBJS}: shod.h
shod.o: init.h monitor.h ewmh.h config.h xevent.h
init.o: init.h
scan.o: scan.h client.h
xevent.o: xevent.h client.h ewmh.h monitor.h workspace.h
monitor.o: monitor.h workspace.h client.h
client.o: client.h winlist.h ewmh.h
ewmh.o: ewmh.h winlist.h
workspace.o: workspace.h ewmh.h client.h
winlist.o: winlist.h

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
