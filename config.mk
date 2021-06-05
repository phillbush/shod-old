#program name
PROG = shod

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

LOCALINC = /usr/local/include
LOCALLIB = /usr/local/lib

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I${LOCALINC} -I${X11INC}
LIBS = -L${LOCALLIB} -L${X11LIB} -lX11 -lXinerama -lXpm

# flags
CFLAGS = -g -O0 -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
