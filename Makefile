# cwm makefile for BSD make and GNU make
# uses pkg-config, DESTDIR and PREFIX

PROG=		cwm

PREFIX?=	/usr/local

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c xevents.c group.c \
		kbfunc.c ribbon.c probe.c parse.y

OBJS=		calmwm.o screen.o xmalloc.o client.o menu.o \
		search.o util.o xutil.o conf.o xevents.o group.o \
		kbfunc.o ribbon.o probe.o strlcpy.o strlcat.o parse.o \
		strtonum.o reallocarray.o
		
PKG_CONFIG?=	pkg-config

CPPFLAGS+=	`${PKG_CONFIG} --cflags x11 xft xrandr`

CFLAGS?=	-Wall -O2 -g -D_GNU_SOURCE

LDFLAGS+=	`${PKG_CONFIG} --libs x11 xft xrandr`

MANPREFIX?=	${PREFIX}/share/man

all: ${PROG}

# Every object sees the layout model through calmwm.h; without this a stale
# object silently disagrees with the rest about the size of struct conf.
${OBJS}: calmwm.h queue.h wsi.h

clean:
	rm -f ${OBJS} ${PROG} parse.c

${PROG}: ${OBJS}
	${CC} ${OBJS} ${LDFLAGS} -o ${PROG}

.c.o:
	${CC} -c ${CFLAGS} ${CPPFLAGS} $<

install: ${PROG}
	install -d ${DESTDIR}${PREFIX}/bin ${DESTDIR}${MANPREFIX}/man1 ${DESTDIR}${MANPREFIX}/man5
	install -m 755 cwm ${DESTDIR}${PREFIX}/bin
	install -m 644 cwm.1 ${DESTDIR}${MANPREFIX}/man1
	install -m 644 cwmrc.5 ${DESTDIR}${MANPREFIX}/man5

# Проверяемая половина слоя macOS. Отдельной целью, а не частью "all": сборка
# под X11 о ней ничего не знает и знать не должна - macos/** не входит ни в
# SRCS, ни в OBJS. Две проверки, потому что вопросов два и они разные:
# check.sh спрашивает, та же ли раскладка получается у ленты поверх порта, что
# у ленты поверх X11 (это проверяется здесь целиком); stub-build.sh - согласован
# ли слой Objective-C с тем, чем мы считаем Accessibility API (мака нет, и это
# всё, что без него проверяемо). Подробности - в шапках обоих скриптов.
macos-check: ${PROG}
	sh macos/check.sh
	sh macos/stub-build.sh

release:
	VERSION=$$(git describe --tags | sed 's/^v//;s/-[^.]*$$//') && \
	git archive --prefix=cwm-$$VERSION/ -o cwm-$$VERSION.tar.gz HEAD

sign:
	VERSION=$$(git describe --tags | sed 's/^v//;s/-[^.]*$$//') && \
	gpg2 --armor --detach-sign cwm-$$VERSION.tar.gz && \
	signify -S -s ~/.signify/cwm.sec -m cwm-$$VERSION.tar.gz && \
	sed -i '1cuntrusted comment: verify with cwm.pub' cwm-$$VERSION.tar.gz.sig

.PRECIOUS: parse.c
