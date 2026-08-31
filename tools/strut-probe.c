/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
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
 * Край выбирается ключом -e: top, bottom, left, right.  Все четыре, а не
 * два, потому что арифметика полосы симметрична по обеим осям, а замерен до
 * сих пор был только верхний край (DGT-WM-13, что осталось).  Боковая панель
 * заявляет отрезок ВЕРТИКАЛЬНОГО края и глубину по горизонтали - у неё
 * меняются местами и поля заявки, и стороны окна.
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

#define EDGE_LEFT	0
#define EDGE_RIGHT	1
#define EDGE_TOP	2
#define EDGE_BOTTOM	3

static Display	*dpy;
static Window	 win;
static Atom	 net_strut, net_strut_partial;
static int	 edge = EDGE_TOP, screen_w, screen_h;

/*
 * Полоса по спецификации: четыре глубины (левая, правая, верхняя, нижняя) и,
 * для каждой, отрезок края, на который она распространяется.  Панель во всю
 * ширину заявляет отрезок от 0 до ширины экрана минус один - оба конца
 * внутри заявки; боковая - от 0 до высоты минус один.
 *
 * Порядок полей задан спецификацией и переставлять его нельзя:
 *   0..3    left, right, top, bottom
 *   4..5    left_start_y, left_end_y
 *   6..7    right_start_y, right_end_y
 *   8..9    top_start_x, top_end_x
 *   10..11  bottom_start_x, bottom_end_x
 */
static void
set_strut(int depth)
{
	unsigned long	 s[12];

	memset(s, 0, sizeof(s));
	switch (edge) {
	case EDGE_LEFT:
		s[0] = depth;
		s[4] = 0;
		s[5] = screen_h - 1;
		break;
	case EDGE_RIGHT:
		s[1] = depth;
		s[6] = 0;
		s[7] = screen_h - 1;
		break;
	case EDGE_BOTTOM:
		s[3] = depth;
		s[10] = 0;
		s[11] = screen_w - 1;
		break;
	case EDGE_TOP:
	default:
		s[2] = depth;
		s[8] = 0;
		s[9] = screen_w - 1;
		break;
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
	int			 wx, wy, ww, wh;

	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-h") == 0) && ((i + 1) < argc))
			height = atoi(argv[++i]);
		else if (strcmp(argv[i], "-b") == 0)
			edge = EDGE_BOTTOM;	/* прежнее имя нижнего края */
		else if ((strcmp(argv[i], "-e") == 0) && ((i + 1) < argc)) {
			i++;
			if (strcmp(argv[i], "top") == 0)
				edge = EDGE_TOP;
			else if (strcmp(argv[i], "bottom") == 0)
				edge = EDGE_BOTTOM;
			else if (strcmp(argv[i], "left") == 0)
				edge = EDGE_LEFT;
			else if (strcmp(argv[i], "right") == 0)
				edge = EDGE_RIGHT;
			else {
				(void)fprintf(stderr,
				    "strut-probe: край - top, bottom, left "
				    "или right\n");
				return 1;
			}
		} else {
			(void)fprintf(stderr,
			    "usage: strut-probe [-h depth] "
			    "[-e top|bottom|left|right] [-b]\n");
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

	/*
	 * Панель по горизонтальному краю лежит поперёк экрана, по
	 * вертикальному - вдоль него; глубина всегда меряется внутрь экрана.
	 */
	switch (edge) {
	case EDGE_LEFT:
		wx = 0; wy = 0; ww = height; wh = screen_h;
		break;
	case EDGE_RIGHT:
		wx = screen_w - height; wy = 0; ww = height; wh = screen_h;
		break;
	case EDGE_BOTTOM:
		wx = 0; wy = screen_h - height; ww = screen_w; wh = height;
		break;
	case EDGE_TOP:
	default:
		wx = 0; wy = 0; ww = screen_w; wh = height;
		break;
	}

	win = XCreateWindow(dpy, DefaultRootWindow(dpy), wx, wy, ww, wh, 0,
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
