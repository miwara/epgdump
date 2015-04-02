PREFIX		= /usr/local
TARGETS		= epgdump hasnextevent waiteventstart
OBJ_TARGETS	= aribstr.o eit.o nit.o sdt.o bit.o ts.o tdt.o util.o
HEADDERDEPEND	= aribstr.h eit.h nit.h sdt.h bit.h ts.h tdt.h util.h

LANG=C
CC		= gcc
CFLAGS		= -std=c99 -O2 -Wall -g
LIBS		= 

.c.o:
	${CC} ${CFLAGS} -c $<

all: ${TARGETS}

epgdump: ${OBJ_TARGETS} epgdump.o
	${CC} ${CFLAGS} ${OBJ_TARGETS} epgdump.o -o $@ ${LDFLAGS} ${LIBS}

hasnextevent: ${OBJ_TARGETS} hasnextevent.o
	${CC} ${CFLAGS} ${OBJ_TARGETS} hasnextevent.o -o $@ ${LDFLAGS} ${LIBS}

waiteventstart: ${OBJ_TARGETS} waiteventstart.o
	${CC} ${CFLAGS} ${OBJ_TARGETS} waiteventstart.o -o $@ ${LDFLAGS} ${LIBS}

${OBJ_TARGETS}: ${HEDDERDEPEND}

clean:
	rm -f core ${TARGETS} *.o

install:
	install -m 755 ${TARGETS} ${PREFIX}/bin
