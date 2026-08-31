/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - layout probe for the conformance harness
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
 * "layout-probe" answers what the layout would do, without drawing anything.
 *
 * The point is the conformance harness: the same vectors go through the
 * TypeScript that FTS generates from the layout models and through this, and
 * a mismatch fails CI naming the utility and the vector.  For that to mean
 * anything the probe has to run the very code the window manager runs, not a
 * second copy of it - so it calls ribbon_policy_* and, for whole layouts,
 * builds a real ribbon out of real columns and runs ribbon_scroll() over it.
 *
 * It opens no display and touches no window.  That is deliberate: CI has no
 * X server to spare, and a probe that needed a running window manager to
 * answer a question about arithmetic would be answering a different question.
 *
 * Nine utilities carry the names of the FTS models, in either surface:
 *
 *   scroll-offset       "Смещение ленты после фокуса"
 *   stack-offset        "Смещение полотна после фокуса"
 *   column-width        "Ширина колонки по пресету"
 *   window-height       "Высота окна в колонке"
 *   insertion           "Куда вставить окно"
 *   focus-after-close   "Фокус после закрытия"
 *   output-change       "Смещение после смены монитора"
 *   strut-span          "Достаёт ли полоса до области"
 *   strut-reserve       "Сколько полоса отнимает у области"
 *   strut-pair          "Что остаётся паре панелей"
 *
 * and "layout" answers with the geometry of every window of a whole
 * scenario.  Field names are English kebab-case on both surfaces, because
 * the vectors are shared between them and can only be spelled one way.
 */

#include <sys/types.h>
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

#define PROBE_VERB	"layout-probe"

/*
 * FTS writes the name of a utility in guillemets, and the Russian surface
 * puts spaces inside them, so the name cannot be picked off by whitespace.
 */
#define PROBE_LQUOT	"«"
#define PROBE_RQUOT	"»"
#define PROBE_MAXARG	32
#define PROBE_MAXCOL	64
#define PROBE_KEYLEN	64
#define PROBE_VALLEN	128

struct probe_arg {
	char	 key[PROBE_KEYLEN];
	char	 val[PROBE_VALLEN];
};

struct probe_ctx {
	struct probe_arg	 arg[PROBE_MAXARG];
	int			 nargs;
	int			 fail;
	char			 msg[256];
};

/* One RandR output as the probe spells it: NAME:WxH+X+Y. */
struct probe_out {
	char	 name[32];
	int	 x, y, w, h;
};

static void		 probe_error(struct probe_ctx *, const char *, ...)
			    __attribute__((__format__ (printf, 2, 3)));
static int		 probe_parse(struct probe_ctx *, char *);
static int		 probe_parse_json(struct probe_ctx *, char *);
static int		 probe_parse_pairs(struct probe_ctx *, char *);
static void		 probe_add(struct probe_ctx *, const char *,
			     const char *);
static const char	*probe_get(struct probe_ctx *, const char *);
static int		 probe_req(struct probe_ctx *, const char *);
static int		 probe_opt(struct probe_ctx *, const char *, int);
static int		 probe_list(struct probe_ctx *, const char *, int *,
			     int);
static int		 probe_dim(struct probe_ctx *, const char *, int *,
			     int *);
static int		 probe_scalar(struct probe_ctx *, const char *);
static void		 probe_report(struct ribbon *, int, const char *);

/*
 * Window identity is off unless a scenario asks for it with "ids=1".
 * The default output is a documented sample (doc/terminal.md) and a
 * byte-for-byte baseline other trees are compared against, so a field
 * that only reordering needs does not belong in it by default.
 */
static int		 probe_ids;
static int		 probe_layout(struct probe_ctx *);
static int		 probe_outs(struct probe_ctx *, const char *,
			     struct probe_out *, int);
static void		 probe_regions(struct screen_ctx *, struct probe_out *,
			     int);
static void		 probe_regions_free(struct screen_ctx *);
static void		 probe_outputs_report(struct screen_ctx *, const char *);
static int		 probe_outputs(struct probe_ctx *);

/* The FTS models exist on two surfaces; both names mean the same utility. */
static const struct {
	const char	*name;
	const char	*alias;
} probe_utils[] = {
	{ "scroll-offset",	"Смещение ленты после фокуса" },
	{ "stack-offset",	"Смещение полотна после фокуса" },
	{ "column-width",	"Ширина колонки по пресету" },
	{ "window-height",	"Высота окна в колонке" },
	{ "insertion",		"Куда вставить окно" },
	{ "focus-after-close",	"Фокус после закрытия" },
	{ "output-change",	"Смещение после смены монитора" },
	{ "strut-span",		"Достаёт ли полоса до области" },
	{ "strut-reserve",	"Сколько полоса отнимает у области" },
	{ "strut-pair",		"Что остаётся паре панелей" },
	{ "layout",		"Раскладка" },
	{ "outputs",		"Мониторы" },
};

static void
probe_error(struct probe_ctx *p, const char *fmt, ...)
{
	va_list	 ap;

	if (p->fail)
		return;

	p->fail = 1;
	va_start(ap, fmt);
	(void)vsnprintf(p->msg, sizeof(p->msg), fmt, ap);
	va_end(ap);
}

static void
probe_add(struct probe_ctx *p, const char *key, const char *val)
{
	if (p->nargs >= PROBE_MAXARG) {
		probe_error(p, "too many fields");
		return;
	}
	if (strlcpy(p->arg[p->nargs].key, key, PROBE_KEYLEN) >= PROBE_KEYLEN) {
		probe_error(p, "field name too long");
		return;
	}
	if (strlcpy(p->arg[p->nargs].val, val, PROBE_VALLEN) >= PROBE_VALLEN) {
		probe_error(p, "value of \"%s\" too long", key);
		return;
	}
	p->nargs++;
}

/*
 * Flat JSON of scalars, which is all an FTS utility ever takes: an object
 * whose values are numbers, booleans or short strings.  Nesting is not
 * accepted because nesting cannot appear in a valid vector.
 */
static int
probe_parse_json(struct probe_ctx *p, char *s)
{
	char	 key[PROBE_KEYLEN], val[PROBE_VALLEN];
	char	*q;
	size_t	 n;

	if (*s != '{') {
		probe_error(p, "expected a JSON object");
		return -1;
	}
	s++;

	for (;;) {
		while ((*s == ' ') || (*s == '\t') || (*s == ',') ||
		    (*s == '\n'))
			s++;
		if ((*s == '}') || (*s == '\0'))
			break;

		if (*s != '"') {
			probe_error(p, "expected a quoted field name");
			return -1;
		}
		s++;
		if ((q = strchr(s, '"')) == NULL) {
			probe_error(p, "unterminated field name");
			return -1;
		}
		n = (size_t)(q - s);
		if (n >= sizeof(key)) {
			probe_error(p, "field name too long");
			return -1;
		}
		(void)memcpy(key, s, n);
		key[n] = '\0';
		s = q + 1;

		while ((*s == ' ') || (*s == '\t'))
			s++;
		if (*s != ':') {
			probe_error(p, "expected ':' after \"%s\"", key);
			return -1;
		}
		s++;
		while ((*s == ' ') || (*s == '\t'))
			s++;

		if (*s == '"') {
			s++;
			if ((q = strchr(s, '"')) == NULL) {
				probe_error(p, "unterminated value of \"%s\"",
				    key);
				return -1;
			}
		} else {
			q = s;
			while ((*q != ',') && (*q != '}') && (*q != '\0') &&
			    (*q != ' ') && (*q != '\n'))
				q++;
		}
		n = (size_t)(q - s);
		if (n >= sizeof(val)) {
			probe_error(p, "value of \"%s\" too long", key);
			return -1;
		}
		(void)memcpy(val, s, n);
		val[n] = '\0';
		s = (*q == '"') ? (q + 1) : q;

		probe_add(p, key, val);
		if (p->fail)
			return -1;
	}
	return 0;
}

/* The plainer form, "key=value key=value", for driving it by hand. */
static int
probe_parse_pairs(struct probe_ctx *p, char *s)
{
	char	*tok, *eq, *save;

	for (tok = strtok_r(s, " \t\n", &save); tok != NULL;
	    tok = strtok_r(NULL, " \t\n", &save)) {
		if ((eq = strchr(tok, '=')) == NULL) {
			probe_error(p, "expected key=value, got \"%s\"", tok);
			return -1;
		}
		*eq = '\0';
		probe_add(p, tok, eq + 1);
		if (p->fail)
			return -1;
	}
	return 0;
}

static int
probe_parse(struct probe_ctx *p, char *s)
{
	while ((*s == ' ') || (*s == '\t'))
		s++;

	if (*s == '\0')
		return 0;
	if (*s == '{')
		return probe_parse_json(p, s);

	return probe_parse_pairs(p, s);
}

static const char *
probe_get(struct probe_ctx *p, const char *key)
{
	int	 i;

	for (i = 0; i < p->nargs; i++) {
		if (strcmp(p->arg[i].key, key) == 0)
			return p->arg[i].val;
	}
	return NULL;
}

/*
 * Numbers.  A missing required field is an error rather than a zero: in a
 * conformance harness a silently defaulted typo is worse than a failure.
 * Booleans are spelled as the vectors spell them.
 */
static int
probe_value(struct probe_ctx *p, const char *key, const char *v)
{
	const char	*errstr;
	long long	 num;

	if ((strcmp(v, "true") == 0) || (strcmp(v, "yes") == 0))
		return 1;
	if ((strcmp(v, "false") == 0) || (strcmp(v, "no") == 0))
		return 0;

	num = strtonum(v, INT_MIN, INT_MAX, &errstr);
	if (errstr != NULL) {
		probe_error(p, "field \"%s\" is %s: %s", key, errstr, v);
		return 0;
	}
	return (int)num;
}

static int
probe_req(struct probe_ctx *p, const char *key)
{
	const char	*v;

	if ((v = probe_get(p, key)) == NULL) {
		probe_error(p, "missing field \"%s\"", key);
		return 0;
	}
	return probe_value(p, key, v);
}

static int
probe_opt(struct probe_ctx *p, const char *key, int def)
{
	const char	*v;

	if ((v = probe_get(p, key)) == NULL)
		return def;

	return probe_value(p, key, v);
}

/* A comma separated list of numbers, as in columns=2,1,3. */
static int
probe_list(struct probe_ctx *p, const char *key, int *out, int max)
{
	char		 buf[PROBE_VALLEN];
	const char	*v;
	char		*tok, *save;
	int		 n = 0;

	if ((v = probe_get(p, key)) == NULL)
		return 0;

	(void)strlcpy(buf, v, sizeof(buf));
	for (tok = strtok_r(buf, ",", &save); tok != NULL;
	    tok = strtok_r(NULL, ",", &save)) {
		if (n >= max) {
			probe_error(p, "field \"%s\" has too many values", key);
			return n;
		}
		out[n++] = probe_value(p, key, tok);
		if (p->fail)
			return n;
	}
	return n;
}

/* A pair spelled WxH, as in viewport=1280x800. */
static int
probe_dim(struct probe_ctx *p, const char *key, int *w, int *h)
{
	char		 buf[PROBE_VALLEN];
	const char	*v;
	char		*x;

	if ((v = probe_get(p, key)) == NULL)
		return 0;

	(void)strlcpy(buf, v, sizeof(buf));
	if ((x = strchr(buf, 'x')) == NULL) {
		probe_error(p, "field \"%s\" wants WxH, got %s", key, v);
		return -1;
	}
	*x = '\0';
	*w = probe_value(p, key, buf);
	*h = probe_value(p, key, x + 1);

	return p->fail ? -1 : 1;
}

/*
 * The nine scalar utilities.  Each one is a single call into the same
 * ribbon_policy_* function the window manager uses.
 */
static int
probe_scalar(struct probe_ctx *p, const char *util)
{
	int	 v = 0;

	if (strcmp(util, "scroll-offset") == 0) {
		v = ribbon_policy_offset(
		    probe_req(p, "viewport-width"),
		    probe_req(p, "column-left"),
		    probe_req(p, "column-width"),
		    probe_req(p, "offset"),
		    probe_opt(p, "gap", Conf.ribbongap),
		    probe_req(p, "ribbon-length"));
	} else if (strcmp(util, "stack-offset") == 0) {
		v = ribbon_policy_voffset(
		    probe_req(p, "viewport-height"),
		    probe_req(p, "window-top"),
		    probe_req(p, "window-height"),
		    probe_req(p, "offset"),
		    probe_opt(p, "gap", Conf.ribbongap),
		    probe_req(p, "canvas-height"));
	} else if (strcmp(util, "column-width") == 0) {
		v = ribbon_policy_width(
		    probe_req(p, "viewport-width"),
		    probe_req(p, "preset"),
		    probe_opt(p, "gap", Conf.ribbongap),
		    probe_opt(p, "min-width", Conf.ribbonminw));
	} else if (strcmp(util, "window-height") == 0) {
		v = ribbon_policy_height(
		    probe_req(p, "viewport-height"),
		    probe_req(p, "window-count"),
		    probe_req(p, "window-index"),
		    probe_opt(p, "gap", Conf.ribbongap),
		    probe_opt(p, "min-height", Conf.ribbonminh));
	} else if (strcmp(util, "insertion") == 0) {
		v = ribbon_policy_insert(
		    probe_req(p, "has-focus"),
		    probe_req(p, "transient"),
		    probe_req(p, "dialog"),
		    probe_req(p, "dock"),
		    probe_req(p, "fullscreen"),
		    probe_opt(p, "rule", RIBBON_RULE_NONE));
	} else if (strcmp(util, "focus-after-close") == 0) {
		v = ribbon_policy_close(
		    probe_req(p, "column-index"),
		    probe_req(p, "column-count"),
		    probe_req(p, "last-column"),
		    probe_req(p, "only-window"));
	} else if (strcmp(util, "output-change") == 0) {
		v = ribbon_policy_output(
		    probe_req(p, "viewport-width"),
		    probe_req(p, "offset"),
		    probe_req(p, "ribbon-length"));
	} else if (strcmp(util, "strut-span") == 0) {
		v = ribbon_policy_span(
		    probe_req(p, "span-start"),
		    probe_req(p, "span-end"),
		    probe_req(p, "region-start"),
		    probe_req(p, "region-length"));
	} else if (strcmp(util, "strut-reserve") == 0) {
		v = ribbon_policy_reserve(
		    probe_req(p, "strut"),
		    probe_req(p, "screen-size"),
		    probe_req(p, "region-start"),
		    probe_req(p, "region-length"),
		    probe_req(p, "far-edge"));
	} else if (strcmp(util, "strut-pair") == 0) {
		v = ribbon_policy_pair(
		    probe_req(p, "near-strut"),
		    probe_req(p, "far-strut"),
		    probe_req(p, "region-length"),
		    probe_req(p, "want-far"));
	} else {
		probe_error(p, "unknown utility \"%s\"", util);
	}

	if (p->fail)
		return -1;

	(void)printf("ok %s %d\n", util, v);
	return 0;
}

/*
 * One state of the ribbon, named by its stage.  A scenario prints one such
 * block, or two when it is asked to insert a window: the harness compares the
 * two and holds the insertion invariant against them.
 */
static void
probe_report(struct ribbon *rb, int border, const char *stage)
{
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 i, j;

	(void)printf("stage %s\n", stage);
	(void)printf("viewport %d %d %d %d\n", rb->view.x, rb->view.y,
	    rb->view.w, rb->view.h);
	(void)printf("gap %d\n", Conf.ribbongap);
	(void)printf("border %d\n", border);
	/*
	 * The canvas and the vertical offset come last on their lines, and the
	 * height of a column last on its own, because the harness reads these
	 * lines by position: appending keeps a reader that knows nothing of the
	 * second axis working, which is exactly what the hotplug harness is.
	 */
	(void)printf("ribbon length %d offset %d columns %d focus %d "
	    "canvas %d voffset %d\n",
	    rb->len, rb->offset, ribbon_col_count(rb),
	    ribbon_col_index(rb, rb->focus), rb->canvas, rb->voffset);

	i = 0;
	TAILQ_FOREACH(col, &rb->colq, entry) {
		(void)printf("column %d ribbon-x %d width %d preset %d "
		    "windows %d height %d\n", i, col->x, col->w, col->preset,
		    col->nwin, col->h);
		j = 0;
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			/*
			 * The identity comes last for the reason the canvas
			 * does: readers take these lines by position, and a
			 * field appended at the end costs them nothing.  It
			 * is the one thing geometry cannot say - two windows
			 * of a stack that trade places trade nothing a
			 * coordinate can see, because the slot keeps its
			 * size and only the occupant changes.  Windows are
			 * numbered in the order the scenario builds them,
			 * so the one an insertion adds carries the highest
			 * number.
			 */
			(void)printf("window %d %d ribbon %d %d %d %d "
			    "screen %d %d %d %d", i, j,
			    cc->rbgeom.x, cc->rbgeom.y,
			    cc->rbgeom.w, cc->rbgeom.h,
			    cc->geom.x, cc->geom.y,
			    cc->geom.w, cc->geom.h);
			if (probe_ids)
				(void)printf(" id %lu",
				    (unsigned long)cc->win);
			(void)printf("\n");
			j++;
		}
		i++;
	}
	(void)printf("end\n");
}

/*
 * A whole scenario: build the ribbon the arguments describe out of the real
 * structures, run the real scroll over it, and report where every window
 * ended up - in ribbon coordinates and on the screen both.
 *
 * Both are printed because they answer different questions.  The invariant
 * "opening a window changes no existing window" is a statement about ribbon
 * coordinates; screen coordinates necessarily move when the viewport
 * scrolls, which is the entire point of a scrollable ribbon.  A harness that
 * checked the invariant against screen coordinates would be testing the
 * opposite of what was promised.
 *
 * With "insert=column" or "insert=stack" the scenario runs on: a window is
 * handed to ribbon_insert(), the same call the MapRequest handler makes, and
 * the state after it is printed as a second stage.
 *
 * "reorder=up|down" and "swap=left|right" carry it on the same way: the first
 * moves the focused window one place along its own stack, the second
 * exchanges the focused column with its neighbour, each through the model
 * call its command makes and each printing a stage of its own.
 */
static int
probe_layout(struct probe_ctx *p)
{
	struct screen_ctx	 sc;
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc, *ccnxt;
	const char		*place, *dir;
	int			 cols[PROBE_MAXCOL], presets[PROBE_MAXCOL];
	int			 widths[RIBBON_NPRESET];
	int			 vw = 0, vh = 0, rw = 0, rh = 0;
	int			 border, focus, fwin, ncol, npreset, nwidth;
	int			 i, j, resized, flags, nid = 0;

	if (probe_dim(p, "viewport", &vw, &vh) != 1) {
		probe_error(p, "missing field \"viewport\"");
		return -1;
	}

	probe_ids = probe_opt(p, "ids", 0);
	Conf.ribbongap = probe_opt(p, "gap", Conf.ribbongap);
	Conf.ribbonminw = probe_opt(p, "min-width", Conf.ribbonminw);
	Conf.ribbonminh = probe_opt(p, "min-height", Conf.ribbonminh);
	border = probe_opt(p, "border", Conf.bwidth);

	nwidth = probe_list(p, "widths", widths, RIBBON_NPRESET);
	for (i = 0; i < nwidth; i++)
		Conf.ribbonwidth[i] = widths[i];

	ncol = probe_list(p, "columns", cols, PROBE_MAXCOL);
	npreset = probe_list(p, "presets", presets, PROBE_MAXCOL);
	if (p->fail)
		return -1;
	if (ncol <= 0) {
		probe_error(p, "missing field \"columns\"");
		return -1;
	}

	(void)memset(&sc, 0, sizeof(sc));
	TAILQ_INIT(&sc.ribbonq);

	rb = ribbon_new(&sc, "probe");
	rb->view.x = probe_opt(p, "viewport-x", 0);
	rb->view.y = probe_opt(p, "viewport-y", 0);
	rb->view.w = vw;
	rb->view.h = vh;
	rb->active = 1;

	for (i = 0; i < ncol; i++) {
		col = ribbon_col_new(rb, NULL);
		col->preset = (i < npreset) ? presets[i] : 1;
		for (j = 0; j < cols[i]; j++) {
			cc = xcalloc(1, sizeof(*cc));
			cc->sc = &sc;
			cc->bwidth = border;
			cc->win = ++nid;
			ribbon_col_add(col, cc);
		}
	}

	focus = probe_opt(p, "focus", ncol - 1);
	rb->focus = ribbon_col_at(rb, focus);
	rb->offset = probe_opt(p, "offset", 0);
	rb->voffset = probe_opt(p, "voffset", 0);

	/*
	 * Which window of the focused column has the focus - the one the
	 * vertical axis follows.  Left alone it is the last window added, which
	 * is what ribbon_col_add() leaves behind and therefore what a freshly
	 * built ribbon really looks like; "focus-window" is for scenarios that
	 * want to look at some other window of the stack.
	 */
	fwin = probe_opt(p, "focus-window", -1);
	if ((rb->focus != NULL) && (fwin >= 0)) {
		j = 0;
		TAILQ_FOREACH(cc, &rb->focus->winq, rbentry) {
			if (j++ == fwin)
				rb->focus->focus = cc;
		}
	}
	if (p->fail)
		goto done;

	ribbon_scroll(rb);

	/*
	 * An optional second viewport replays what happens when RandR
	 * resizes the output under a ribbon that already exists - the same
	 * arithmetic ribbon_screen_update() performs, without the X calls.
	 */
	resized = probe_dim(p, "resize", &rw, &rh);
	if (resized < 0)
		goto done;
	if (resized == 1) {
		rb->view.w = rw;
		rb->view.h = rh;
		ribbon_measure(rb);
		rb->offset = ribbon_policy_output(rb->view.w, rb->offset,
		    rb->len);
		rb->voffset = ribbon_policy_output(rb->view.h, rb->voffset,
		    rb->canvas);
		ribbon_scroll(rb);
	}

	(void)printf("ok layout\n");
	probe_report(rb, border, "initial");

	/*
	 * The insertion itself, through the same call the MapRequest handler
	 * makes.  "column" and "stack" name the two places the insertion
	 * policy can return for a window the ribbon keeps; a policy answer of
	 * "float" leaves the ribbon alone and is therefore nothing to print.
	 */
	if ((place = probe_get(p, "insert")) != NULL) {
		cc = xcalloc(1, sizeof(*cc));
		cc->sc = &sc;
		cc->bwidth = border;
		cc->win = ++nid;

		if (strcmp(place, "column") == 0)
			col = ribbon_insert(rb, RIBBON_PLACE_COLUMN, cc);
		else if (strcmp(place, "stack") == 0)
			col = ribbon_insert(rb, RIBBON_PLACE_STACK, cc);
		else {
			free(cc);
			probe_error(p, "insert wants column or stack, got %s",
			    place);
			goto done;
		}

		if (col == NULL) {
			free(cc);
			probe_error(p, "the ribbon declined the window");
			goto done;
		}
		probe_report(rb, border, place);
	}

	/*
	 * The two reorders, through the very model calls the commands make.
	 * Stages of a scenario rather than utilities of their own, because
	 * what they answer is a whole layout and not a number: there is no
	 * scalar decision in either to write down as an FTS model, and the
	 * state before and next to the state after is the whole of the
	 * answer.  A window already at the end of its stack, or a column
	 * already at the end of the ribbon, prints a stage identical to the
	 * one before it - which is what "it stays where it is" looks like
	 * when it is printed rather than asserted.
	 */
	if ((dir = probe_get(p, "reorder")) != NULL) {
		if (strcmp(dir, "up") == 0)
			flags = CWM_UP;
		else if (strcmp(dir, "down") == 0)
			flags = CWM_DOWN;
		else {
			probe_error(p, "reorder wants up or down, got %s", dir);
			goto done;
		}
		if (rb->focus == NULL) {
			probe_error(p, "reorder wants a focused column");
			goto done;
		}
		if ((cc = rb->focus->focus) == NULL)
			cc = TAILQ_FIRST(&rb->focus->winq);
		if (cc == NULL) {
			probe_error(p, "the focused column has no window");
			goto done;
		}

		(void)ribbon_stack_reorder(rb->focus, cc, flags);
		rb->focus->focus = cc;
		ribbon_scroll(rb);
		probe_report(rb, border, "reorder");
	}

	if ((dir = probe_get(p, "swap")) != NULL) {
		if (strcmp(dir, "left") == 0)
			flags = CWM_LEFT;
		else if (strcmp(dir, "right") == 0)
			flags = CWM_RIGHT;
		else {
			probe_error(p, "swap wants left or right, got %s", dir);
			goto done;
		}
		if (rb->focus == NULL) {
			probe_error(p, "swap wants a focused column");
			goto done;
		}

		(void)ribbon_col_reorder(rb, rb->focus, flags);
		ribbon_scroll(rb);
		probe_report(rb, border, "swap");
	}

done:
	TAILQ_FOREACH(col, &rb->colq, entry) {
		TAILQ_FOREACH_SAFE(cc, &col->winq, rbentry, ccnxt) {
			TAILQ_REMOVE(&col->winq, cc, rbentry);
			free(cc);
		}
	}
	ribbon_free(rb);

	return p->fail ? -1 : 0;
}

/*
 * A list of outputs, "NAME:WxH+X+Y" separated by commas - the shape xrandr
 * prints, so that a scenario can be read off a real desktop.
 */
static int
probe_outs(struct probe_ctx *p, const char *key, struct probe_out *out, int max)
{
	char		 buf[PROBE_VALLEN];
	const char	*v;
	char		*tok, *save, *colon, *x, *plus1, *plus2;
	int		 n = 0;

	if ((v = probe_get(p, key)) == NULL)
		return 0;

	(void)strlcpy(buf, v, sizeof(buf));
	for (tok = strtok_r(buf, ",", &save); tok != NULL;
	    tok = strtok_r(NULL, ",", &save)) {
		if (n >= max) {
			probe_error(p, "field \"%s\" has too many outputs", key);
			return n;
		}
		if ((colon = strchr(tok, ':')) == NULL) {
			probe_error(p, "output \"%s\" wants NAME:WxH+X+Y", tok);
			return n;
		}
		*colon = '\0';
		if (strlcpy(out[n].name, tok, sizeof(out[n].name)) >=
		    sizeof(out[n].name)) {
			probe_error(p, "output name \"%s\" too long", tok);
			return n;
		}
		tok = colon + 1;

		if (((x = strchr(tok, 'x')) == NULL) ||
		    ((plus1 = strchr(tok, '+')) == NULL) ||
		    ((plus2 = strchr(plus1 + 1, '+')) == NULL)) {
			probe_error(p, "output \"%s\" wants WxH+X+Y",
			    out[n].name);
			return n;
		}
		*x = *plus1 = *plus2 = '\0';
		out[n].w = probe_value(p, key, tok);
		out[n].h = probe_value(p, key, x + 1);
		out[n].x = probe_value(p, key, plus1 + 1);
		out[n].y = probe_value(p, key, plus2 + 1);
		if (p->fail)
			return n;
		n++;
	}
	return n;
}

/* Replace the region list, the way screen_update_geometry() does after RandR. */
static void
probe_regions(struct screen_ctx *sc, struct probe_out *out, int nout)
{
	struct region_ctx	*rc;
	int			 i;

	probe_regions_free(sc);

	for (i = 0; i < nout; i++) {
		rc = xcalloc(1, sizeof(*rc));
		rc->num = i;
		rc->name = xstrdup(out[i].name);
		rc->view.x = out[i].x;
		rc->view.y = out[i].y;
		rc->view.w = out[i].w;
		rc->view.h = out[i].h;
		rc->work = rc->view;
		TAILQ_INSERT_TAIL(&sc->regionq, rc, entry);
	}
}

static void
probe_regions_free(struct screen_ctx *sc)
{
	struct region_ctx	*rc;

	while ((rc = TAILQ_FIRST(&sc->regionq)) != NULL) {
		TAILQ_REMOVE(&sc->regionq, rc, entry);
		free(rc->name);
		free(rc);
	}
}

/* Every ribbon of the screen, attached or not, with everything it holds. */
static void
probe_outputs_report(struct screen_ctx *sc, const char *stage)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 i, j;

	(void)printf("stage %s\n", stage);
	TAILQ_FOREACH(rb, &sc->ribbonq, entry) {
		(void)printf("ribbon %s active %d view %d %d %d %d "
		    "length %d offset %d columns %d focus %d "
		    "canvas %d voffset %d\n",
		    rb->output, rb->active, rb->view.x, rb->view.y,
		    rb->view.w, rb->view.h, rb->len, rb->offset,
		    ribbon_col_count(rb), ribbon_col_index(rb, rb->focus),
		    rb->canvas, rb->voffset);

		i = 0;
		TAILQ_FOREACH(col, &rb->colq, entry) {
			(void)printf("column %s %d ribbon-x %d width %d "
			    "preset %d windows %d height %d\n", rb->output, i,
			    col->x, col->w, col->preset, col->nwin, col->h);
			j = 0;
			TAILQ_FOREACH(cc, &col->winq, rbentry) {
				(void)printf("window %s %d %d ribbon %d %d %d "
				    "%d\n", rb->output, i, j, cc->rbgeom.x,
				    cc->rbgeom.y, cc->rbgeom.w, cc->rbgeom.h);
				j++;
			}
			i++;
		}
	}
	(void)printf("end\n");
}

/*
 * A monitor coming and going.  "outputs" names the RandR outputs present at
 * the start, "then" and "after" the ones present at the second and third
 * stage; every stage runs through ribbon_screen_relayout(), the same call the
 * RRScreenChangeNotify handler makes once the X part is stripped off it.
 *
 * What is being asked: does a ribbon survive its monitor being unplugged, does
 * it come back as it was, and does anything ever cross from one ribbon to
 * another.  The answer is printed, not asserted, because the assertion belongs
 * in the harness where it can be read.
 */
static int
probe_outputs(struct probe_ctx *p)
{
	struct screen_ctx	 sc;
	struct probe_out	 outs[8], then[8], after[8];
	struct ribbon		*rb, *rbnxt;
	struct ribbon_col	*col;
	struct client_ctx	*cc, *ccnxt;
	char			 buf[PROBE_VALLEN];
	const char		*v;
	char			*tok, *save, *colon, *spec, *save2;
	int			 nout, nthen, nafter, i, n;

	if ((nout = probe_outs(p, "outputs", outs, nitems(outs))) <= 0) {
		if (!p->fail)
			probe_error(p, "missing field \"outputs\"");
		return -1;
	}
	nthen = probe_outs(p, "then", then, nitems(then));
	nafter = probe_outs(p, "after", after, nitems(after));
	if (p->fail)
		return -1;

	Conf.ribbongap = probe_opt(p, "gap", Conf.ribbongap);
	Conf.ribbonminw = probe_opt(p, "min-width", Conf.ribbonminw);
	Conf.ribbonminh = probe_opt(p, "min-height", Conf.ribbonminh);

	(void)memset(&sc, 0, sizeof(sc));
	TAILQ_INIT(&sc.ribbonq);
	TAILQ_INIT(&sc.regionq);

	probe_regions(&sc, outs, nout);
	ribbon_screen_relayout(&sc);

	/*
	 * Columns per output: "HDMI-1:2.1.3" is three columns on HDMI-1, with
	 * two, one and three windows in them.
	 */
	if ((v = probe_get(p, "columns")) != NULL) {
		(void)strlcpy(buf, v, sizeof(buf));
		for (tok = strtok_r(buf, ",", &save); tok != NULL;
		    tok = strtok_r(NULL, ",", &save)) {
			if ((colon = strchr(tok, ':')) == NULL) {
				probe_error(p, "columns want NAME:n.n.n");
				goto done;
			}
			*colon = '\0';
			if ((rb = ribbon_find(&sc, tok)) == NULL) {
				probe_error(p, "no output named \"%s\"", tok);
				goto done;
			}
			for (spec = strtok_r(colon + 1, ".", &save2);
			    spec != NULL;
			    spec = strtok_r(NULL, ".", &save2)) {
				n = probe_value(p, "columns", spec);
				if (p->fail)
					goto done;
				col = ribbon_col_new(rb, NULL);
				for (i = 0; i < n; i++) {
					cc = xcalloc(1, sizeof(*cc));
					cc->sc = &sc;
					ribbon_col_add(col, cc);
				}
			}
			rb->focus = TAILQ_FIRST(&rb->colq);
		}
	}

	/* Focus and offset per output, both optional: "HDMI-1:2". */
	if ((v = probe_get(p, "focus")) != NULL) {
		(void)strlcpy(buf, v, sizeof(buf));
		for (tok = strtok_r(buf, ",", &save); tok != NULL;
		    tok = strtok_r(NULL, ",", &save)) {
			if (((colon = strchr(tok, ':')) == NULL) ||
			    ((*colon = '\0'), (rb = ribbon_find(&sc, tok)) == NULL)) {
				probe_error(p, "focus wants NAME:index");
				goto done;
			}
			rb->focus = ribbon_col_at(rb,
			    probe_value(p, "focus", colon + 1));
		}
	}
	if ((v = probe_get(p, "offset")) != NULL) {
		(void)strlcpy(buf, v, sizeof(buf));
		for (tok = strtok_r(buf, ",", &save); tok != NULL;
		    tok = strtok_r(NULL, ",", &save)) {
			if (((colon = strchr(tok, ':')) == NULL) ||
			    ((*colon = '\0'), (rb = ribbon_find(&sc, tok)) == NULL)) {
				probe_error(p, "offset wants NAME:pixels");
				goto done;
			}
			rb->offset = probe_value(p, "offset", colon + 1);
		}
	}
	/* The same for the other axis: "HDMI-1:400". */
	if ((v = probe_get(p, "voffset")) != NULL) {
		(void)strlcpy(buf, v, sizeof(buf));
		for (tok = strtok_r(buf, ",", &save); tok != NULL;
		    tok = strtok_r(NULL, ",", &save)) {
			if (((colon = strchr(tok, ':')) == NULL) ||
			    ((*colon = '\0'), (rb = ribbon_find(&sc, tok)) == NULL)) {
				probe_error(p, "voffset wants NAME:pixels");
				goto done;
			}
			rb->voffset = probe_value(p, "voffset", colon + 1);
		}
	}
	if (p->fail)
		goto done;

	(void)printf("ok outputs\n");
	ribbon_screen_relayout(&sc);
	probe_outputs_report(&sc, "outputs");

	if (nthen > 0) {
		probe_regions(&sc, then, nthen);
		ribbon_screen_relayout(&sc);
		probe_outputs_report(&sc, "then");
	}
	if (nafter > 0) {
		probe_regions(&sc, after, nafter);
		ribbon_screen_relayout(&sc);
		probe_outputs_report(&sc, "after");
	}

done:
	TAILQ_FOREACH_SAFE(rb, &sc.ribbonq, entry, rbnxt) {
		TAILQ_FOREACH(col, &rb->colq, entry) {
			TAILQ_FOREACH_SAFE(cc, &col->winq, rbentry, ccnxt) {
				TAILQ_REMOVE(&col->winq, cc, rbentry);
				free(cc);
			}
		}
		TAILQ_REMOVE(&sc.ribbonq, rb, entry);
		ribbon_free(rb);
	}
	probe_regions_free(&sc);

	return p->fail ? -1 : 0;
}

/*
 * Does this -c argument name a control command rather than a config file?
 * Upstream cwm spells the config file -c, and digitwm keeps that; a command
 * is told apart by its verb, so both spellings work and neither has to be
 * explained away.  -C is the unambiguous form.
 */
int
probe_is_command(const char *s)
{
	size_t	 n = strlen(PROBE_VERB);

	while ((*s == ' ') || (*s == '\t'))
		s++;
	if (strncmp(s, PROBE_VERB, n) != 0)
		return 0;

	return ((s[n] == '\0') || (s[n] == ' ') || (s[n] == '\t'));
}

/*
 * Pull the utility name off the front of s.  It is either bare, or wrapped
 * in the guillemets FTS uses, or in plain double quotes; the wrapped forms
 * are the ones that may hold spaces.  Returns the name and leaves s on the
 * first character after it.
 */
static char *
probe_name(struct probe_ctx *p, char **s)
{
	char	*name, *end;

	while ((**s == ' ') || (**s == '\t'))
		(*s)++;

	if (strncmp(*s, PROBE_LQUOT, strlen(PROBE_LQUOT)) == 0) {
		name = *s + strlen(PROBE_LQUOT);
		if ((end = strstr(name, PROBE_RQUOT)) == NULL) {
			probe_error(p, "unterminated utility name");
			return NULL;
		}
		*end = '\0';
		*s = end + strlen(PROBE_RQUOT);
	} else if (**s == '"') {
		name = *s + 1;
		if ((end = strchr(name, '"')) == NULL) {
			probe_error(p, "unterminated utility name");
			return NULL;
		}
		*end = '\0';
		*s = end + 1;
	} else {
		name = *s;
		end = name;
		while ((*end != '\0') && (*end != ' ') && (*end != '\t'))
			end++;
		if (*end != '\0')
			*end++ = '\0';
		*s = end;
	}

	if (*name == '\0') {
		probe_error(p, "%s wants a utility name", PROBE_VERB);
		return NULL;
	}
	return name;
}

int
probe_run(const char *cmd)
{
	struct probe_ctx	 p;
	char			*buf, *s, *util;
	unsigned int		 i;
	int			 rv = -1;

	(void)memset(&p, 0, sizeof(p));
	s = buf = xstrdup(cmd);

	if ((util = probe_name(&p, &s)) == NULL)
		goto fail;
	if (strcmp(util, PROBE_VERB) != 0) {
		probe_error(&p, "unknown command \"%s\"", util);
		goto fail;
	}
	if ((util = probe_name(&p, &s)) == NULL)
		goto fail;

	/* Accept either surface's name for the model. */
	for (i = 0; i < nitems(probe_utils); i++) {
		if (strcmp(util, probe_utils[i].alias) == 0) {
			util = (char *)probe_utils[i].name;
			break;
		}
	}

	if (probe_parse(&p, s) == -1)
		goto fail;

	if (strcmp(util, "layout") == 0)
		rv = probe_layout(&p);
	else if (strcmp(util, "outputs") == 0)
		rv = probe_outputs(&p);
	else
		rv = probe_scalar(&p, util);

	if (rv == 0) {
		free(buf);
		return 0;
	}
fail:
	warnx("error %s", p.fail ? p.msg : "malformed command");
	free(buf);
	return 1;
}
