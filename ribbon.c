/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - scrollable tiling for the calm window manager
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
 * Windows live on an endless row of columns; the screen is a viewport
 * sliding along it.  Two invariants drive everything here:
 *
 *   - inserting a window never alters the ribbon geometry of a window that
 *     is already on the ribbon.  Neighbours are pushed along, never squeezed;
 *   - after any event the focused column lies wholly inside the viewport
 *     horizontally.  Only horizontally: the ribbon scrolls sideways and
 *     nothing scrolls it vertically, so a column whose windows do not fit the
 *     viewport height sticks out below it (see ribbon_policy_height).
 *
 * The scalar decisions - how far to scroll, how wide a column is, how tall a
 * window in it is, where a new window goes, what gets focus after a close -
 * are the ribbon_policy_* functions below.  They take and return plain
 * numbers, touch no state and call no X function, because the same policy is
 * written a second time as an FTS model and the two are compared vector by
 * vector in CI through "layout-probe".  Everything that needs more than
 * numbers - the loop over columns, the X calls - is deliberately kept out
 * of them.
 */

#include <sys/types.h>
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static void		 ribbon_col_free(struct ribbon *, struct ribbon_col *);
static void		 ribbon_col_del(struct client_ctx *);
static int		 ribbon_col_visible(struct ribbon *,
			     struct ribbon_col *);
static void		 ribbon_sync_one(struct ribbon *);
static void		 ribbon_activate(struct ribbon_col *);
static void		 ribbon_warp(struct ribbon *);
static void		 ribbon_settle(void);

/*
 * Offset of the viewport once the focused column must be visible.
 *
 * The gap doubles as the margin kept between the focused column and the edge
 * of the viewport, so that scrolling leaves a hint of the neighbour rather
 * than butting the column against the border.  It is dropped when the column
 * is too wide to afford it.
 */
int
ribbon_policy_offset(int vw, int cl, int cw, int off, int gap, int len)
{
	int	 margin, max;

	margin = gap;
	if ((cw + margin) > vw)
		margin = 0;

	if (cw >= vw)
		off = cl;
	else if ((cl - margin) < off)
		off = cl - margin;
	else if ((cl + cw + margin) > (off + vw))
		off = cl + cw + margin - vw;

	max = len - vw;
	if (max < 0)
		max = 0;
	if (off > max)
		off = max;
	if (off < 0)
		off = 0;

	return off;
}

/*
 * Width of a column from its preset.  A preset is a percentage of the
 * viewport; the gap is taken off first so that two half-width columns and
 * the gap between them come to exactly one viewport.  Full width is the
 * whole viewport, gap and all, since nothing sits beside it.
 */
int
ribbon_policy_width(int vw, int preset, int gap, int minw)
{
	int	 pct, w, max;

	if (preset < 0)
		preset = 0;
	if (preset >= RIBBON_NPRESET)
		preset = RIBBON_NPRESET - 1;

	pct = Conf.ribbonwidth[preset];
	if (pct >= 100)
		w = vw;
	else
		w = ((vw - gap) * pct) / 100;

	/*
	 * Both properties of the model hold - width no larger than the
	 * viewport, width no smaller than the minimum - as long as the
	 * viewport itself is not narrower than the minimum.  When it is,
	 * the minimum wins and the column simply scrolls.
	 */
	max = MAX(vw, minw);
	if (w > max)
		w = max;
	if (w < minw)
		w = minw;

	return w;
}

/*
 * Height of window number idx of nwin in a column.  The remainder of the
 * division goes to the last window, so a column fills the viewport exactly
 * instead of leaving a stray pixel row at the bottom - for as long as the equal
 * share clears the minimum height.
 *
 * Once it does not, the minimum wins: it is a declared property of the model
 * ("height is at least the minimum", fts/window-height.fts), and answering with
 * less would break the model's own property.  So the stack runs past the bottom
 * edge instead - nwin windows of minh plus the gaps.  Unlike the same collision
 * in ribbon_policy_width(), which the horizontal scroll resolves, there is no
 * vertical scroll and ribbon_col_visible() only tests the x extent: whatever
 * ends up below the edge is neither parked nor reachable.  Measured at
 * 1280x800, gap 8, minh 60: 11 windows fit, 12 overflow by 11px, 20 overflow by
 * 552px with the last 8 entirely below the edge.  Which side should give is a
 * product decision and is deliberately left open here; the harness pins the
 * arithmetic of both cases so the exception cannot go unnoticed.
 *
 * The empty column takes the same lower bound as every other answer.  It used
 * to return vh outright and skip both clamps, which quietly broke the model's
 * property "height is at least the minimum" whenever the viewport was shorter
 * than ribbonminh; MAX(vh, minh) is what the rest of the function does, and
 * what ribbon_policy_width() does with a viewport narrower than its minimum.
 */
int
ribbon_policy_height(int vh, int nwin, int idx, int gap, int minh)
{
	int	 total, base, h;

	if (nwin <= 0)
		return MAX(vh, minh);

	total = vh - (gap * (nwin - 1));
	base = total / nwin;

	h = base;
	if (idx == (nwin - 1))
		h = total - (base * (nwin - 1));

	if (h > vh)
		h = vh;
	if (h < minh)
		h = minh;

	return h;
}

/*
 * Where a newly mapped window goes.  The conditions are deliberately
 * non-overlapping: the FTS model runs every rule whose condition holds and
 * lets the last one win, so overlapping conditions there would be a trap.
 */
int
ribbon_policy_insert(int hasfocus, int transient, int dialog, int dock,
    int full, int rule)
{
	if (rule == RIBBON_RULE_FLOAT)
		return RIBBON_PLACE_FLOAT;
	if (dock)
		return RIBBON_PLACE_FLOAT;
	if (dialog || transient)
		return RIBBON_PLACE_FLOAT;
	if (full)
		return RIBBON_PLACE_FULL;
	if ((rule == RIBBON_RULE_STACK) && hasfocus)
		return RIBBON_PLACE_STACK;

	return RIBBON_PLACE_COLUMN;
}

/*
 * Do a panel's strut and one region meet at all?  A strut names the stretch of
 * a screen edge it claims - span0 to span1, both ends inside the claim - and a
 * region occupies pos to pos + len along that same edge.  A panel on the left
 * monitor must not shorten the right one, and this is the whole of that rule.
 *
 * Ends touching count as meeting: a strut ending exactly where a region begins
 * shares that one pixel column with it.
 */
int
ribbon_policy_span(int span0, int span1, int pos, int len)
{
	if (span1 < span0)
		return 0;
	if (len <= 0)
		return 0;

	return ((span0 <= (pos + len - 1)) && (span1 >= pos));
}

/*
 * How many pixels a strut takes off one edge of one region.  strut is the
 * depth the panel claims measured from the screen edge, screen the size of the
 * screen along that axis, pos and len the region's own extent along it; far
 * says the edge in question is the far one (bottom or right) rather than the
 * near one (top or left).
 *
 * The region is passed in already reduced by the configured gap, so a panel
 * that fits inside a gap the user has already given away costs nothing more.
 * That is deliberate: "gap 40 0 0 0" plus a panel 28 tall means 40 taken, not
 * 68.  Nothing can be taken twice, and nothing can take more than the region
 * has - a strut deeper than the whole region leaves it empty rather than
 * negative.
 */
int
ribbon_policy_reserve(int strut, int screen, int pos, int len, int far)
{
	int	 n;

	if (strut <= 0)
		return 0;

	if (far)
		n = (pos + len) - (screen - strut);
	else
		n = strut - pos;

	if (n < 0)
		n = 0;
	if (n > len)
		n = len;

	return n;
}

/*
 * Which column holds focus after the window in column idx closed.  ncol is
 * the number of columns before the close, last says the column was the
 * rightmost, only says the window was the last one in it - that is, that the
 * column goes away with it.
 */
int
ribbon_policy_close(int idx, int ncol, int last, int only)
{
	int	 n;

	if (!only)
		return idx;

	n = ncol - 1;
	if (n <= 0)
		return 0;
	if (last || (idx >= n))
		return n - 1;

	return idx;
}

/* Offset after the viewport changed size under it. */
int
ribbon_policy_output(int vw, int off, int len)
{
	int	 max;

	max = len - vw;
	if (max < 0)
		max = 0;
	if (off > max)
		off = max;
	if (off < 0)
		off = 0;

	return off;
}

struct ribbon *
ribbon_new(struct screen_ctx *sc, const char *output)
{
	struct ribbon	*rb;

	rb = xcalloc(1, sizeof(*rb));
	rb->sc = sc;
	rb->output = xstrdup(output);
	rb->focus = NULL;
	rb->offset = 0;
	rb->len = 0;
	rb->active = 0;
	TAILQ_INIT(&rb->colq);

	if (sc != NULL)
		TAILQ_INSERT_TAIL(&sc->ribbonq, rb, entry);

	return rb;
}

void
ribbon_free(struct ribbon *rb)
{
	struct ribbon_col	*col;

	while ((col = TAILQ_FIRST(&rb->colq)) != NULL) {
		TAILQ_REMOVE(&rb->colq, col, entry);
		free(col);
	}
	free(rb->output);
	free(rb);
}

struct ribbon *
ribbon_find(struct screen_ctx *sc, const char *output)
{
	struct ribbon	*rb;

	if (output == NULL)
		return NULL;

	TAILQ_FOREACH(rb, &sc->ribbonq, entry) {
		if (strcmp(rb->output, output) == 0)
			return rb;
	}
	return NULL;
}

/*
 * The ribbon a command applies to: the one holding the active window, else
 * the one under the pointer, else the first attached one.
 */
struct ribbon *
ribbon_current(struct screen_ctx *sc)
{
	struct client_ctx	*cc;
	struct region_ctx	*rc;
	struct ribbon		*rb;
	int			 x, y;

	if (((cc = client_current(sc)) != NULL) && (cc->rbcol != NULL))
		return cc->rbcol->rb;

	xu_ptr_get(sc->rootwin, &x, &y);
	if (((rc = region_find(sc, x, y)) != NULL) &&
	    ((rb = ribbon_find(sc, rc->name)) != NULL) && rb->active)
		return rb;

	TAILQ_FOREACH(rb, &sc->ribbonq, entry) {
		if (rb->active)
			return rb;
	}
	return NULL;
}

struct ribbon_col *
ribbon_col_new(struct ribbon *rb, struct ribbon_col *after)
{
	struct ribbon_col	*col;

	col = xcalloc(1, sizeof(*col));
	col->rb = rb;
	col->nwin = 0;
	col->focus = NULL;
	col->preset = 1;
	TAILQ_INIT(&col->winq);

	if (after != NULL)
		TAILQ_INSERT_AFTER(&rb->colq, after, col, entry);
	else
		TAILQ_INSERT_TAIL(&rb->colq, col, entry);

	return col;
}

static void
ribbon_col_free(struct ribbon *rb, struct ribbon_col *col)
{
	TAILQ_REMOVE(&rb->colq, col, entry);
	if (rb->focus == col)
		rb->focus = NULL;
	free(col);
}

struct ribbon_col *
ribbon_col_at(struct ribbon *rb, int idx)
{
	struct ribbon_col	*col;
	int			 i = 0;

	TAILQ_FOREACH(col, &rb->colq, entry) {
		if (i++ == idx)
			return col;
	}
	return NULL;
}

int
ribbon_col_index(struct ribbon *rb, struct ribbon_col *col)
{
	struct ribbon_col	*ci;
	int			 i = 0;

	TAILQ_FOREACH(ci, &rb->colq, entry) {
		if (ci == col)
			return i;
		i++;
	}
	return -1;
}

int
ribbon_col_count(struct ribbon *rb)
{
	struct ribbon_col	*col;
	int			 n = 0;

	TAILQ_FOREACH(col, &rb->colq, entry)
		n++;

	return n;
}

void
ribbon_col_add(struct ribbon_col *col, struct client_ctx *cc)
{
	TAILQ_INSERT_TAIL(&col->winq, cc, rbentry);
	col->nwin++;
	col->focus = cc;
	cc->rbcol = col;
	cc->flags |= CLIENT_RIBBON;
}

/* Take a window out of its column, dropping the column if it empties. */
static void
ribbon_col_del(struct client_ctx *cc)
{
	struct ribbon_col	*col = cc->rbcol;

	TAILQ_REMOVE(&col->winq, cc, rbentry);
	col->nwin--;
	if (col->focus == cc)
		col->focus = TAILQ_FIRST(&col->winq);

	cc->rbcol = NULL;
	cc->flags &= ~CLIENT_RIBBON;
}

/*
 * Widths and ribbon positions of every column, and with them the length of
 * the ribbon.  Independent of the offset, so insertion can measure without
 * having decided yet where to scroll.
 */
void
ribbon_measure(struct ribbon *rb)
{
	struct ribbon_col	*col;
	int			 x = 0;

	TAILQ_FOREACH(col, &rb->colq, entry) {
		col->w = ribbon_policy_width(rb->view.w, col->preset,
		    Conf.ribbongap, Conf.ribbonminw);
		col->x = x;
		x += col->w + Conf.ribbongap;
	}

	rb->len = (x > 0) ? (x - Conf.ribbongap) : 0;
}

/*
 * Screen geometry of every window from the measured columns and the current
 * offset.  Windows left of the viewport simply get a negative x: owning the
 * geometry is the whole reason this is a window manager and not a script
 * driving one.
 */
void
ribbon_place(struct ribbon *rb)
{
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 i, y, h;

	TAILQ_FOREACH(col, &rb->colq, entry) {
		i = 0;
		y = 0;
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			h = ribbon_policy_height(rb->view.h, col->nwin, i,
			    Conf.ribbongap, Conf.ribbonminh);

			cc->rbgeom.x = col->x;
			cc->rbgeom.y = y;
			cc->rbgeom.w = col->w;
			cc->rbgeom.h = h;

			/*
			 * A frozen or fullscreen window keeps the geometry it
			 * was given; it still holds its slot in the column, so
			 * the rest of the stack does not shuffle under it.
			 */
			if (!(cc->flags & (CLIENT_FREEZE | CLIENT_FULLSCREEN))) {
				cc->geom.x = rb->view.x + col->x - rb->offset;
				cc->geom.y = rb->view.y + y;
				cc->geom.w = MAX(1, col->w - (cc->bwidth * 2));
				cc->geom.h = MAX(1, h - (cc->bwidth * 2));
			}

			y += h + Conf.ribbongap;
			i++;
		}
	}
}

/* Measure, pull the viewport onto the focused column, place. */
void
ribbon_scroll(struct ribbon *rb)
{
	struct ribbon_col	*col = rb->focus;

	ribbon_measure(rb);

	if (col != NULL)
		rb->offset = ribbon_policy_offset(rb->view.w, col->x, col->w,
		    rb->offset, Conf.ribbongap, rb->len);
	else
		rb->offset = ribbon_policy_output(rb->view.w, rb->offset,
		    rb->len);

	ribbon_place(rb);
}

static int
ribbon_col_visible(struct ribbon *rb, struct ribbon_col *col)
{
	if ((col->x + col->w) <= rb->offset)
		return 0;
	if (col->x >= (rb->offset + rb->view.w))
		return 0;

	return 1;
}

static void
ribbon_sync_one(struct ribbon *rb)
{
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 show;

	TAILQ_FOREACH(col, &rb->colq, entry) {
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			show = rb->active;
			if (show && Conf.ribbonhide)
				show = ribbon_col_visible(rb, col);

			/*
			 * Only ever un-hide what the ribbon itself parked.  A
			 * window hidden because its group is hidden is not
			 * ours to bring back.
			 */
			if (!show) {
				if (!(cc->flags & CLIENT_HIDDEN)) {
					client_hide(cc);
					cc->flags |= CLIENT_RIBBON_PARKED;
				}
				continue;
			}
			if (cc->flags & CLIENT_RIBBON_PARKED) {
				if (cc->flags & CLIENT_HIDDEN)
					client_show(cc);
				cc->flags &= ~CLIENT_RIBBON_PARKED;
			}
			if (cc->flags & (CLIENT_HIDDEN | CLIENT_FREEZE |
			    CLIENT_FULLSCREEN))
				continue;

			client_resize(cc, 0);
		}
	}
}

/*
 * Swallow the crossing events the ribbon has just caused itself.  cwm gives
 * focus to whatever the pointer is over, and a window sliding under a
 * resting pointer is the ribbon talking to itself, not the user asking for
 * anything.  Left alone it oscillates: the scroll moves a window under the
 * pointer, the EnterNotify moves focus back, and the next scroll undoes the
 * first.
 */
static void
ribbon_settle(void)
{
	XEvent	 ev;

	XSync(X_Dpy, False);
	while (XCheckMaskEvent(X_Dpy, EnterWindowMask, &ev))
		;
}

/* Push the model onto the server for every ribbon of a screen. */
void
ribbon_sync(struct screen_ctx *sc)
{
	struct ribbon	*rb;

	TAILQ_FOREACH(rb, &sc->ribbonq, entry)
		ribbon_sync_one(rb);

	ribbon_settle();
}

void
ribbon_screen_init(struct screen_ctx *sc)
{
	TAILQ_INIT(&sc->ribbonq);
}

/*
 * Bind ribbons to RandR outputs by name, because the name is what survives a
 * cable being pulled - the CRTC index is not.  A ribbon whose output went
 * away keeps every column it had and is only marked detached; its windows
 * are parked until the output comes back.  Nothing is ever moved from one
 * ribbon to another, so two monitors never end up sharing a row.
 */
void
ribbon_screen_relayout(struct screen_ctx *sc)
{
	struct region_ctx	*rc;
	struct ribbon		*rb;

	TAILQ_FOREACH(rb, &sc->ribbonq, entry)
		rb->active = 0;

	TAILQ_FOREACH(rc, &sc->regionq, entry) {
		if (rc->name == NULL)
			continue;
		if ((rb = ribbon_find(sc, rc->name)) == NULL)
			rb = ribbon_new(sc, rc->name);

		rb->view = rc->work;
		rb->active = 1;

		ribbon_measure(rb);
		rb->offset = ribbon_policy_output(rb->view.w, rb->offset,
		    rb->len);
		ribbon_scroll(rb);
	}
}

/*
 * The whole of it: the arithmetic above, and then the X calls that push it
 * onto the server.  They are apart because a monitor coming and going is
 * worth proving, and everything worth proving is in the half that needs no
 * display: "layout-probe outputs" replays a hotplug through
 * ribbon_screen_relayout() and prints what became of every ribbon.
 */
void
ribbon_screen_update(struct screen_ctx *sc)
{
	ribbon_screen_relayout(sc);
	ribbon_sync(sc);
}

/*
 * Put a window on the ribbon at the place the insertion policy named.  A new
 * column is made after the focused one, so the ribbon grows where the eye
 * already is; the columns to the right of it are pushed along the ribbon and
 * none of them is resized, which is the invariant this window manager exists
 * for.
 *
 * Kept apart from ribbon_client_insert() because the invariant harness drives
 * it through "layout-probe": a proof about insertion is worth something only
 * if it exercises the code the MapRequest handler runs, and everything above
 * this line in that handler needs an X server.
 *
 * Returns the column the window landed in, NULL when the ribbon declined it.
 */
struct ribbon_col *
ribbon_insert(struct ribbon *rb, int place, struct client_ctx *cc)
{
	struct ribbon_col	*col;

	switch (place) {
	case RIBBON_PLACE_STACK:
		col = rb->focus;
		break;
	case RIBBON_PLACE_COLUMN:
		col = ribbon_col_new(rb, rb->focus);
		break;
	default:
		return NULL;
	}
	if (col == NULL)
		return NULL;

	ribbon_col_add(col, cc);
	rb->focus = col;
	ribbon_scroll(rb);

	return col;
}

/*
 * Decide the place of a freshly managed window and put it there.  Called
 * from client_init(), that is from within the MapRequest handler and before
 * the window is mapped, so that it appears where it belongs instead of
 * appearing and then jumping.
 *
 * Returns 1 when the ribbon took the window.
 */
int
ribbon_client_insert(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct ribbon		*rb;
	int			 place;

	if (!Conf.ribbon)
		return 0;
	if (cc->flags & (CLIENT_IGNORE | CLIENT_RIBBON))
		return 0;
	if ((rb = ribbon_current(sc)) == NULL)
		return 0;

	place = ribbon_policy_insert((rb->focus != NULL),
	    ((cc->flags & CLIENT_TRANSIENT) != 0),
	    ((cc->flags & CLIENT_TYPE_DIALOG) != 0),
	    ((cc->flags & CLIENT_TYPE_DOCK) != 0),
	    ((cc->flags & CLIENT_FULLSCREEN) != 0),
	    RIBBON_RULE_NONE);

	return (ribbon_insert(rb, place, cc) != NULL);
}

/*
 * Called from client_remove() while the client is still whole.  The column
 * losing its last window goes with it, and focus lands where the close
 * policy says.
 */
void
ribbon_client_remove(struct client_ctx *cc)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;
	int			 idx, ncol, last, only, newidx;

	if (!(cc->flags & CLIENT_RIBBON))
		return;

	col = cc->rbcol;
	rb = col->rb;

	idx = ribbon_col_index(rb, col);
	ncol = ribbon_col_count(rb);
	last = (TAILQ_NEXT(col, entry) == NULL);

	ribbon_col_del(cc);

	only = (col->nwin == 0);
	newidx = ribbon_policy_close(idx, ncol, last, only);

	if (only)
		ribbon_col_free(rb, col);

	rb->focus = ribbon_col_at(rb, newidx);
	ribbon_scroll(rb);
}

/*
 * Keep the model's idea of focus in step with the server's, whichever way
 * focus was given - a ribbon command, the pointer, or a client asking for
 * it.  Only a change of column costs a scroll.
 */
void
ribbon_client_focus(struct client_ctx *cc)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (!(cc->flags & CLIENT_RIBBON))
		return;

	col = cc->rbcol;
	rb = col->rb;
	col->focus = cc;

	if (rb->focus == col)
		return;

	rb->focus = col;
	ribbon_scroll(rb);
	ribbon_sync(rb->sc);
}

/* Give the keyboard to the column's own last-focused window. */
static void
ribbon_activate(struct ribbon_col *col)
{
	struct client_ctx	*cc, *old;

	if ((cc = col->focus) == NULL)
		cc = TAILQ_FIRST(&col->winq);
	if (cc == NULL)
		return;

	if ((old = client_current(col->rb->sc)) != NULL)
		client_ptr_save(old);

	client_show(cc);
	client_raise(cc);
	client_set_active(cc);
	client_ptr_warp(cc);
	ribbon_settle();
}

/*
 * Put the pointer back inside the focused window after the ribbon moved
 * underneath it.  cwm follows the mouse, so without this a scroll slides
 * some other window under a resting pointer, the EnterNotify that follows
 * hands focus to it, and the viewport scrolls straight back where it came
 * from.
 */
static void
ribbon_warp(struct ribbon *rb)
{
	struct client_ctx	*cc;

	if (rb->focus == NULL)
		return;
	if ((cc = rb->focus->focus) == NULL)
		cc = TAILQ_FIRST(&rb->focus->winq);
	if ((cc == NULL) || (cc->flags & CLIENT_HIDDEN))
		return;

	client_ptr_warp(cc);
	ribbon_settle();
}

void
ribbon_focus_col(struct screen_ctx *sc, int flags)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (((rb = ribbon_current(sc)) == NULL) || (rb->focus == NULL))
		return;

	if (flags & CWM_LEFT)
		col = TAILQ_PREV(rb->focus, ribbon_col_q, entry);
	else
		col = TAILQ_NEXT(rb->focus, entry);
	if (col == NULL)
		return;

	rb->focus = col;
	ribbon_scroll(rb);
	ribbon_sync(sc);
	ribbon_activate(col);
}

void
ribbon_focus_win(struct screen_ctx *sc, int flags)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;
	struct client_ctx	*cc;

	if (((rb = ribbon_current(sc)) == NULL) || (rb->focus == NULL))
		return;

	col = rb->focus;
	if ((cc = col->focus) == NULL)
		cc = TAILQ_FIRST(&col->winq);
	if (cc == NULL)
		return;

	if (flags & CWM_UP)
		cc = TAILQ_PREV(cc, rb_client_q, rbentry);
	else
		cc = TAILQ_NEXT(cc, rbentry);
	if (cc == NULL)
		return;

	col->focus = cc;
	ribbon_activate(col);
}

/*
 * Carry the window to the neighbouring column, making one at the edge of the
 * ribbon when there is none to carry it to.  A window already alone at the
 * edge has nowhere to go and stays put.
 */
void
ribbon_move_client(struct client_ctx *cc, int flags)
{
	struct ribbon		*rb;
	struct ribbon_col	*src, *dst;

	if (!(cc->flags & CLIENT_RIBBON))
		return;

	src = cc->rbcol;
	rb = src->rb;

	if (flags & CWM_LEFT)
		dst = TAILQ_PREV(src, ribbon_col_q, entry);
	else
		dst = TAILQ_NEXT(src, entry);

	if (dst == NULL) {
		if (src->nwin == 1)
			return;
		dst = ribbon_col_new(rb, src);
		/*
		 * ribbon_col_new() inserts after; going left off the first
		 * column has to land before it, not at the far end of the
		 * ribbon.
		 */
		if (flags & CWM_LEFT) {
			TAILQ_REMOVE(&rb->colq, dst, entry);
			TAILQ_INSERT_BEFORE(src, dst, entry);
		}
		dst->preset = src->preset;
	}

	ribbon_col_del(cc);
	ribbon_col_add(dst, cc);

	if (src->nwin == 0)
		ribbon_col_free(rb, src);

	rb->focus = dst;
	ribbon_scroll(rb);
	ribbon_sync(rb->sc);
	ribbon_warp(rb);
}

/*
 * Step the focused column through the width presets.  CWM_CENTER cycles,
 * CWM_RIGHT widens, CWM_LEFT narrows.
 */
void
ribbon_width(struct client_ctx *cc, int flags)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (!(cc->flags & CLIENT_RIBBON))
		return;

	col = cc->rbcol;
	rb = col->rb;

	if (flags & CWM_CENTER)
		col->preset = (col->preset + 1) % RIBBON_NPRESET;
	else if (flags & CWM_RIGHT)
		col->preset = MIN(col->preset + 1, RIBBON_NPRESET - 1);
	else if (flags & CWM_LEFT)
		col->preset = MAX(col->preset - 1, 0);

	rb->focus = col;
	ribbon_scroll(rb);
	ribbon_sync(rb->sc);
	ribbon_warp(rb);
}

/* Put the focused column in the middle of the viewport. */
void
ribbon_center(struct screen_ctx *sc)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (((rb = ribbon_current(sc)) == NULL) || (rb->focus == NULL))
		return;

	col = rb->focus;
	ribbon_measure(rb);
	rb->offset = ribbon_policy_output(rb->view.w,
	    col->x + (col->w / 2) - (rb->view.w / 2), rb->len);
	ribbon_place(rb);
	ribbon_sync(sc);
	ribbon_warp(rb);
}

/*
 * Drop a window out of the ribbon, or pick a floating one up into it.  The
 * floating mode of cwm stays reachable - not every window wants a column.
 */
void
ribbon_float_toggle(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (cc->flags & CLIENT_RIBBON) {
		col = cc->rbcol;
		rb = col->rb;

		ribbon_col_del(cc);
		if (col->nwin == 0)
			ribbon_col_free(rb, col);
		if (rb->focus == NULL)
			rb->focus = TAILQ_FIRST(&rb->colq);

		ribbon_scroll(rb);
		ribbon_sync(sc);
		ribbon_warp(rb);
		client_raise(cc);
		return;
	}

	if (((rb = ribbon_current(sc)) == NULL) || !rb->active)
		return;

	col = ribbon_col_new(rb, rb->focus);
	ribbon_col_add(col, cc);
	rb->focus = col;
	ribbon_scroll(rb);
	ribbon_sync(sc);
}
