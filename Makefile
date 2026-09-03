# cwm makefile for BSD make and GNU make
# uses pkg-config, DESTDIR and PREFIX

PROG=		cwm

PREFIX?=	/usr/local

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c confpath.c xevents.c group.c \
		kbfunc.c ribbon.c probe.c parse.y

OBJS=		calmwm.o screen.o xmalloc.o client.o menu.o \
		search.o util.o xutil.o conf.o confpath.o xevents.o group.o \
		kbfunc.o ribbon.o probe.o strlcpy.o strlcat.o parse.o \
		strtonum.o reallocarray.o

# Арифметика ленты.  Это ПЕЧАТЬ компилятора flang из библиотеки
# ribbon-flang/flang-ribbon, а не наш исходник; лежит в дереве затем, чтобы
# сборка из чистого клона по-прежнему требовала только C, yacc и три
# библиотеки X - довод целиком в ribbon-flang/README.md.  Пересоздаётся
# одной командой `make -C ribbon-flang emit`; правка руками - дефект, и
# `make -C ribbon-flang verify` его находит.
#
# Объектные файлы кладутся сюда, в корень, а не рядом с исходником: правило
# `.c.o` без `-o` пишет именно сюда, и одно явное правило на модуль дешевле,
# чем переписывать суффиксное правило под подкаталог в двух разных make.
FLANGDIR=	ribbon-flang/out-c
FLANGOBJS=	viewport.o geometry.o placement.o strut.o flang_runtime.o
FLANGHDRS=	${FLANGDIR}/viewport.h ${FLANGDIR}/geometry.h \
		${FLANGDIR}/placement.h ${FLANGDIR}/strut.h \
		${FLANGDIR}/flang_runtime.h

# Рантайм печати зовёт pthread_create (собственный стек под глубокую
# рекурсию) и fmod.  На macOS обе живут в libSystem, на Linux, NetBSD и
# FreeBSD - нет, поэтому они здесь названы.
FLANGLIBS=	-lm -lpthread
		
PKG_CONFIG?=	pkg-config

CPPFLAGS+=	`${PKG_CONFIG} --cflags x11 xft xrandr`

CFLAGS?=	-Wall -O2 -g -D_GNU_SOURCE

LDFLAGS+=	`${PKG_CONFIG} --libs x11 xft xrandr`

MANPREFIX?=	${PREFIX}/share/man

all: ${PROG}

# Every object sees the layout model through calmwm.h; without this a stale
# object silently disagrees with the rest about the size of struct conf.
${OBJS}: calmwm.h queue.h wsi.h confpath.h
ribbon.o: ${FLANGHDRS}

clean:
	rm -f ${OBJS} ${FLANGOBJS} ${PROG} parse.c

${PROG}: ${OBJS} ${FLANGOBJS}
	${CC} ${OBJS} ${FLANGOBJS} ${LDFLAGS} ${FLANGLIBS} -o ${PROG}

.c.o:
	${CC} -c ${CFLAGS} ${CPPFLAGS} $<

# Напечатанное собирается без ${CPPFLAGS}: заголовков X ему не нужно ни
# одного, и это не экономия, а утверждение - арифметика ленты про оконную
# систему не знает.  Заголовки она берёт из своего же каталога, через
# относительный #include, поэтому и -I здесь не стоит.
viewport.o: ${FLANGDIR}/viewport.c ${FLANGHDRS}
	${CC} -c ${CFLAGS} ${FLANGDIR}/viewport.c

geometry.o: ${FLANGDIR}/geometry.c ${FLANGHDRS}
	${CC} -c ${CFLAGS} ${FLANGDIR}/geometry.c

placement.o: ${FLANGDIR}/placement.c ${FLANGHDRS}
	${CC} -c ${CFLAGS} ${FLANGDIR}/placement.c

strut.o: ${FLANGDIR}/strut.c ${FLANGHDRS}
	${CC} -c ${CFLAGS} ${FLANGDIR}/strut.c

flang_runtime.o: ${FLANGDIR}/flang_runtime.c ${FLANGDIR}/flang_runtime.h
	${CC} -c ${CFLAGS} ${FLANGDIR}/flang_runtime.c

install: ${PROG}
	install -d ${DESTDIR}${PREFIX}/bin ${DESTDIR}${MANPREFIX}/man1 ${DESTDIR}${MANPREFIX}/man5
	install -m 755 cwm ${DESTDIR}${PREFIX}/bin
	install -m 644 cwm.1 ${DESTDIR}${MANPREFIX}/man1
	install -m 644 cwmrc.5 ${DESTDIR}${MANPREFIX}/man5

# Проверяемая половина слоя macOS. Отдельной целью, а не частью "all": сборка
# под X11 о ней ничего не знает и знать не должна - macos/** не входит ни в
# SRCS, ни в OBJS. Две проверки, потому что вопросов два и они разные:
# check.sh спрашивает, та же ли раскладка получается у ленты поверх порта, что
# у ленты поверх X11, и делает ли точка входа то, что обещает - запуск, чтение
# cwmrc, разбор клавиши (это проверяется здесь целиком, вплоть до линковки
# двоичного файла); stub-build.sh - согласованы ли два файла на Objective-C с
# тем, чем мы считаем Accessibility API и Carbon Event Manager (мака нет, и это
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
