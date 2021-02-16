#program name
PROG = shod

# paths
PREFIX = ../../usr/local
MANPREFIX = ${PREFIX}/man

LOCALINC = /usr/local/include
LOCALLIB = /usr/local/lib

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

FREETYPEINC = /usr/include/freetype2
# OpenBSD (uncomment)
FREETYPEINC = ${X11INC}/freetype2

# includes and libs
INCS = -I${LOCALINC} -I${X11INC} -I${FREETYPEINC}
LIBS = -L${LOCALLIB} -L${X11LIB} -lfontconfig -lXft -lX11 -lXinerama -lXpm

# flags
CPPFLAGS =
CFLAGS = -g -O0 -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
