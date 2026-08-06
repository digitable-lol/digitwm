/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - клиент, который честно рассказывает, когда его перерисовали
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
 */

/*
 * Вопрос DGT-WM-07 - гасить ли окна, которые вьюпорт не показывает, - решается
 * числом: сколько времени проходит от прокрутки до готовой картинки, когда
 * окно въезжает обратно.  Измерять это снаружи нечем: снимок экрана говорит,
 * что нарисовано, но не когда клиент об этом узнал.  Поэтому меряет сам
 * клиент.
 *
 * Он рисует ровно то, что рисует обычное приложение при получении Expose:
 * заливку и сетку линий, объём которой задаётся ключом -w (по умолчанию 2000
 * отрезков - примерно страница текста в терминале).  Ни секунды сна, ни
 * подгонки: если бы клиент рисовал мгновенно, разница между двумя политиками
 * была бы только в круговых задержках протокола, а она не только в них.
 *
 * Печатает по строке на событие, со временем по CLOCK_REALTIME в
 * миллисекундах - тем же часам, по которым отмечает свои нажатия сценарий:
 *
 *   ready <ms> <window-id>
 *   map|unmap|configure|expose|drawn <ms> ...
 *
 * Сборка:
 *   cc -O2 -Wall -o tools/redraw-probe tools/redraw-probe.c \
 *      `pkg-config --cflags --libs x11`
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

static double
now_ms(void)
{
	struct timespec	 ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

int
main(int argc, char **argv)
{
	Display		*dpy;
	Window		 win;
	GC		 gc;
	XEvent		 ev;
	XClassHint	 hint;
	XSizeHints	 size;
	const char	*name = "probe";
	int		 screen, work = 2000, opt, i;
	unsigned long	 ink;

	while ((opt = getopt(argc, argv, "n:w:")) != -1) {
		switch (opt) {
		case 'n':
			name = optarg;
			break;
		case 'w':
			work = atoi(optarg);
			break;
		default:
			errx(1, "usage: redraw-probe [-n name] [-w segments]");
		}
	}

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "no display");

	screen = DefaultScreen(dpy);
	win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, 400, 300,
	    0, BlackPixel(dpy, screen), WhitePixel(dpy, screen));

	hint.res_name = (char *)name;
	hint.res_class = (char *)"digitwm-redraw-probe";
	XSetClassHint(dpy, win, &hint);
	XStoreName(dpy, win, name);

	(void)memset(&size, 0, sizeof(size));
	size.flags = PMinSize;
	size.min_width = 40;
	size.min_height = 40;
	XSetWMNormalHints(dpy, win, &size);

	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask |
	    VisibilityChangeMask);

	gc = XCreateGC(dpy, win, 0, NULL);
	ink = BlackPixel(dpy, screen);

	XMapWindow(dpy, win);
	XFlush(dpy);

	(void)printf("ready %.3f %lu\n", now_ms(), (unsigned long)win);
	(void)fflush(stdout);

	for (;;) {
		XNextEvent(dpy, &ev);

		switch (ev.type) {
		case Expose: {
			double	 t0 = now_ms();
			XWindowAttributes	 wa;

			(void)printf("expose %.3f %d %d %d %d count %d\n", t0,
			    ev.xexpose.x, ev.xexpose.y, ev.xexpose.width,
			    ev.xexpose.height, ev.xexpose.count);

			if (ev.xexpose.count > 0)
				break;

			/*
			 * Рисуем только на последнем Expose пачки - так же, как
			 * это делает всякое приложение, которому не всё равно,
			 * сколько раз перерисовывать один кадр.
			 */
			XGetWindowAttributes(dpy, win, &wa);
			XSetForeground(dpy, gc, WhitePixel(dpy, screen));
			XFillRectangle(dpy, win, gc, 0, 0, wa.width, wa.height);
			XSetForeground(dpy, gc, ink);
			for (i = 0; i < work; i++) {
				int	 y = (i * 7) % (wa.height ? wa.height : 1);

				XDrawLine(dpy, win, gc, 0, y, wa.width, y);
			}
			/*
			 * XSync, а не XFlush: нужно время, когда сервер эту
			 * работу закончил, а не время, когда её отправили.
			 */
			XSync(dpy, False);
			(void)printf("drawn %.3f %.3f\n", now_ms(),
			    now_ms() - t0);
			break;
		}
		case MapNotify:
			(void)printf("map %.3f\n", now_ms());
			break;
		case UnmapNotify:
			(void)printf("unmap %.3f\n", now_ms());
			break;
		case VisibilityNotify:
			(void)printf("visibility %.3f %d\n", now_ms(),
			    ev.xvisibility.state);
			break;
		case ConfigureNotify:
			(void)printf("configure %.3f %d %d %d %d\n", now_ms(),
			    ev.xconfigure.x, ev.xconfigure.y,
			    ev.xconfigure.width, ev.xconfigure.height);
			break;
		case DestroyNotify:
			return 0;
		default:
			break;
		}
		(void)fflush(stdout);
	}

	return 0;
}
