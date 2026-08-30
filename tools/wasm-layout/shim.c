/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - лента в браузере: та же ribbon.c, механика вместо X11 - страница
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
 * Что здесь настоящее и что нет - это надо сказать до кода, потому что вся
 * затея держится на этой границе.
 *
 * НАСТОЯЩЕЕ: ribbon.c.  Тот же файл, что собирается в cwm, без единой правки,
 * собранный компилятором в WebAssembly.  Все решения - куда встанет окно,
 * какой ширины колонка, куда уедет вьюпорт, кто получит фокус после закрытия -
 * принимает он, а не JavaScript.  Что это возможно, доказано не здесь:
 * tools/no-x-build.sh показывает, что лента не берёт у X11 ни одного имени, а
 * весь её договор с оконной системой - одиннадцать операций, перечисленных в
 * wsi.h.  Этот файл и есть те одиннадцать, написанные заново для другой
 * механики: первая реализация контракта, которая не X11, и потому первое
 * свидетельство, что контракт вообще переносим.
 *
 * НЕ НАСТОЯЩЕЕ: окна.  В браузере нет ни приложений, ни X-сервера; окно здесь
 * - прямоугольник на странице.  Страница обязана говорить это прямо, иначе
 * получается подделка, а подделка запрещена: на /digitwm/ лежит запись живого
 * сеанса именно потому, что показанное там - настоящее.
 *
 * Итого честная формула: считает digitwm, рисует страница.  Это НЕ «оконный
 * менеджер в браузере» и не заменяет запись сеанса.
 *
 * Сборка: sh tools/wasm-layout/build.sh
 * Проверка тождества с двоичным файлом: node tools/wasm-layout/check.mjs
 */

#include <sys/types.h>
#include "queue.h"

#include <stdio.h>
#include <string.h>

#include "calmwm.h"

#define DGT_MAXWIN	256
#define DGT_HEAP	(768 * 1024)

#define DGT_EXPORT	__attribute__((visibility("default")))

/* Механика, которой на этой стороне занимается страница. */
extern void js_move(unsigned long id, int x, int y, int w, int h);
extern void js_hide(unsigned long id);
extern void js_show(unsigned long id);
extern void js_focus(unsigned long id);

struct conf		 Conf;

static struct screen_ctx	 dgt_sc;
static struct client_ctx	*dgt_win[DGT_MAXWIN];
static struct region_ctx	 dgt_region;
static int			 dgt_nwin;
static unsigned long		 dgt_nextid = 1;

/*
 * Куча - одна и на всё.  Освобождения нет: лента отдаёт память колонки при
 * закрытии окна, а страница живёт минуты, и добросовестный аллокатор был бы
 * здесь ответом на вопрос, которого никто не задавал.  Кончится - скажет
 * сама, а не тихо испортит соседа.
 */
static unsigned char	 dgt_heap[DGT_HEAP];
static unsigned long	 dgt_heap_used;
static int		 dgt_heap_full;

void *
xcalloc(size_t nmemb, size_t size)
{
	unsigned long	 need = (unsigned long)nmemb * size;
	void		*p;

	need = (need + 15) & ~15UL;
	if (dgt_heap_used + need > sizeof(dgt_heap)) {
		dgt_heap_full = 1;
		return &dgt_heap[0];
	}
	p = &dgt_heap[dgt_heap_used];
	dgt_heap_used += need;
	memset(p, 0, need);
	return p;
}

void *
xmalloc(size_t size)
{
	return xcalloc(1, size);
}

void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	(void)ptr;
	return xcalloc(nmemb, size);
}

char *
xstrdup(const char *s)
{
	size_t	 n = strlen(s) + 1;
	char	*p = xcalloc(1, n);

	memcpy(p, s, n);
	return p;
}

void
free(void *p)
{
	(void)p;
}

/* Свои, потому что libc здесь нет вовсе: -nostdlib. */
void *
memset(void *dst, int c, size_t n)
{
	unsigned char	*d = dst;

	while (n-- > 0)
		*d++ = (unsigned char)c;
	return dst;
}

void *
memcpy(void *dst, const void *src, size_t n)
{
	unsigned char		*d = dst;
	const unsigned char	*s = src;

	while (n-- > 0)
		*d++ = *s++;
	return dst;
}

size_t
strlen(const char *s)
{
	const char	*p = s;

	while (*p != '\0')
		p++;
	return (size_t)(p - s);
}

int
strcmp(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}
	return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

/*
 * Десять функций механики - весь договор ленты с внешним миром.  Здесь они
 * ведут в страницу; в cwm - в Xlib.  Список не выдуман: он ровно тот, что
 * печатает `nm -u ribbon.o`.
 */
void
client_resize(struct client_ctx *cc, int reset)
{
	(void)reset;
	client_geom_sent(cc);
	js_move(cc->win, cc->geom.x, cc->geom.y, cc->geom.w, cc->geom.h);
}

void
client_geom_sent(struct client_ctx *cc)
{
	cc->sentgeom = cc->geom;
	cc->flags |= CLIENT_GEOM_SENT;
}

int
client_geom_current(struct client_ctx *cc)
{
	if (!(cc->flags & CLIENT_GEOM_SENT))
		return 0;

	return (cc->geom.x == cc->sentgeom.x && cc->geom.y == cc->sentgeom.y &&
	    cc->geom.w == cc->sentgeom.w && cc->geom.h == cc->sentgeom.h);
}

void
client_hide(struct client_ctx *cc)
{
	cc->flags |= CLIENT_HIDDEN;
	js_hide(cc->win);
}

void
client_show(struct client_ctx *cc)
{
	cc->flags &= ~CLIENT_HIDDEN;
	js_show(cc->win);
}

void
client_set_active(struct client_ctx *cc)
{
	struct client_ctx	*ci;

	TAILQ_FOREACH(ci, &dgt_sc.clientq, entry)
		ci->flags &= ~CLIENT_ACTIVE;
	if (cc == NULL)
		return;
	cc->flags |= CLIENT_ACTIVE;
	js_focus(cc->win);
}

struct client_ctx *
client_current(struct screen_ctx *sc)
{
	struct client_ctx	*cc;

	TAILQ_FOREACH(cc, &dgt_sc.clientq, entry)
		if (cc->flags & CLIENT_ACTIVE)
			return cc;
	(void)sc;
	return NULL;
}

void
client_raise(struct client_ctx *cc)
{
	(void)cc;
}

void
client_ptr_save(struct client_ctx *cc)
{
	(void)cc;
}

void
client_ptr_warp(struct client_ctx *cc)
{
	(void)cc;
}

/* Выход один, страница им и является. */
struct region_ctx *
region_pointer(struct screen_ctx *sc)
{
	(void)sc;
	return &dgt_region;
}

/*
 * Гасить события пересечения указателя здесь нечего: их нет.  Обещание
 * wsi.h - «когда вызов вернулся, ни одна перемена фокуса, вызванная только
 * что разосланной геометрией, не идёт следом» - страница держит даром: она
 * не двигает фокус за указателем вовсе.  Это и есть законный способ
 * выполнить контракт ничем, и второй такой случай в wsi.h назван прямо.
 */
void
wsi_settle(void)
{
}

static struct ribbon *
dgt_ribbon(void)
{
	return TAILQ_FIRST(&dgt_sc.ribbonq);
}

static void
dgt_conf(int gap, int minw, int minh, int hide, int w0, int w1, int w2, int w3)
{
	Conf.ribbon = 1;
	Conf.ribbonhide = hide;
	Conf.ribbongap = gap;
	Conf.ribbonminw = minw;
	Conf.ribbonminh = minh;
	Conf.ribbonwidth[0] = w0;
	Conf.ribbonwidth[1] = w1;
	Conf.ribbonwidth[2] = w2;
	Conf.ribbonwidth[3] = w3;
}

DGT_EXPORT int
dgt_init(int vw, int vh, int gap, int minw, int minh, int hide,
    int w0, int w1, int w2, int w3)
{
	struct ribbon	*rb;

	dgt_heap_used = 0;
	dgt_heap_full = 0;
	dgt_nwin = 0;
	dgt_nextid = 1;
	memset(&dgt_sc, 0, sizeof(dgt_sc));
	memset(&dgt_region, 0, sizeof(dgt_region));
	memset(dgt_win, 0, sizeof(dgt_win));

	dgt_conf(gap, minw, minh, hide, w0, w1, w2, w3);

	TAILQ_INIT(&dgt_sc.clientq);
	TAILQ_INIT(&dgt_sc.regionq);
	ribbon_screen_init(&dgt_sc);

	dgt_sc.view.w = vw;
	dgt_sc.view.h = vh;
	dgt_sc.work = dgt_sc.view;
	dgt_region.view = dgt_sc.view;
	dgt_region.work = dgt_sc.view;
	dgt_region.name = xstrdup("web");
	TAILQ_INSERT_TAIL(&dgt_sc.regionq, &dgt_region, entry);

	rb = ribbon_new(&dgt_sc, "web");
	rb->view = dgt_sc.view;
	rb->active = 1;
	return 1;
}

/*
 * Новое окно - тем же путём, каким его заводит обработчик MapRequest:
 * ribbon_client_insert() решает, куда оно встанет, ribbon_sync() двигает
 * соседей.  Порядок тот же, что в client.c: сперва сдвинуть соседей, потом
 * показать новичка, иначе он накроет соседа и тот перерисуется.
 */
DGT_EXPORT int
dgt_open(int preset, int bwidth)
{
	struct client_ctx	*cc;

	if (dgt_nwin >= DGT_MAXWIN || dgt_heap_full)
		return -1;

	cc = xcalloc(1, sizeof(*cc));
	cc->sc = &dgt_sc;
	cc->win = dgt_nextid++;
	cc->bwidth = bwidth;
	cc->geom.w = dgt_sc.view.w / 2;
	cc->geom.h = dgt_sc.view.h / 2;
	TAILQ_INSERT_TAIL(&dgt_sc.clientq, cc, entry);
	dgt_win[dgt_nwin++] = cc;

	ribbon_client_insert(cc);
	if (preset >= 0 && preset < RIBBON_NPRESET && cc->rbcol != NULL)
		cc->rbcol->preset = preset;
	ribbon_sync(&dgt_sc);
	client_set_active(cc);
	ribbon_client_focus(cc);
	ribbon_sync(&dgt_sc);
	return (int)cc->win;
}

static struct client_ctx *
dgt_find(int id)
{
	struct client_ctx	*cc;

	TAILQ_FOREACH(cc, &dgt_sc.clientq, entry)
		if ((int)cc->win == id)
			return cc;
	return NULL;
}

DGT_EXPORT int
dgt_close(int id)
{
	struct client_ctx	*cc = dgt_find(id);

	if (cc == NULL)
		return 0;
	ribbon_client_remove(cc);
	TAILQ_REMOVE(&dgt_sc.clientq, cc, entry);
	ribbon_sync(&dgt_sc);
	return 1;
}

DGT_EXPORT void
dgt_focus_col(int delta)
{
	ribbon_focus_col(&dgt_sc, delta);
}

DGT_EXPORT void
dgt_focus_win(int delta)
{
	ribbon_focus_win(&dgt_sc, delta);
}

DGT_EXPORT void
dgt_move_client(int id, int delta)
{
	struct client_ctx	*cc = dgt_find(id);

	if (cc != NULL)
		ribbon_move_client(cc, delta);
}

DGT_EXPORT void
dgt_width(int id, int preset)
{
	struct client_ctx	*cc = dgt_find(id);

	if (cc != NULL)
		ribbon_width(cc, preset);
}

DGT_EXPORT void
dgt_center(void)
{
	ribbon_center(&dgt_sc);
}

DGT_EXPORT void
dgt_view(int vw, int vh)
{
	struct ribbon	*rb = dgt_ribbon();

	dgt_sc.view.w = vw;
	dgt_sc.view.h = vh;
	dgt_sc.work = dgt_sc.view;
	dgt_region.view = dgt_sc.view;
	dgt_region.work = dgt_sc.view;
	if (rb == NULL)
		return;
	rb->view.w = vw;
	rb->view.h = vh;
	ribbon_scroll(rb);
	ribbon_sync(&dgt_sc);
}

/*
 * Вторая роль модуля: отвечать ровно то же, что печатает `cwm -C layout-probe
 * layout ...`, и теми же вызовами в том же порядке (probe.c:542).  На это
 * опирается check.mjs: если два ответа разошлись хоть в одном числе, сборка в
 * WebAssembly - не та же лента, и говорить, что в браузере считает digitwm,
 * будет нельзя.
 */
static int dgt_out[DGT_MAXWIN * 10];
static int dgt_in[DGT_MAXWIN * 2];

DGT_EXPORT int *
dgt_out_ptr(void)
{
	return dgt_out;
}

/* Куда страница кладёт списки колонок и пресетов перед вызовом dgt_probe. */
DGT_EXPORT int *
dgt_in_ptr(void)
{
	return dgt_in;
}

DGT_EXPORT int
dgt_probe(int vw, int vh, int gap, int minw, int minh, int border,
    int ncol, int *cols, int *presets, int focus, int offset, int voffset,
    int w0, int w1, int w2, int w3)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 i, j, n = 0;

	dgt_init(vw, vh, gap, minw, minh, 0, w0, w1, w2, w3);
	rb = dgt_ribbon();

	for (i = 0; i < ncol; i++) {
		col = ribbon_col_new(rb, NULL);
		col->preset = presets[i];
		for (j = 0; j < cols[i]; j++) {
			cc = xcalloc(1, sizeof(*cc));
			cc->sc = &dgt_sc;
			cc->bwidth = border;
			cc->win = dgt_nextid++;
			ribbon_col_add(col, cc);
		}
	}

	rb->focus = ribbon_col_at(rb, focus);
	rb->offset = offset;
	rb->voffset = voffset;

	ribbon_scroll(rb);

	i = 0;
	TAILQ_FOREACH(col, &rb->colq, entry) {
		j = 0;
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			if (n >= DGT_MAXWIN)
				break;
			dgt_out[n * 10 + 0] = i;
			dgt_out[n * 10 + 1] = j;
			dgt_out[n * 10 + 2] = cc->rbgeom.x;
			dgt_out[n * 10 + 3] = cc->rbgeom.y;
			dgt_out[n * 10 + 4] = cc->rbgeom.w;
			dgt_out[n * 10 + 5] = cc->rbgeom.h;
			dgt_out[n * 10 + 6] = cc->geom.x;
			dgt_out[n * 10 + 7] = cc->geom.y;
			dgt_out[n * 10 + 8] = cc->geom.w;
			dgt_out[n * 10 + 9] = cc->geom.h;
			n++;
			j++;
		}
		i++;
	}
	return n;
}

DGT_EXPORT int
dgt_offset(void)
{
	struct ribbon	*rb = dgt_ribbon();

	return (rb == NULL) ? 0 : rb->offset;
}

DGT_EXPORT int
dgt_len(void)
{
	struct ribbon	*rb = dgt_ribbon();

	return (rb == NULL) ? 0 : rb->len;
}

DGT_EXPORT int
dgt_heap_left(void)
{
	return (int)(sizeof(dgt_heap) - dgt_heap_used);
}
