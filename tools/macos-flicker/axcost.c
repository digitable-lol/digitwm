/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - что стоит на macOS одна выдача геометрии и как поздно приходит
 * известие о новом окне
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
 * Перенос ленты на macOS упирается в одно место, которое там устроено иначе:
 * менеджер узнаёт о новом окне ПОСЛЕ того, как приложение его показало, а не
 * вместо системы, как на X11.  Значит окно сперва появится там, где решило
 * приложение, и лишь потом прыгнет в свою колонку.
 *
 * «Мелькание» - не оценка, а число, и оно раскладывается на три слагаемых:
 *
 *   1. сколько ждать известия  - от создания окна до kAXWindowCreated;
 *   2. сколько стоит выдача    - AXUIElementSetAttributeValue, круг в чужой
 *                                цикл событий;
 *   3. сколько таких выдач     - это уже наше, и это измерено на X11
 *                                (tools/measure-syncs.sh).
 *
 * Эта программа меряет первые два. Третье к маку отношения не имеет.
 *
 * Роли:
 *   axcost trusted            - есть ли право «Универсальный доступ»
 *   axcost cost <pid> [n]     - цена одной выдачи и одного чтения, n раз
 *   axcost watch <pid> [сек]  - ждать новых окон и мерить путь
 *                               «известие - чтение - выдача»
 *
 * Часы - CLOCK_MONOTONIC_RAW, общие для всех процессов машины: числа отсюда
 * складываются с числами помощника (flicker.m) без поправок.
 *
 * Сборка: см. Makefile рядом.  Собиралось это только против заглушки на
 * Linux (stub-build.sh) - мака у нас нет, и ни одна цифра здесь ещё не
 * получена.
 */

#include <ApplicationServices/ApplicationServices.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TRIALS_MAX 4096

static double
now_ms(void)
{
	struct timespec	 ts;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0);
}

static int
cmp_double(const void *a, const void *b)
{
	double	 x = *(const double *)a, y = *(const double *)b;

	return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/*
 * Печатать распределение, а не одно число: у обращения к чужому процессу
 * длинный хвост, и среднее его прячет.  doc/baseline.md держится того же
 * правила для X11.
 */
static void
report(const char *what, double *v, int n)
{
	double	 sum = 0.0;
	int	 i;

	if (n <= 0) {
		printf("  %-22s нет замеров\n", what);
		return;
	}
	qsort(v, n, sizeof(*v), cmp_double);
	for (i = 0; i < n; i++)
		sum += v[i];

	printf("  %-22s n=%d  мин %.2f  медиана %.2f  p95 %.2f  макс %.2f  "
	    "среднее %.2f (мс)\n", what, n, v[0], v[n / 2],
	    v[(int)(n * 0.95) >= n ? n - 1 : (int)(n * 0.95)], v[n - 1],
	    sum / n);
}

static CFStringRef
copy_string_attr(AXUIElementRef el, CFStringRef attr)
{
	CFTypeRef	 val = NULL;

	if (AXUIElementCopyAttributeValue(el, attr, &val) != kAXErrorSuccess)
		return NULL;
	if (val != NULL && CFGetTypeID(val) == CFStringGetTypeID())
		return (CFStringRef)val;
	if (val != NULL)
		CFRelease(val);
	return NULL;
}

static void
say_title(AXUIElementRef win)
{
	CFStringRef	 title;
	char		 buf[256];

	if ((title = copy_string_attr(win, kAXTitleAttribute)) == NULL) {
		printf("  окно без заголовка\n");
		return;
	}
	if (CFStringGetCString(title, buf, sizeof(buf), kCFStringEncodingUTF8))
		printf("  окно: %s\n", buf);
	CFRelease(title);
}

/* Первое окно приложения; для замера годится любое, лишь бы оно двигалось. */
static AXUIElementRef
copy_first_window(AXUIElementRef app)
{
	CFTypeRef	 wins = NULL;
	AXUIElementRef	 win;

	if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute,
	    &wins) != kAXErrorSuccess || wins == NULL)
		return NULL;
	if (CFGetTypeID(wins) != CFArrayGetTypeID() ||
	    CFArrayGetCount((CFArrayRef)wins) == 0) {
		CFRelease(wins);
		return NULL;
	}
	win = (AXUIElementRef)CFArrayGetValueAtIndex((CFArrayRef)wins, 0);
	CFRetain(win);
	CFRelease(wins);
	return win;
}

static int
get_point(AXUIElementRef win, CFStringRef attr, CGPoint *out)
{
	CFTypeRef	 val = NULL;
	Boolean		 ok;

	if (AXUIElementCopyAttributeValue(win, attr, &val) != kAXErrorSuccess ||
	    val == NULL)
		return 0;
	ok = AXValueGetValue((AXValueRef)val, kAXValueTypeCGPoint, out);
	CFRelease(val);
	return ok ? 1 : 0;
}

static int
set_point(AXUIElementRef win, CFStringRef attr, CGPoint p)
{
	AXValueRef	 val;
	AXError		 err;

	if ((val = AXValueCreate(kAXValueTypeCGPoint, &p)) == NULL)
		return 0;
	err = AXUIElementSetAttributeValue(win, attr, val);
	CFRelease(val);
	return (err == kAXErrorSuccess);
}

/*
 * Цена одного круга.  Позиция сдвигается на точку туда и обратно: окно
 * остаётся там же, где стояло, а работа проделывается настоящая - подделать
 * дешёвый путь, записав то же самое значение, приложение вправе (и некоторые
 * так и делают), и тогда число получилось бы не про то.
 */
static int
role_cost(pid_t pid, int n)
{
	AXUIElementRef	 app, win;
	CGPoint		 home, p;
	double		*set, *get, t0;
	int		 i, nset = 0, nget = 0, failed = 0;

	if ((app = AXUIElementCreateApplication(pid)) == NULL) {
		fprintf(stderr, "нет приложения с pid %d\n", (int)pid);
		return 1;
	}
	if ((win = copy_first_window(app)) == NULL) {
		fprintf(stderr, "у приложения %d нет окон, доступных AX "
		    "(или нет права «Универсальный доступ»)\n", (int)pid);
		CFRelease(app);
		return 1;
	}
	say_title(win);
	if (!get_point(win, kAXPositionAttribute, &home)) {
		fprintf(stderr, "AXPosition не читается\n");
		CFRelease(win);
		CFRelease(app);
		return 1;
	}
	printf("  исходная позиция: %.0f %.0f, замеров: %d\n", home.x, home.y,
	    n);

	set = calloc(n, sizeof(*set));
	get = calloc(n, sizeof(*get));
	if (set == NULL || get == NULL) {
		free(set);
		free(get);
		CFRelease(win);
		CFRelease(app);
		return 1;
	}

	for (i = 0; i < n; i++) {
		p = home;
		p.x += (i % 2) ? 1 : 0;

		t0 = now_ms();
		if (set_point(win, kAXPositionAttribute, p))
			set[nset++] = now_ms() - t0;
		else
			failed++;

		t0 = now_ms();
		if (get_point(win, kAXPositionAttribute, &p))
			get[nget++] = now_ms() - t0;
	}
	set_point(win, kAXPositionAttribute, home);

	report("выдача AXPosition", set, nset);
	report("чтение AXPosition", get, nget);
	if (failed)
		printf("  отказов на выдаче: %d\n", failed);

	free(set);
	free(get);
	CFRelease(win);
	CFRelease(app);
	return 0;
}

struct watch {
	int		 seen;
	int		 limit;
	double		*notify_to_read;
	double		*write;
	double		*total;
};

/*
 * Вот та самая точка, из-за которой перенос теряет обещание: обратного канала
 * здесь нет.  Callback объявлен void - отказать в показе окна или подставить
 * геометрию до показа нечем, окно уже на экране.  Всё, что можно измерить, -
 * насколько поздно мы об этом узнали и сколько ещё после этого потратим.
 */
static void
on_window(AXObserverRef obs, AXUIElementRef win, CFStringRef note, void *ctx)
{
	struct watch	*w = ctx;
	CGPoint		 p, want;
	double		 t_note, t_read, t_write;

	(void)obs;
	(void)note;

	t_note = now_ms();
	if (w->seen >= w->limit)
		return;

	if (!get_point(win, kAXPositionAttribute, &p)) {
		printf("окно %d: AXPosition не прочиталось\n", w->seen + 1);
		w->seen++;
		return;
	}
	t_read = now_ms();

	want = p;
	want.x += 400;

	if (!set_point(win, kAXPositionAttribute, want)) {
		printf("окно %d: AXPosition не записалось\n", w->seen + 1);
		w->seen++;
		return;
	}
	t_write = now_ms();

	w->notify_to_read[w->seen] = t_read - t_note;
	w->write[w->seen] = t_write - t_read;
	w->total[w->seen] = t_write - t_note;

	/*
	 * Абсолютное время печатается, чтобы помощник (flicker.m) мог сложить
	 * свою половину с этой: часы у них одни.
	 */
	printf("окно %d: известие %.3f, чтение %.2f мс, выдача %.2f мс, "
	    "было %.0f %.0f -> стало %.0f %.0f\n", w->seen + 1, t_note,
	    t_read - t_note, t_write - t_read, p.x, p.y, want.x, want.y);
	fflush(stdout);
	w->seen++;
}

static int
role_watch(pid_t pid, double seconds, int limit)
{
	AXUIElementRef	 app;
	AXObserverRef	 obs = NULL;
	struct watch	 w;

	memset(&w, 0, sizeof(w));
	if (limit > TRIALS_MAX)
		limit = TRIALS_MAX;
	w.limit = limit;
	w.notify_to_read = calloc(limit, sizeof(double));
	w.write = calloc(limit, sizeof(double));
	w.total = calloc(limit, sizeof(double));
	if (w.notify_to_read == NULL || w.write == NULL || w.total == NULL) {
		fprintf(stderr, "памяти нет\n");
		return 1;
	}

	if ((app = AXUIElementCreateApplication(pid)) == NULL) {
		fprintf(stderr, "нет приложения с pid %d\n", (int)pid);
		return 1;
	}
	if (AXObserverCreate(pid, on_window, &obs) != kAXErrorSuccess ||
	    obs == NULL) {
		fprintf(stderr, "AXObserverCreate отказал: нет права "
		    "«Универсальный доступ»?\n");
		CFRelease(app);
		return 1;
	}
	if (AXObserverAddNotification(obs, app, kAXWindowCreatedNotification,
	    &w) != kAXErrorSuccess) {
		fprintf(stderr, "на kAXWindowCreated подписаться не вышло\n");
		CFRelease(obs);
		CFRelease(app);
		return 1;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
	    AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);

	printf("жду новых окон приложения %d, %.0f с\n", (int)pid, seconds);
	fflush(stdout);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, 0);

	printf("\nразложение пути от известия до выдачи:\n");
	report("известие -> чтение", w.notify_to_read, w.seen);
	report("выдача AXPosition", w.write, w.seen);
	report("всего наша доля", w.total, w.seen);
	printf("\nЭто НЕ всё мелькание: сюда не входит время от появления окна\n"
	    "на экране до известия.  Его меряет помощник, роль helper в\n"
	    "flicker.m, своими часами - теми же.\n");

	CFRelease(obs);
	CFRelease(app);
	free(w.notify_to_read);
	free(w.write);
	free(w.total);
	return 0;
}

static int
role_trusted(void)
{
	const void	*keys[1];
	const void	*vals[1];
	CFDictionaryRef	 opts;
	Boolean		 ok;

	keys[0] = kAXTrustedCheckOptionPrompt;
	vals[0] = kCFBooleanTrue;
	opts = CFDictionaryCreate(NULL, keys, vals, 1,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	ok = AXIsProcessTrustedWithOptions(opts);
	CFRelease(opts);

	printf("право «Универсальный доступ»: %s\n", ok ? "есть" : "нет");
	return ok ? 0 : 1;
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: axcost trusted\n"
	    "       axcost cost <pid> [замеров]\n"
	    "       axcost watch <pid> [секунд] [окон]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc < 2)
		usage();

	if (strcmp(argv[1], "trusted") == 0)
		return role_trusted();
	if (strcmp(argv[1], "cost") == 0) {
		if (argc < 3)
			usage();
		return role_cost((pid_t)atoi(argv[2]),
		    argc > 3 ? atoi(argv[3]) : 200);
	}
	if (strcmp(argv[1], "watch") == 0) {
		if (argc < 3)
			usage();
		return role_watch((pid_t)atoi(argv[2]),
		    argc > 3 ? atof(argv[3]) : 30.0,
		    argc > 4 ? atoi(argv[4]) : 64);
	}
	usage();
	return 1;
}
