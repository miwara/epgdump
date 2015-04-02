PREFIX		= /usr/local
TARGETS		= epgdump
OBJ_TARGETS	= epgdump.o aribstr.o eit.o nit.o sdt.o bit.o ts.o tdt.o util.o
HEADDERDEPEND	= aribstr.h eit.h nit.h sdt.h bit.h ts.h tdt.h util.h

LANG=C
CC		= gcc
CFLAGS		= -std=c99 -O2 -Wall -g
LIBS		= 

.c.o:
	${CC} ${CFLAGS} -c $<

all: ${TARGETS}

${TARGETS}: ${OBJ_TARGETS}
	${CC} ${CFLAGS} ${OBJ_TARGETS} -o $@ ${LDFLAGS} ${LIBS}

${OBJ_TARGETS}: ${HEDDERDEPEND}

clean:
	rm -f core ${TARGETS} *.o

install:
	install -m 755 ${TARGETS} ${PREFIX}/bin
