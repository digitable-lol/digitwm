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
 * Six utilities carry the names of the FTS models, in either surface:
 *
 *   scroll-offset       "Смещение ленты после фокуса"
 *   column-width        "Ширина колонки по пресету"
 *   window-height       "Высота окна в колонке"
 *   insertion           "Куда вставить окно"
 *   focus-after-close   "Фокус после закрытия"
 *   output-change       "Смещение после смены монитора"
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
static int		 probe_layout(struct probe_ctx *);

/* The FTS models exist on two surfaces; both names mean the same utility. */
static const struct {
	const char	*name;
	const char	*alias;
} probe_utils[] = {
	{ "scroll-offset",	"Смещение ленты после фокуса" },
	{ "column-width",	"Ширина колонки по пресету" },
	{ "window-height",	"Высота окна в колонке" },
	{ "insertion",		"Куда вставить окно" },
	{ "focus-after-close",	"Фокус после закрытия" },
	{ "output-change",	"Смещение после смены монитора" },
	{ "layout",		"Раскладка" },
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
 * The five scalar utilities plus output-change.  Each one is a single call
 * into the same ribbon_policy_* function the window manager uses.
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
	} else {
		probe_error(p, "unknown utility \"%s\"", util);
	}

	if (p->fail)
		return -1;

	(void)printf("ok %s %d\n", util, v);
	return 0;
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
 */
static int
probe_layout(struct probe_ctx *p)
{
	struct screen_ctx	 sc;
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc, *ccnxt;
	int			 cols[PROBE_MAXCOL], presets[PROBE_MAXCOL];
	int			 widths[RIBBON_NPRESET];
	int			 vw = 0, vh = 0, rw = 0, rh = 0;
	int			 border, focus, ncol, npreset, nwidth;
	int			 i, j, resized;

	if (probe_dim(p, "viewport", &vw, &vh) != 1) {
		probe_error(p, "missing field \"viewport\"");
		return -1;
	}

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
			ribbon_col_add(col, cc);
		}
	}

	focus = probe_opt(p, "focus", ncol - 1);
	rb->focus = ribbon_col_at(rb, focus);
	rb->offset = probe_opt(p, "offset", 0);
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
		ribbon_scroll(rb);
	}

	(void)printf("ok layout\n");
	(void)printf("viewport %d %d %d %d\n", rb->view.x, rb->view.y,
	    rb->view.w, rb->view.h);
	(void)printf("gap %d\n", Conf.ribbongap);
	(void)printf("border %d\n", border);
	(void)printf("ribbon length %d offset %d columns %d focus %d\n",
	    rb->len, rb->offset, ribbon_col_count(rb),
	    ribbon_col_index(rb, rb->focus));

	i = 0;
	TAILQ_FOREACH(col, &rb->colq, entry) {
		(void)printf("column %d ribbon-x %d width %d preset %d "
		    "windows %d\n", i, col->x, col->w, col->preset, col->nwin);
		j = 0;
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			(void)printf("window %d %d ribbon %d %d %d %d "
			    "screen %d %d %d %d\n", i, j,
			    cc->rbgeom.x, cc->rbgeom.y,
			    cc->rbgeom.w, cc->rbgeom.h,
			    cc->geom.x, cc->geom.y,
			    cc->geom.w, cc->geom.h);
			j++;
		}
		i++;
	}
	(void)printf("end\n");

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
