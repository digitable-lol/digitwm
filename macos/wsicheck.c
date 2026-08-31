/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - is the ribbon over the macOS port the same ribbon as over X11
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
 * Saying "the macOS port implements wsi.h" is worth nothing unless the
 * sentence can fail, so this is the way it is made able to.  Three questions,
 * each answered in numbers, none of them about macOS - the machine running
 * this has no macOS, and macos/wsi_fake.c says at length what its windows
 * are and are not.
 *
 *   1. THE SAME LAYOUT.  A ribbon is built the live way - windows opened one
 *      by one through ribbon_client_insert(), focus moved, widths changed,
 *      windows closed, every step pushed onto the port through the eleven
 *      calls - and then the state it arrived at is put to the X11 binary as
 *      "cwm -C 'layout-probe layout ...'".  Every window's rectangle is
 *      compared twice: against the model, and against what the fake window
 *      system actually holds.  The second comparison is the one that catches
 *      this file's own subject: a move the port decided not to send, or sent
 *      wrong, shows up there and nowhere else.
 *
 *      This is the same proof tools/wasm-layout/check.mjs makes for the
 *      browser, and it is made the same way on purpose: two answers to one
 *      question, compared by number.  It is stronger here in one respect -
 *      the browser check drives ribbon_scroll() alone, which touches none of
 *      the contract, while this one drives the calls.
 *
 *   2. THE REVERSE MECHANISM.  wsi.h says a port without a round trip has to
 *      keep the settle guarantee backwards - tag your own moves, drop the
 *      notifications that carry a tag.  The check runs one scenario twice,
 *      with the tagging on and off, and prints what the difference is: with
 *      it, a scroll costs a fixed number of writes and stops; without it,
 *      every write comes back as somebody else's move and is answered with
 *      another write.  That runaway is the oscillation wsi.h describes, and
 *      showing it is the only way to show that preventing it was work.
 *
 *   3. PARKING.  With "ribbonhide" on, every window outside the viewport
 *      must be off the canvas and every window inside it must hold the
 *      rectangle the model gave it.  macOS has no unmap, so "off the canvas"
 *      is "past the edge", and the check states the leak in pixels rather
 *      than hiding it.
 *
 *   ./macos/wsicheck [--wm ./cwm] [--cases 400]
 */

#include <sys/types.h>
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calmwm.h"
#include "wsi_core.h"
#include "wsi_fake.h"

#define MAXWIN		200
#define MAXCOLS		24

struct conf	 Conf;

static struct screen_ctx	 sc;
static struct client_ctx	*win[MAXWIN];
static int			 nwin;
static wsip_window		 nextid;
static const char		*wmpath = "./cwm";

static int	 fail_layout;
static int	 fail_held;
static int	 fail_park;
static int	 fail_slop;
static long	 cmp_windows;
static long	 cmp_cases;
static long	 cmp_held;
static long	 park_shown;
static long	 park_hidden;

/*
 * The same generator the other harnesses use, seed written down rather than
 * called random: a check whose cases change between runs cannot be quoted.
 */
static unsigned long	 seed = 20260830UL;

static int
rnd(int n)
{
	seed = (seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
	return (int)(seed % (unsigned long)n);
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void	*p = calloc(nmemb, size);

	if (p == NULL) {
		(void)fprintf(stderr, "out of memory\n");
		exit(1);
	}
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
	void	*p = realloc(ptr, nmemb * size);

	if (p == NULL) {
		(void)fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return p;
}

char *
xstrdup(const char *s)
{
	size_t	 n = strlen(s) + 1;
	char	*p = xcalloc(1, n);

	(void)memcpy(p, s, n);
	return p;
}

/*
 * No cwmrc here, so no "ribbonrule" line to match - the same answer
 * tools/wasm-layout/shim.c gives, and for the same reason: it is the right
 * answer for a surface with no configuration, not a stub for the linker.
 */
int
conf_ribbonrule_match(struct client_ctx *cc)
{
	(void)cc;
	return RIBBON_RULE_NONE;
}

/*
 * Pad a label to a width counted in characters rather than bytes: the labels
 * are Russian, the terminal is UTF-8, and "%-26s" pads by bytes, which turns
 * a table of numbers into a staircase.
 */
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

static struct ribbon *
first_ribbon(void)
{
	return TAILQ_FIRST(&sc.ribbonq);
}

/*
 * The port's event loop, as much of it as this check needs: run the clock
 * forward, let the platform deliver what has come due, and if any of it was
 * somebody else moving a window, put the layout back.  That last clause is
 * what a window manager is; it is also what turns an unrecognised echo into
 * a runaway, which is the point of check 2.
 */
static int
pump(double ms, int rounds)
{
	const struct wsi_stats	*st = wsi_core_stats();
	unsigned long		 before;
	int			 i;

	for (i = 0; i < rounds; i++) {
		before = st->echo_foreign;
		if (wsip_pump(ms) == 0 && st->echo_foreign == before)
			break;
		if (st->echo_foreign != before)
			ribbon_sync(&sc);
	}
	return i;
}

static void
setup(int vw, int vh, int gap, int minw, int minh, int hide, int border)
{
	fake_reset();
	fake_display("fake0", 0, 0, vw, vh, 0, 0, vw, vh);
	fake_pointer(-10000, -10000);	/* nowhere on any display */

	(void)memset(&Conf, 0, sizeof(Conf));
	Conf.ribbon = 1;
	Conf.ribbonhide = hide;
	Conf.ribbongap = gap;
	Conf.ribbonminw = minw;
	Conf.ribbonminh = minh;
	Conf.ribbonwidth[0] = 33;
	Conf.ribbonwidth[1] = 50;
	Conf.ribbonwidth[2] = 67;
	Conf.ribbonwidth[3] = 100;

	(void)memset(&sc, 0, sizeof(sc));
	TAILQ_INIT(&sc.clientq);
	TAILQ_INIT(&sc.regionq);
	ribbon_screen_init(&sc);

	nwin = 0;
	nextid = 1;
	(void)memset(win, 0, sizeof(win));

	wsi_core_init(&sc);
	wsi_core_border(border);
	ribbon_screen_relayout(&sc);
}

/*
 * A window appears.  The harness does the platform layer's whole share of it
 * - there is a window now, here is where its application put it - and
 * everything after that is macos/wsi_core.c's, which is the point of drawing
 * the line where it is drawn.
 */
static struct client_ctx *
open_window(void)
{
	struct wsip_rect	 r;
	wsip_window		 id;

	if (nwin >= MAXWIN)
		return NULL;

	id = nextid++;

	/*
	 * Where the application put it before we heard of it.  On X11 this
	 * geometry never reaches the screen - the manager answers the
	 * MapRequest instead of the server.  On macOS it does, and that is
	 * loss 1 of doc/portability.md; here it matters only in that the
	 * window starts somewhere that is not its place, so a port that
	 * forgot to move it would be caught.
	 */
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

static void
close_window(int idx)
{
	wsip_window	 id = win[idx]->win;
	int		 i;

	wsi_note_close(id);
	fake_close(id);

	for (i = idx; i < nwin - 1; i++)
		win[i] = win[i + 1];
	nwin--;
}

/*
 * Turn the state the live path arrived at into the question "layout-probe"
 * answers, and ask the X11 binary.  Everything the probe needs is read off
 * the ribbon rather than remembered, so a bug in the live path shows up as a
 * disagreement instead of being asked about twice.
 */
static int
probe_cmd(char *buf, size_t len, int vw, int vh, int gap, int minw, int minh,
    int border)
{
	struct ribbon		*rb = first_ribbon();
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	char			 cols[192], presets[192];
	char			 num[32];
	int			 ncol = 0, fwin = -1, i;

	if (rb == NULL)
		return -1;

	cols[0] = presets[0] = '\0';
	TAILQ_FOREACH(col, &rb->colq, entry) {
		if (ncol >= MAXCOLS)
			return -1;
		(void)snprintf(num, sizeof(num), "%s%d", ncol ? "," : "",
		    col->nwin);
		if (strlcat(cols, num, sizeof(cols)) >= sizeof(cols))
			return -1;
		(void)snprintf(num, sizeof(num), "%s%d", ncol ? "," : "",
		    col->preset);
		if (strlcat(presets, num, sizeof(presets)) >= sizeof(presets))
			return -1;
		ncol++;
	}
	if (ncol == 0)
		return -1;

	if (rb->focus != NULL) {
		i = 0;
		TAILQ_FOREACH(cc, &rb->focus->winq, rbentry) {
			if (cc == rb->focus->focus)
				fwin = i;
			i++;
		}
	}

	(void)snprintf(buf, len,
	    "%s -C 'layout-probe layout viewport=%dx%d gap=%d border=%d "
	    "min-width=%d min-height=%d widths=33,50,67,100 columns=%s "
	    "presets=%s focus=%d focus-window=%d offset=%d voffset=%d' "
	    "2>/dev/null",
	    wmpath, vw, vh, gap, border, minw, minh, cols, presets,
	    ribbon_col_index(rb, rb->focus), fwin, rb->offset, rb->voffset);
	return 0;
}

struct prow {
	int	 col, idx;
	int	 rx, ry, rw, rh;
	int	 sx, sy, sw, sh;
};

static int
ask_wm(const char *cmd, struct prow *row, int max)
{
	char	 line[512];
	FILE	*fp;
	int	 n = 0;

	if ((fp = popen(cmd, "r")) == NULL)
		return -1;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (strncmp(line, "window ", 7) != 0)
			continue;
		if (n >= max)
			break;
		if (sscanf(line, "window %d %d ribbon %d %d %d %d "
		    "screen %d %d %d %d", &row[n].col, &row[n].idx,
		    &row[n].rx, &row[n].ry, &row[n].rw, &row[n].rh,
		    &row[n].sx, &row[n].sy, &row[n].sw, &row[n].sh) == 10)
			n++;
	}
	(void)pclose(fp);
	return n;
}

/*
 * Check 1.
 */
static void
check_layout(int cases)
{
	struct prow		 row[MAXWIN];
	char			 cmd[1024];
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	struct wsip_rect	 held;
	int			 c, k, n, i, nrow;
	int			 vw, vh, gap, minw, minh, border, hide;
	int			 nops, shown;

	for (c = 0; c < cases; c++) {
		vw = 640 + rnd(1600);
		vh = 480 + rnd(1000);
		gap = rnd(17);
		minw = 60 + rnd(120);
		minh = 40 + rnd(80);
		border = rnd(4);
		hide = rnd(2);

		setup(vw, vh, gap, minw, minh, hide, border);

		n = 1 + rnd(14);
		for (k = 0; k < n; k++)
			(void)open_window();

		nops = rnd(12);
		for (k = 0; k < nops; k++) {
			switch (rnd(6)) {
			case 0:
				ribbon_focus_col(&sc, CWM_LEFT);
				break;
			case 1:
				ribbon_focus_col(&sc, CWM_RIGHT);
				break;
			case 2:
				ribbon_focus_win(&sc, CWM_UP);
				break;
			case 3:
				ribbon_focus_win(&sc, CWM_DOWN);
				break;
			case 4:
				if (nwin > 0)
					ribbon_width(win[rnd(nwin)], rnd(4));
				break;
			default:
				if (nwin > 1)
					close_window(rnd(nwin));
				break;
			}
			(void)pump(50.0, 8);
		}
		(void)pump(50.0, 16);

		if ((rb = first_ribbon()) == NULL || nwin == 0)
			continue;
		if (probe_cmd(cmd, sizeof(cmd), vw, vh, gap, minw, minh,
		    border) != 0)
			continue;
		if ((nrow = ask_wm(cmd, row, MAXWIN)) <= 0) {
			(void)fprintf(stderr, "cwm answered nothing: %s\n",
			    cmd);
			fail_layout++;
			continue;
		}

		cmp_cases++;
		i = 0;
		TAILQ_FOREACH(col, &rb->colq, entry) {
			TAILQ_FOREACH(cc, &col->winq, rbentry) {
				if (i >= nrow)
					break;
				cmp_windows++;

				/* the model, against the X11 binary */
				if (cc->rbgeom.x != row[i].rx ||
				    cc->rbgeom.y != row[i].ry ||
				    cc->rbgeom.w != row[i].rw ||
				    cc->rbgeom.h != row[i].rh ||
				    cc->geom.x != row[i].sx ||
				    cc->geom.y != row[i].sy ||
				    cc->geom.w != row[i].sw ||
				    cc->geom.h != row[i].sh) {
					if (fail_layout < 5)
						(void)printf("РАСХОЖДЕНИЕ "
						    "случай %d окно %d: наше "
						    "%d %d %d %d / %d %d %d %d,"
						    " cwm %d %d %d %d / "
						    "%d %d %d %d\n", c, i,
						    cc->rbgeom.x, cc->rbgeom.y,
						    cc->rbgeom.w, cc->rbgeom.h,
						    cc->geom.x, cc->geom.y,
						    cc->geom.w, cc->geom.h,
						    row[i].rx, row[i].ry,
						    row[i].rw, row[i].rh,
						    row[i].sx, row[i].sy,
						    row[i].sw, row[i].sh);
					fail_layout++;
				}

				/*
				 * And the thing the browser check cannot ask:
				 * did the port actually hand that rectangle
				 * to the window system?
				 */
				shown = !(cc->flags & CLIENT_HIDDEN);
				if (shown && fake_frame(cc->win, &held) == 0) {
					cmp_held++;
					if (held.x != cc->geom.x ||
					    held.y != cc->geom.y ||
					    held.w != cc->geom.w ||
					    held.h != cc->geom.h) {
						if (fail_held < 5)
							(void)printf("НЕ "
							    "ВЫДАНО случай %d "
							    "окно %d: модель "
							    "%d %d %d %d, "
							    "система %d %d %d "
							    "%d\n", c, i,
							    cc->geom.x,
							    cc->geom.y,
							    cc->geom.w,
							    cc->geom.h,
							    held.x, held.y,
							    held.w, held.h);
						fail_held++;
					}
				}
				i++;
			}
		}
		if (i != nrow) {
			(void)printf("РАСХОЖДЕНИЕ случай %d: окон у нас %d, "
			    "у cwm %d\n", c, i, nrow);
			fail_layout++;
		}
	}
}

/*
 * Check 2: the reverse mechanism, shown by taking it away.
 */
struct echo_result {
	unsigned long	 frame_set;
	unsigned long	 note_frame;
	unsigned long	 own;
	unsigned long	 foreign;
	int		 rounds;
	int		 focus_kept;
};

static void
echo_scenario(int tagging, struct echo_result *res)
{
	const struct wsi_stats	*st = wsi_core_stats();
	struct client_ctx	*a, *b;
	int			 i;

	setup(1280, 800, 8, 120, 60, 0, 1);
	wsi_core_tagging(tagging);
	fake_latency(20.0);

	for (i = 0; i < 8; i++)
		(void)open_window();
	(void)pump(50.0, 40);

	wsi_core_stats_reset();

	/* A scroll: the ribbon moves every window under a resting pointer. */
	for (i = 0; i < 6; i++) {
		ribbon_focus_col(&sc, (i % 2) ? CWM_LEFT : CWM_RIGHT);
		res->rounds += pump(25.0, 200);
	}

	/*
	 * And the stale-focus case: two activations in a row, into two
	 * applications of different speeds, so the first one's notice arrives
	 * after the second's.  Untagged, that older echo takes the keyboard
	 * back off the window the ribbon just moved it to - the keyboard
	 * jumping backwards a tenth of a second after the user asked for it.
	 * No timeout fixes this, which is the point: the port counts its own
	 * requests instead of timing them.
	 */
	a = win[0];
	b = win[nwin - 1];
	fake_latency_win(a->win, 90.0);	/* a slow application */
	fake_latency_win(b->win, 5.0);	/* and a quick one */
	client_set_active(a);
	client_set_active(b);
	(void)pump(50.0, 40);
	res->focus_kept = (client_current(&sc) == b);

	res->frame_set = st->frame_set;
	res->note_frame = st->note_frame;
	res->own = st->echo_own;
	res->foreign = st->echo_foreign;

	wsi_core_tagging(1);
}

/*
 * Check 3: what "off the canvas" means where there is no unmap.
 */
static void
check_park(void)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	struct wsip_rect	 held;
	int			 i, visible, edge;

	setup(1024, 768, 8, 120, 60, 1, 1);
	for (i = 0; i < 20; i++)
		(void)open_window();
	(void)pump(50.0, 40);

	if ((rb = first_ribbon()) == NULL)
		return;
	edge = 1024 - 1;	/* WSI_PARK_LEAK of the display stays visible */

	TAILQ_FOREACH(col, &rb->colq, entry) {
		visible = !((col->x + col->w) <= rb->offset ||
		    col->x >= (rb->offset + rb->view.w) ||
		    col->h <= rb->voffset);
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			if (fake_frame(cc->win, &held) != 0)
				continue;
			if (visible) {
				park_shown++;
				if (held.x != cc->geom.x ||
				    held.y != cc->geom.y ||
				    held.w != cc->geom.w ||
				    held.h != cc->geom.h)
					fail_park++;
				if (cc->flags & CLIENT_HIDDEN)
					fail_park++;
			} else {
				park_hidden++;
				if (held.x != edge)
					fail_park++;
				if (!(cc->flags & CLIENT_HIDDEN))
					fail_park++;
			}
		}
	}
}

/*
 * Check 4: the application that obeys approximately.
 *
 * A terminal snaps to whole character cells, so the rectangle that comes
 * back is not the rectangle that went out - and it is still our own move,
 * not the user's.  This is the case that makes a tag more than a remembered
 * rectangle: the exact arm misses it and the batch clock catches it.  Get it
 * wrong and every window with size hints starts a fight the manager cannot
 * win.
 */
static void
check_slop(unsigned long *own, unsigned long *foreign, long *drift)
{
	const struct wsi_stats	*st = wsi_core_stats();
	struct client_ctx	*cc;
	struct wsip_rect	 held;
	int			 i;

	setup(1280, 800, 8, 120, 60, 0, 1);
	fake_latency(20.0);
	fake_slop(7);			/* cells of eight pixels */

	for (i = 0; i < 6; i++)
		(void)open_window();
	wsi_core_stats_reset();
	for (i = 0; i < 4; i++) {
		ribbon_focus_col(&sc, CWM_RIGHT);
		(void)pump(25.0, 40);
	}
	(void)pump(50.0, 40);

	*own = st->echo_own;
	*foreign = st->echo_foreign;
	*drift = 0;
	TAILQ_FOREACH(cc, &sc.clientq, entry) {
		if (fake_frame(cc->win, &held) != 0)
			continue;
		if (held.w != cc->geom.w || held.h != cc->geom.h)
			(*drift)++;
	}
}

int
main(int argc, char **argv)
{
	struct echo_result	 on, off;
	unsigned long		 slop_own = 0, slop_foreign = 0;
	long			 slop_drift = 0;
	int			 cases = 400, i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--wm") == 0 && i + 1 < argc)
			wmpath = argv[++i];
		else if (strcmp(argv[i], "--cases") == 0 && i + 1 < argc)
			cases = atoi(argv[++i]);
		else {
			(void)fprintf(stderr,
			    "использование: %s [--wm ./cwm] [--cases N]\n",
			    argv[0]);
			return 2;
		}
	}

	(void)printf("== 1. та же ли это раскладка, что у двоичного файла\n");
	check_layout(cases);
	(void)printf("   двоичный файл:  %s\n", wmpath);
	(void)printf("   случаев:        %ld\n", cmp_cases);
	(void)printf("   окон сверено:   %ld (модель против cwm)\n",
	    cmp_windows);
	(void)printf("   из них выдано:  %ld (что реально держит "
	    "оконная система)\n", cmp_held);
	if (fail_layout == 0)
		(void)printf("   расхождений:    0 — лента поверх macOS-порта "
		    "считает то же, что лента поверх X11\n");
	else
		(void)printf("   РАСХОЖДЕНИЙ:    %d\n", fail_layout);
	if (fail_held == 0)
		(void)printf("   не выдано:      0 — каждое видимое окно стоит "
		    "там, где велела модель\n");
	else
		(void)printf("   НЕ ВЫДАНО:      %d\n", fail_held);

	(void)memset(&on, 0, sizeof(on));
	(void)memset(&off, 0, sizeof(off));
	echo_scenario(1, &on);
	echo_scenario(0, &off);

	(void)printf("\n== 2. обратный механизм вместо круговых рейсов\n");
	(void)printf("   тот же сценарий (восемь окон, шесть прокруток), "
	    "с пометкой и без\n");
	(void)printf("   ");
	label("", 24);
	(void)printf("%12s %12s\n", "с пометкой", "без неё");
	(void)printf("   ");
	label("выдач геометрии", 24);
	(void)printf("%12lu %12lu\n", on.frame_set, off.frame_set);
	(void)printf("   ");
	label("извещений получено", 24);
	(void)printf("%12lu %12lu\n", on.note_frame, off.note_frame);
	(void)printf("   ");
	label("признано своими", 24);
	(void)printf("%12lu %12lu\n", on.own, off.own);
	(void)printf("   ");
	label("принято за чужие", 24);
	(void)printf("%12lu %12lu\n", on.foreign, off.foreign);
	(void)printf("   ");
	label("кругов до тишины", 24);
	(void)printf("%12d %12d\n", on.rounds, off.rounds);
	(void)printf("   ");
	label("фокус у нового окна", 24);
	(void)printf("%12s %12s\n", on.focus_kept ? "да" : "НЕТ",
	    off.focus_kept ? "да" : "НЕТ");

	check_park();
	(void)printf("\n== 3. что значит «убрать с экрана» там, где нет "
	    "unmap\n");
	(void)printf("   окон внутри вьюпорта:  %ld, все держат геометрию "
	    "модели\n", park_shown);
	(void)printf("   окон за краем:         %ld, все припаркованы\n",
	    park_hidden);
	(void)printf("   видимый остаток:       1 пиксель на окно "
	    "(macOS не даёт увести окно за экран целиком)\n");
	if (fail_park == 0)
		(void)printf("   нарушений:             0\n");
	else
		(void)printf("   НАРУШЕНИЙ:             %d\n", fail_park);

	check_slop(&slop_own, &slop_foreign, &slop_drift);
	(void)printf("\n== 4. приложение, которое слушается приблизительно\n");
	(void)printf("   шесть окон подгоняют размер под ячейку в 8 пикселей; "
	    "к нам возвращается\n   не тот прямоугольник, что мы выдали, "
	    "и это всё равно НАШЕ перемещение\n");
	(void)printf("   признано своими:       %lu\n", slop_own);
	(void)printf("   принято за чужие:      %lu\n", slop_foreign);
	(void)printf("   окон разошлось с моделью по размеру: %ld "
	    "(приложение вправе, лента не спорит)\n", slop_drift);

	if (slop_foreign != 0)
		fail_slop++;

	if (fail_layout || fail_held || fail_park || fail_slop ||
	    !on.focus_kept || off.focus_kept) {
		(void)printf("\nПРОВЕРКА НЕ ПРОЙДЕНА\n");
		return 1;
	}
	(void)printf("\nПроверено здесь: macos/wsi_core.c выполняет "
	    "одиннадцать операций wsi.h,\n");
	(void)printf("и лента над ним даёт те же числа, что лента над X11. "
	    "Ни одно из этих чисел\n");
	(void)printf("не является измерением macOS — см. шапку "
	    "macos/wsi_fake.c.\n");
	return 0;
}
