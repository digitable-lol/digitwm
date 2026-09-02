/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - прогнать точку входа macOS-порта там, где macOS нет
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
 * macos/wsicheck.c доказывает, что ЛЕНТА поверх порта считает то же, что лента
 * поверх X11. Это не то же самое, что «программу можно запустить»: между лентой
 * и человеком стоят ещё три вещи, которых в том харнессе нет вовсе -
 * последовательность запуска, чтение cwmrc и разбор нажатой клавиши. Каждая из
 * них до сих пор существовала бы только внутри main(), то есть не была бы
 * проверена ничем.
 *
 * Этот харнесс запускает ИМЕННО ИХ - macos/wsi_run.c и macos/wsi_conf.c целиком,
 * дословно те же файлы, что собираются в двоичный файл на маке, - поверх
 * оконной системы из памяти (macos/wsi_fake.c). Не проверяется ровно одно:
 * что под ними macOS отвечает правдиво. Это же не проверяет и macos/check.sh,
 * и по той же причине - мака нет.
 *
 * Клавиатурный слой здесь свой: настоящий (macos/wsi_key.m) - это Carbon,
 * которого на Linux нет. Подделка реализует тот же macos/wsi_key.h и делает
 * одну вещь сверх записи привязок: ОДНУ комбинацию она отвергает, потому что
 * настоящая система отвергает занятые кем-то другим, и путь «отказ назван
 * вслух» обязан быть пройден хоть раз.
 *
 * Запуск:  ./macos/runcheck
 * Выход:   0 - точка входа делает то, что обещает.
 */

#include <sys/types.h>
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"
#include "wsi_core.h"
#include "wsi_conf.h"
#include "wsi_fake.h"
#include "wsi_key.h"
#include "wsi_run.h"

#define MAXWIN	32

static int		 failures;
static wsip_window	 nextid = 1;
static int		 nwin;
static struct client_ctx	*win[MAXWIN];

static unsigned long	 seed = 20260901UL;

static int
rnd(int n)
{
	seed = (seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
	return (int)(seed % (unsigned long)n);
}

/*
 * Память здесь берётся из настоящего xmalloc.c дерева, а не из копии внутри
 * харнесса: набор объектных файлов, который собирается ниже, обязан совпадать
 * с тем, что собирает macos/Makefile на маке, иначе «собралось здесь» не
 * говорит ничего о «соберётся там».
 */

/*
 * macos/wsi_key.h, подделкой. Отвергается ровно одна комбинация - та, что
 * привязана к ribbon-center, - и отвергается по имени клавиши, а не по номеру:
 * так виден именно тот путь, которым настоящая система сообщает «эту
 * комбинацию уже держит кто-то другой».
 */
static int		 key_opened;
static int		 key_taken[WSICONF_MAXBIND];

int
wsik_open(void)
{
	key_opened = 1;
	return 0;
}

int
wsik_bind(int id, unsigned int mods, const char *key)
{
	if (!key_opened || id < 0 || id >= WSICONF_MAXBIND)
		return -1;
	if (mods == (WSIK_CONTROL | WSIK_OPTION) && strcmp(key, "c") == 0)
		return -1;
	key_taken[id] = 1;
	return 0;
}

void
wsik_close(void)
{
	key_opened = 0;
	(void)memset(key_taken, 0, sizeof(key_taken));
}

static void
label(const char *s, int width)
{
	int	 n = 0;

	while (*s != '\0') {
		if ((*s & 0xc0) != 0x80)
			n++;
		(void)fputc(*s++, stdout);
	}
	while (n++ < width)
		(void)fputc(' ', stdout);
}

static void
verdict(const char *what, int ok, const char *detail)
{
	(void)printf("   ");
	label(what, 40);
	(void)printf("%s", ok ? "да" : "НЕТ");
	if (detail != NULL)
		(void)printf("  (%s)", detail);
	(void)printf("\n");
	if (!ok)
		failures++;
}

/* Номер привязки, к которой привязана эта команда; -1, если такой нет. */
static int
bind_of(int cmd)
{
	const struct wsiconf_bind	*b;
	int				 i, n;

	b = wsiconf_binds(&n);
	for (i = 0; i < n; i++) {
		if (b[i].cmd == cmd)
			return i;
	}
	return -1;
}

static void
press(int cmd)
{
	int	 id = bind_of(cmd);

	if (id >= 0)
		wsi_note_key(id);
}

static struct client_ctx *
open_window(void)
{
	struct wsip_rect	 r;
	wsip_window		 id = nextid++;

	if (nwin >= MAXWIN)
		return NULL;

	r.x = 40 + rnd(200);
	r.y = 40 + rnd(200);
	r.w = 300 + rnd(300);
	r.h = 200 + rnd(200);
	fake_open(id, r.x, r.y, r.w, r.h);
	wsi_note_open(id, &r);

	if ((win[nwin] = wsi_lookup(id)) == NULL)
		return NULL;
	return win[nwin++];
}

/*
 * Сколько окон стоят не там, где велела модель. Ноль - единственный
 * допустимый ответ: всё, что лента решила, порт обязан был выдать наружу.
 */
static int
drift(void)
{
	struct wsip_rect	 r;
	int			 i, bad = 0;

	for (i = 0; i < nwin; i++) {
		if (fake_frame(win[i]->win, &r) != 0)
			continue;
		if (r.x != win[i]->geom.x || r.y != win[i]->geom.y ||
		    r.w != win[i]->geom.w || r.h != win[i]->geom.h)
			bad++;
	}
	return bad;
}

/*
 * Образец cwmrc: по строке на каждый из трёх ответов разбора. Пишется здесь, а
 * не лежит в дереве, чтобы числа ниже нельзя было подогнать правкой файла.
 */
static const char	 sample[] =
	"# образец\n"
	"ribbongap 12\n"
	"ribbonminwidth 200\n"
	"ribbonwidths 25 50 75 100\n"
	"ribbonhide yes\n"
	"bind-key CM-n ribbon-focus-right\n"
	"unbind-key CM-f\n"
	"borderwidth 3\n"
	"color activeborder \"#ffffff\"\n"
	"autogroup 3 \"Firefox\"\n"
	"ribbonrule column \"Emacs\"\n"
	"bind-mouse M-1 window-move\n"
	"nonsense 42\n";

static int
write_sample(char *path, size_t len)
{
	FILE	*fp;
	int	 fd;

	(void)snprintf(path, len, "%s/digitwm-runcheck.XXXXXX",
	    (getenv("TMPDIR") != NULL) ? getenv("TMPDIR") : "/tmp");
	if ((fd = mkstemp(path)) == -1)
		return -1;
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		return -1;
	}
	(void)fputs(sample, fp);
	(void)fclose(fp);
	return 0;
}

int
main(void)
{
	struct wsiconf_report	 rep;
	struct ribbon		*rb;
	struct client_ctx	*cc;
	struct screen_ctx	*sc;
	struct wsip_rect	 r;
	struct ribbon_col	*col;
	char			 path[256];
	int			 i, before, after, refused, cols;

	/*
	 * Построчно, чтобы жалобы разбора (stderr) стояли там, где им место, -
	 * между строкой, которая их обещает, и числами, которые их считают.
	 */
	(void)setvbuf(stdout, NULL, _IOLBF, 0);

	(void)printf("== 1. cwmrc: что порт берёт, а что называет чужим\n");

	wsiconf_default();
	if (write_sample(path, sizeof(path)) != 0) {
		(void)fprintf(stderr, "не удалось написать временный cwmrc\n");
		return 1;
	}
	(void)printf("   (разбор образца из 12 директив; жалобы под этой "
	    "строкой - его и есть)\n");
	if (wsiconf_load(path, &rep) != 0) {
		(void)fprintf(stderr, "не удалось прочитать %s\n", path);
		return 1;
	}
	(void)unlink(path);

	(void)printf("   директив:         %d\n", rep.lines);
	(void)printf("   принято:          %d\n", rep.taken);
	(void)printf("   отдано X11:       %d (названы поимённо, с причиной)\n",
	    rep.skipped);
	(void)printf("   не понято:        %d\n", rep.bad);
	verdict("настройки ленты применены", Conf.ribbongap == 12 &&
	    Conf.ribbonminw == 200 && Conf.ribbonhide == 1 &&
	    Conf.ribbonwidth[0] == 25, "ribbongap/minwidth/hide/widths");
	verdict("bind-key добавил привязку",
	    bind_of(WSI_CMD_FOCUS_RIGHT) >= 0, NULL);
	verdict("unbind-key убрал привязку",
	    bind_of(WSI_CMD_FLOAT) < 0, "CM-f был ribbon-float-toggle");
	verdict("мусорная строка не понята", rep.bad == 1, "nonsense 42");
	(void)printf("\n");

	(void)printf("== 2. запуск: та же последовательность, что в main()\n");

	fake_reset();
	fake_display("fake0", 0, 0, 1440, 900, 0, 0, 1440, 900);
	fake_pointer(700, 400);

	/*
	 * Настройки те же, что после разбора, кроме ribbonhide: с ним половина
	 * окон паркуется за краем, и «стоит там, где велела модель» пришлось бы
	 * проверять двумя мерками вместо одной. Парковка проверена отдельно, в
	 * macos/check.sh.
	 */
	Conf.ribbonhide = 0;

	verdict("wsi_run_init() поднял порт", wsi_run_init() == 0, NULL);
	sc = wsi_run_screen();
	verdict("лента заведена по имени дисплея",
	    (rb = ribbon_current(sc)) != NULL, "fake0");
	verdict("цикл считает себя запущенным", wsi_run_running() == 1, NULL);

	for (i = 0; i < 6; i++) {
		if (open_window() == NULL) {
			(void)fprintf(stderr, "окно не открылось\n");
			return 1;
		}
	}
	rb = ribbon_current(sc);
	cols = (rb != NULL) ? ribbon_col_count(rb) : 0;
	(void)printf("   окон принято:     %lu\n", wsi_core_stats()->opened);
	(void)printf("   колонок:          %d\n", cols);
	verdict("окна стоят там, где велела модель", drift() == 0,
	    "сверено с оконной системой");
	(void)printf("\n");

	(void)printf("== 3. клавиши: от привязки до движения окна\n");

	refused = wsi_run_keys();
	(void)wsiconf_binds(&i);
	(void)printf("   привязок:         %d\n", i);
	(void)printf("   отвергнуто:       %d (названа вслух, остальные "
	    "работают)\n", refused);
	verdict("отказ не считается провалом запуска", refused == 1, NULL);

	cc = client_current(sc);
	before = (cc != NULL && cc->rbcol != NULL) ?
	    ribbon_col_index(rb, cc->rbcol) : -1;
	press(WSI_CMD_FOCUS_LEFT);
	cc = client_current(sc);
	after = (cc != NULL && cc->rbcol != NULL) ?
	    ribbon_col_index(rb, cc->rbcol) : -1;
	verdict("ribbon-focus-left сменил колонку", after == before - 1,
	    "фокус ушёл влево на одну");

	cc = client_current(sc);
	before = (cc != NULL) ? cc->geom.w : 0;
	press(WSI_CMD_WIDTH_GROW);
	cc = client_current(sc);
	after = (cc != NULL) ? cc->geom.w : 0;
	verdict("ribbon-width-grow расширил колонку", after > before, NULL);
	verdict("и выдано наружу, а не только в модель",
	    drift() == 0, NULL);

	/*
	 * Мерка тут не «индекс колонки вырос»: окно было в колонке одно, и
	 * уходя вправо оно свою колонку распускает - ribbon.c:1379. Так что
	 * проверяется то, что произошло на самом деле: колонка у окна другая,
	 * а колонок на ленте стало на одну меньше.
	 */
	cc = client_current(sc);
	before = ribbon_col_count(rb);
	col = (cc != NULL) ? cc->rbcol : NULL;
	press(WSI_CMD_MOVE_RIGHT);
	cc = client_current(sc);
	after = ribbon_col_count(rb);
	verdict("ribbon-move-right унёс окно вправо",
	    cc != NULL && cc->rbcol != NULL && cc->rbcol != col &&
	    after == before - 1, "своя колонка распустилась");

	press(WSI_CMD_QUIT);
	verdict("quit остановил цикл", wsi_run_running() == 0, NULL);
	(void)printf("\n");

	(void)printf("== 4. чужое перемещение возвращается на место\n");

	/*
	 * Это и есть разница между «расставить окна один раз» и «оконный
	 * менеджер»: пункт 2 цикла в macos/wsi_run.c. Пользователь тащит окно,
	 * извещение приходит как чужое, и следующий же оборот кладёт окно
	 * обратно.
	 */
	cc = win[0];
	fake_user_move(cc->win, 5, 5, 100, 100);
	for (i = 0; i < 8; i++)
		wsi_run_step(20.0);
	verdict("окно вернулось туда, где велела модель",
	    fake_frame(cc->win, &r) == 0 && r.x == cc->geom.x &&
	    r.w == cc->geom.w, NULL);
	(void)printf("   чужих извещений:  %lu\n",
	    wsi_core_stats()->echo_foreign);
	(void)printf("   своих отброшено:  %lu\n",
	    wsi_core_stats()->echo_own);
	(void)printf("\n");

	(void)printf("== 5. монитор подключили\n");

	fake_display("fake1", 1440, 0, 1280, 800, 1440, 0, 1280, 800);
	for (i = 0; i < 80; i++)
		wsi_run_step(20.0);
	i = 0;
	TAILQ_FOREACH(rb, &sc->ribbonq, entry)
		i++;
	verdict("вторая лента появилась сама", i == 2,
	    "опрос дисплеев раз в секунду");
	(void)printf("\n");

	if (failures > 0) {
		(void)printf("НЕ СОШЛОСЬ: %d\n", failures);
		return 1;
	}
	(void)printf("Проверено здесь: последовательность запуска, чтение "
	    "cwmrc и разбор клавиши\nделают то, что обещают, поверх оконной "
	    "системы из памяти. Ни одно число выше\nне является измерением "
	    "macOS.\n");
	return 0;
}
