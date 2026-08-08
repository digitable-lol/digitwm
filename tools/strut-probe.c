/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - панель-пустышка для замера полосы, которую панель отнимает
 *
 * Copyright (c) 2026 Digitable <https://digitable.life>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Что панель отнимает у ленты, видно и без панели: важна не картинка на ней,
 * а два свойства окна - тип `_NET_WM_WINDOW_TYPE_DOCK` и полоса
 * `_NET_WM_STRUT_PARTIAL`.  Настоящая панель (polybar и любая другая) ставит
 * ровно их; этот клиент ставит только их и ничего больше, поэтому замер
 * говорит про оконный менеджер, а не про чужую программу.
 *
 * Команды читаются со стандартного ввода, по строке:
 *
 *   hide   свернуть - снять окно с экрана (так сворачивается polybar)
 *   show   развернуть обратно
 *   drop   оставить окно, но обнулить полосу (так сворачиваются другие)
 *   set N  запросить полосу в N точек, не снимая окна
 *   quit   выйти
 *
 * Печатает свой идентификатор окна первой строкой.
 *
 * Сборка:
 *   cc -O2 -Wall -o tools/strut-probe tools/strut-probe.c \
 *      `pkg-config --cflags --libs x11`
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Display	*dpy;
static Window	 win;
static Atom	 net_strut, net_strut_partial;
static int	 edge_bottom, screen_w, screen_h;

/*
 * Полоса по спецификации: четыре глубины и, для каждой, отрезок края, на
 * который она распространяется.  Панель во всю ширину заявляет отрезок от 0
 * до ширины экрана минус один - оба конца внутри заявки.
 */
static void
set_strut(int depth)
{
	unsigned long	 s[12];

	memset(s, 0, sizeof(s));
	if (edge_bottom) {
		s[3] = depth;
		s[10] = 0;
		s[11] = screen_w - 1;
	} else {
		s[2] = depth;
		s[8] = 0;
		s[9] = screen_w - 1;
	}
	XChangeProperty(dpy, win, net_strut_partial, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)s, 12);
	XChangeProperty(dpy, win, net_strut, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)s, 4);
}

int
main(int argc, char **argv)
{
	XSetWindowAttributes	 attr;
	Atom			 type, dock;
	char			 line[64];
	int			 i, height = 28;

	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-h") == 0) && ((i + 1) < argc))
			height = atoi(argv[++i]);
		else if (strcmp(argv[i], "-b") == 0)
			edge_bottom = 1;
		else {
			(void)fprintf(stderr,
			    "usage: strut-probe [-h height] [-b]\n");
			return 1;
		}
	}

	if ((dpy = XOpenDisplay(NULL)) == NULL) {
		(void)fprintf(stderr, "strut-probe: нет дисплея\n");
		return 1;
	}
	screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
	screen_h = DisplayHeight(dpy, DefaultScreen(dpy));

	attr.override_redirect = False;
	attr.background_pixel = BlackPixel(dpy, DefaultScreen(dpy));
	win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0,
	    edge_bottom ? (screen_h - height) : 0, screen_w, height, 0,
	    CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWBackPixel, &attr);

	type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	net_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
	net_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);

	XChangeProperty(dpy, win, type, XA_ATOM, 32, PropModeReplace,
	    (unsigned char *)&dock, 1);
	set_strut(height);
	XStoreName(dpy, win, "strut-probe");
	XMapWindow(dpy, win);
	XFlush(dpy);

	(void)printf("0x%lx\n", (unsigned long)win);
	(void)fflush(stdout);

	while (fgets(line, sizeof(line), stdin) != NULL) {
		if (strncmp(line, "hide", 4) == 0)
			XUnmapWindow(dpy, win);
		else if (strncmp(line, "show", 4) == 0) {
			set_strut(height);
			XMapWindow(dpy, win);
		} else if (strncmp(line, "drop", 4) == 0)
			set_strut(0);
		else if (strncmp(line, "set ", 4) == 0)
			set_strut(atoi(line + 4));
		else if (strncmp(line, "quit", 4) == 0)
			break;
		XFlush(dpy);
	}

	XCloseDisplay(dpy);
	return 0;
}
