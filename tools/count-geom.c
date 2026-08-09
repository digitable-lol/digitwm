/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - счётчик выдач геометрии: сколько раз менеджер трогает чужое окно
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
 * На X11 выдача геометрии почти ничего не стоит: XMoveResizeWindow кладётся в
 * буфер и уходит к серверу пачкой, ответа никто не ждёт.  Поэтому в коде и нет
 * проверки «а изменилась ли геометрия» - её отсутствие ничем не пахнет.
 *
 * На macOS та же строка - AXUIElementSetAttributeValue, синхронный круг в
 * чужой процесс: запрос уходит в его цикл событий и возвращается, когда тот
 * соизволит ответить.  Значит число таких выдач за одну вставку - это не
 * мелочь учёта, а прямая цена переноса, и её надо знать до переноса.
 *
 * Считать её изнутри менеджера нечестно: собственный счётчик считает то, что
 * автор счётчика имел в виду.  Эта заглушка считает то, что действительно ушло
 * в libX11 - она подставляется под LD_PRELOAD и печатает строку на каждый
 * вызов, который на macOS станет кругом IPC:
 *
 *   <мс> <вызов> <окно> [x y w h]
 *
 * Вес каждого вызова в кругах AX подписан в столбце ax= - XMoveResizeWindow
 * это две записи (AXPosition и AXSize), XMoveWindow одна, а XSendEvent с
 * синтетическим ConfigureNotify на macOS не имеет соответствия вовсе и стоит
 * ноль.
 *
 * Сборка:
 *   cc -shared -fPIC -O2 -o tools/count-geom.so tools/count-geom.c -ldl
 * Запуск:
 *   DIGITWM_GEOM_LOG=/путь/лог LD_PRELOAD=tools/count-geom.so ./cwm
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>

static FILE	*logf;

static void
geom_open(void)
{
	const char	*path;

	if (logf != NULL)
		return;
	if ((path = getenv("DIGITWM_GEOM_LOG")) == NULL)
		path = "/dev/null";
	if ((logf = fopen(path, "a")) == NULL)
		logf = stderr;
	setvbuf(logf, NULL, _IOLBF, 0);
}

static double
geom_now(void)
{
	struct timespec	 ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return (ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0);
}

static void
geom_say(const char *call, int ax, Window w, const char *fmt, ...)
{
	va_list	 ap;

	geom_open();
	fprintf(logf, "%.1f %s ax=%d 0x%lx", geom_now(), call, ax, w);
	if (fmt != NULL) {
		fputc(' ', logf);
		va_start(ap, fmt);
		vfprintf(logf, fmt, ap);
		va_end(ap);
	}
	fputc('\n', logf);
}

/*
 * Настоящая функция берётся из libX11 один раз и лениво: заглушка грузится
 * раньше, чем менеджер соединится с сервером, и dlsym в конструкторе стоил бы
 * порядка загрузки.
 */
#define REAL(name)							\
	static name##_fn real_##name;					\
	if (real_##name == NULL)					\
		real_##name = (name##_fn)dlsym(RTLD_NEXT, #name)

typedef int (*XMoveResizeWindow_fn)(Display *, Window, int, int, unsigned int,
    unsigned int);
typedef int (*XMoveWindow_fn)(Display *, Window, int, int);
typedef int (*XResizeWindow_fn)(Display *, Window, unsigned int, unsigned int);
typedef int (*XUnmapWindow_fn)(Display *, Window);
typedef int (*XMapWindow_fn)(Display *, Window);
typedef int (*XSetWindowBorderWidth_fn)(Display *, Window, unsigned int);
typedef int (*XSetWindowBorder_fn)(Display *, Window, unsigned long);
typedef Status (*XSendEvent_fn)(Display *, Window, Bool, long, XEvent *);

int
XMoveResizeWindow(Display *d, Window w, int x, int y, unsigned int width,
    unsigned int height)
{
	REAL(XMoveResizeWindow);

	geom_say("moveresize", 2, w, "%d %d %u %u", x, y, width, height);
	return real_XMoveResizeWindow(d, w, x, y, width, height);
}

int
XMoveWindow(Display *d, Window w, int x, int y)
{
	REAL(XMoveWindow);

	geom_say("move", 1, w, "%d %d", x, y);
	return real_XMoveWindow(d, w, x, y);
}

int
XResizeWindow(Display *d, Window w, unsigned int width, unsigned int height)
{
	REAL(XResizeWindow);

	geom_say("resize", 1, w, "%u %u", width, height);
	return real_XResizeWindow(d, w, width, height);
}

int
XUnmapWindow(Display *d, Window w)
{
	REAL(XUnmapWindow);

	geom_say("unmap", 1, w, NULL);
	return real_XUnmapWindow(d, w);
}

int
XMapWindow(Display *d, Window w)
{
	REAL(XMapWindow);

	geom_say("map", 1, w, NULL);
	return real_XMapWindow(d, w);
}

/*
 * Рамку вокруг чужого окна macOS рисовать не даёт вовсе, поэтому цена этих
 * двух там не «дорого», а «нечем»: они считаются с ax=0 и попадают в лог
 * только чтобы было видно, сколько работы отпадёт вместе с рамкой.
 */
int
XSetWindowBorderWidth(Display *d, Window w, unsigned int width)
{
	REAL(XSetWindowBorderWidth);

	geom_say("borderwidth", 0, w, "%u", width);
	return real_XSetWindowBorderWidth(d, w, width);
}

int
XSetWindowBorder(Display *d, Window w, unsigned long pixel)
{
	REAL(XSetWindowBorder);

	geom_say("border", 0, w, "%lu", pixel);
	return real_XSetWindowBorder(d, w, pixel);
}

/*
 * Синтетический ConfigureNotify - требование ICCCM, у macOS соответствия нет:
 * там приложение узнаёт о новой геометрии от системы само.  Ноль кругов, но в
 * логе виден, потому что на X11 он идёт парой к каждой выдаче.
 */
Status
XSendEvent(Display *d, Window w, Bool propagate, long mask, XEvent *ev)
{
	REAL(XSendEvent);

	if (ev != NULL && ev->type == ConfigureNotify)
		geom_say("config", 0, w, NULL);
	return real_XSendEvent(d, w, propagate, mask, ev);
}
