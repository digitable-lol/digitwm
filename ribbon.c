/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
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
 * Windows live on an endless row of columns, each column an ordered stack of
 * windows.  Row and stacks together make a canvas, and the screen is a
 * viewport sliding over it on both axes.  Two invariants drive everything
 * here:
 *
 *   - inserting a window never alters the ribbon geometry of a window that
 *     is already on the ribbon.  Neighbours are pushed along, never squeezed;
 *   - after any event the focused column lies wholly inside the viewport
 *     horizontally, and the focused window of it lies wholly inside the
 *     viewport vertically.  The unit differs per axis because the promise
 *     has to be keepable: a column is bounded by its width, but a stack is
 *     as tall as the windows in it and can outgrow any viewport, so
 *     vertically the most that can be promised is about one window.  What is
 *     larger than the viewport on either axis shows its beginning - left edge
 *     across, top edge down.
 *
 * The scalar decisions - how far to scroll on either axis, how wide a column
 * is, how tall a window in it is, where a new window goes, what gets focus
 * after a close - are the ribbon_policy_* functions below.  They take and
 * return plain numbers, touch no state and call no X function, because the
 * same policy is written a second time as an FTS model and the two are
 * compared vector by vector in CI through "layout-probe".  Everything that
 * needs more than numbers - the loop over columns, the X calls - is
 * deliberately kept out of them.
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
static int		 ribbon_win_away(struct client_ctx *);
static int		 ribbon_col_onribbon(struct ribbon_col *);
static int		 ribbon_col_visible(struct ribbon *,
			     struct ribbon_col *);
static void		 ribbon_focus_extent(struct ribbon *, int *, int *);
static void		 ribbon_sync_one(struct ribbon *);
static void		 ribbon_activate(struct ribbon_col *);
static void		 ribbon_warp(struct ribbon *);

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
 * Offset of the viewport down the canvas once the focused window must be
 * visible.  Same policy as ribbon_policy_offset(), turned on its side: the
 * canvas has to behave the same way down as across or it is not one canvas
 * but two unrelated things sharing a name.  Hence the delegation rather than
 * a second copy of the arithmetic - a copy would be free to drift.
 *
 * What differs is the unit, and that is a matter for the caller: across, the
 * viewport follows the focused column; down, it follows the focused window of
 * it, because a stack of ten windows fits no viewport and a promise about the
 * whole stack could not be kept.
 *
 * Its own name and its own FTS model (fts/stack-offset.fts) all the same:
 * "layout-probe stack-offset" is what the conformance harness compares that
 * model against, and a model written in heights has to be answered by a
 * function that reads in heights.
 */
int
ribbon_policy_voffset(int vh, int wy, int wh, int off, int gap, int canvas)
{
	return ribbon_policy_offset(vh, wy, wh, off, gap, canvas);
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
 * edge instead - nwin windows of minh plus the gaps.  That is the same
 * collision ribbon_policy_width() has, and it is now resolved the same way: by
 * scrolling.  The stack is part of a canvas taller than the viewport, and
 * ribbon_policy_voffset() brings whatever sits below the edge back into it.
 * Measured at 1280x800, gap 8, minh 60: 11 windows fit exactly, 12 make a
 * canvas 811 tall, 20 make one 1352 tall - and the vertical offset runs to
 * exactly 11 and 552, which is what it takes to reach the last window of
 * either.  Before the second axis those were 11 and 552 pixels of windows
 * nothing could reach.
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
 * What two panels facing each other are left with when together they claim
 * more than the region has.  near is the depth already granted to the near
 * edge (top or left), far to the far one (bottom or right), len the size of
 * the region along that axis; want_far picks which of the two the answer is
 * about.
 *
 * Each half of the pair is decided on its own by ribbon_policy_reserve(),
 * which knows only its own edge and therefore cannot see the overrun: a panel
 * 28 deep on top and another 28 deep on the bottom are each perfectly modest,
 * and on a region 40 tall they are together impossible.  This is the fact
 * about the PAIR, and it is a policy rather than a line in screen.c so that
 * the number it decides is written down and proved like every other.
 *
 * The near edge wins the tie.  That is a choice, not a law: something has to
 * give, and giving to the near edge keeps the top of a region where a reader
 * expects it and pushes the loss to the bottom, which is where a status bar
 * lives and where losing it is noticed rather than mysterious.
 *
 * Both arguments arrive from ribbon_policy_reserve() and are therefore never
 * negative and never larger than the region on their own; the arithmetic here
 * does not depend on that, but the reasoning about it does.
 */
int
ribbon_policy_pair(int near, int far, int len, int want_far)
{
	if (len < 0)
		len = 0;
	if (near < 0)
		near = 0;
	if (far < 0)
		far = 0;

	if ((near + far) <= len)
		return want_far ? far : near;

	if (near > len)
		near = len;

	return want_far ? (len - near) : near;
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
	rb->voffset = 0;
	rb->len = 0;
	rb->canvas = 0;
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

	if (((cc = client_current(sc)) != NULL) && (cc->rbcol != NULL))
		return cc->rbcol->rb;

	if (((rc = region_pointer(sc)) != NULL) &&
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
 * Is this window away from the ribbon - hidden by something that is not the
 * ribbon itself?
 *
 * Two mechanisms hide a window and they answer to different owners.  The
 * ribbon parks a window because of where the measurement put it, and marks
 * what it parked with CLIENT_RIBBON_PARKED; a group hides a window because
 * the user put the group away, and marks nothing.  Only the second is a fact
 * the measurement has to take in: a window whose group is hidden holds no
 * slot in its column, and a column left with no window on the ribbon holds
 * no width on the row.
 *
 * The ribbon's own parking is deliberately not counted, and that is the whole
 * reason the mark exists.  Parking is a consequence of the measurement, and a
 * measurement that read its own consequences back would never settle: the
 * parked window would give up its slot, the column would shrink into the
 * viewport, the window would come back, and the same again.
 */
static int
ribbon_win_away(struct client_ctx *cc)
{
	return (((cc->flags & CLIENT_HIDDEN) != 0) &&
	    ((cc->flags & CLIENT_RIBBON_PARKED) == 0));
}

/* How many windows of the column are on the ribbon at all. */
static int
ribbon_col_onribbon(struct ribbon_col *col)
{
	struct client_ctx	*cc;
	int			 n = 0;

	TAILQ_FOREACH(cc, &col->winq, rbentry) {
		if (!ribbon_win_away(cc))
			n++;
	}

	return n;
}

/*
 * The size of the canvas: widths and ribbon positions of every column, the
 * height of every stack, and with them the length of the ribbon and the height
 * of the canvas.  Independent of both offsets, so insertion can measure
 * without having decided yet where to scroll.
 *
 * Only the windows on the ribbon are counted, here and everywhere the row is
 * walked: a window put away with its group is not on the canvas, and a row
 * measured over windows that are not on the screen keeps a hole the width of
 * every column they left behind.  ribbon_win_away() is the whole of that
 * rule.
 *
 * The heights are walked here and again in ribbon_place() because the two
 * answer different questions: scrolling needs to know how tall the canvas is
 * before it can clamp the offset to it, and placing needs to know where each
 * window goes after the clamping.  Walking a stack costs one call to the
 * height policy per window and no allocation, so the honest order is worth
 * more than the saved arithmetic.
 */
void
ribbon_measure(struct ribbon *rb)
{
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 x = 0, y, i, n;

	rb->canvas = 0;

	TAILQ_FOREACH(col, &rb->colq, entry) {
		n = ribbon_col_onribbon(col);

		/*
		 * A column that had windows and has none on the ribbon takes
		 * no width until one of them comes back.  It keeps its place
		 * in the row and every window it holds, so the group comes
		 * back into the column it left and not to the end of the row.
		 *
		 * A column with no windows at all is a different thing and is
		 * measured as before: it is a column just made, on its way to
		 * holding the window that is being inserted into it, and the
		 * width it takes is the width that window will have.
		 */
		if ((n == 0) && (col->nwin > 0)) {
			col->x = x;
			col->w = 0;
			col->h = 0;
			continue;
		}

		col->w = ribbon_policy_width(rb->view.w, col->preset,
		    Conf.ribbongap, Conf.ribbonminw);
		col->x = x;
		x += col->w + Conf.ribbongap;

		i = 0;
		y = 0;
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			if (ribbon_win_away(cc))
				continue;
			y += ribbon_policy_height(rb->view.h, n, i,
			    Conf.ribbongap, Conf.ribbonminh) + Conf.ribbongap;
			i++;
		}
		col->h = (y > 0) ? (y - Conf.ribbongap) : 0;
		if (col->h > rb->canvas)
			rb->canvas = col->h;
	}

	rb->len = (x > 0) ? (x - Conf.ribbongap) : 0;
}

/*
 * Where the focused window sits in its stack, in canvas coordinates.  This is
 * the loop the vertical policy needs and does not do itself: which window has
 * the focus is state, and the policy takes numbers only.
 *
 * A ribbon with no focused column, or a focused column that lost its last
 * window, answers with the top of the canvas - the same place ribbon_scroll()
 * would settle on anyway.  A focused window that went away with its group is
 * the same case: it is not on the canvas, the walk never reaches it, and the
 * answer stays the top.
 */
static void
ribbon_focus_extent(struct ribbon *rb, int *top, int *height)
{
	struct ribbon_col	*col = rb->focus;
	struct client_ctx	*cc, *want;
	int			 i = 0, y = 0, h, n;

	*top = 0;
	*height = 0;

	if (col == NULL)
		return;
	if ((want = col->focus) == NULL)
		want = TAILQ_FIRST(&col->winq);
	if (want == NULL)
		return;

	n = ribbon_col_onribbon(col);

	TAILQ_FOREACH(cc, &col->winq, rbentry) {
		if (ribbon_win_away(cc))
			continue;
		h = ribbon_policy_height(rb->view.h, n, i,
		    Conf.ribbongap, Conf.ribbonminh);
		if (cc == want) {
			*top = y;
			*height = h;
			return;
		}
		y += h + Conf.ribbongap;
		i++;
	}
}

/*
 * Screen geometry of every window from the measured canvas and the current
 * offsets.  Windows left of the viewport simply get a negative x and windows
 * above it a negative y: owning the geometry is the whole reason this is a
 * window manager and not a script driving one.
 *
 * The canvas coordinates in rbgeom keep no offset at all, on either axis.
 * That is what makes the first invariant a statement about anything: a scroll
 * moves every window on the screen, and a harness that checked "the insertion
 * moved nobody" against screen coordinates would be checking the opposite of
 * what was promised.
 */
void
ribbon_place(struct ribbon *rb)
{
	struct ribbon_col	*col;
	struct client_ctx	*cc;
	int			 i, y, h, n;

	TAILQ_FOREACH(col, &rb->colq, entry) {
		i = 0;
		y = 0;
		n = ribbon_col_onribbon(col);
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			/*
			 * A window away with its group keeps the geometry it
			 * had.  Nothing reads it while the window is hidden,
			 * and it is what the window comes back to if the group
			 * returns before anything else moves the ribbon.
			 */
			if (ribbon_win_away(cc))
				continue;

			h = ribbon_policy_height(rb->view.h, n, i,
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
				cc->geom.y = rb->view.y + y - rb->voffset;
				cc->geom.w = MAX(1, col->w - (cc->bwidth * 2));
				cc->geom.h = MAX(1, h - (cc->bwidth * 2));
			}

			y += h + Conf.ribbongap;
			i++;
		}
	}
}

/*
 * Measure, pull the viewport onto the focus on both axes, place.  Across, the
 * viewport follows the focused column; down, the focused window inside it.
 * A ribbon without a focus has nothing to follow and is only pulled back
 * inside the canvas, which is what an output change leaves behind.
 */
void
ribbon_scroll(struct ribbon *rb)
{
	struct ribbon_col	*col = rb->focus;
	int			 top, height;

	ribbon_measure(rb);

	if (col != NULL) {
		rb->offset = ribbon_policy_offset(rb->view.w, col->x, col->w,
		    rb->offset, Conf.ribbongap, rb->len);
		ribbon_focus_extent(rb, &top, &height);
		rb->voffset = ribbon_policy_voffset(rb->view.h, top, height,
		    rb->voffset, Conf.ribbongap, rb->canvas);
	} else {
		rb->offset = ribbon_policy_output(rb->view.w, rb->offset,
		    rb->len);
		rb->voffset = ribbon_policy_output(rb->view.h, rb->voffset,
		    rb->canvas);
	}

	ribbon_place(rb);
}

/*
 * Whether any part of the column is inside the viewport.  Both axes, because
 * a column scrolled off the top of the canvas is as invisible as one scrolled
 * off its left, and ribbonhide exists to stop drawing what is not seen.
 * Every stack starts at the top of the canvas and the offset never goes
 * negative, so the only way a column leaves the viewport upwards is by being
 * shorter than the scroll - a short stack next to a tall one.
 */
static int
ribbon_col_visible(struct ribbon *rb, struct ribbon_col *col)
{
	if ((col->x + col->w) <= rb->offset)
		return 0;
	if (col->x >= (rb->offset + rb->view.w))
		return 0;
	if (col->h <= rb->voffset)
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

			/*
			 * A window that already stands where the model wants it
			 * is not told so again.  On X11 the saving is a buffered
			 * request; on macOS, where the same line is a
			 * synchronous round trip into another process, it is
			 * the difference this check was written for.
			 */
			if (client_geom_current(cc))
				continue;

			client_resize(cc, 0);
		}
	}
}

/* Push the model onto the window system for every ribbon of a screen. */
void
ribbon_sync(struct screen_ctx *sc)
{
	struct ribbon	*rb;

	TAILQ_FOREACH(rb, &sc->ribbonq, entry)
		ribbon_sync_one(rb);

	wsi_settle();
}

/*
 * A group was shown or hidden under the ribbon.  Which windows are on the
 * canvas has changed, so the row is measured again and pushed out - the same
 * cycle every other mutation of the ribbon runs, and the reason group.c has
 * to call something rather than nothing.  Without it the row keeps the widths
 * and the offsets it had while the windows were there, and a hidden group
 * leaves a gap the width of every column it emptied.
 *
 * group.c calls this only when it actually changed a window.  A group holding
 * nothing on this screen - and eight of the nine usually hold nothing - costs
 * no measurement and no settle.
 */
void
ribbon_group_update(struct screen_ctx *sc)
{
	struct ribbon	*rb;

	if (!Conf.ribbon)
		return;

	TAILQ_FOREACH(rb, &sc->ribbonq, entry)
		ribbon_scroll(rb);

	ribbon_sync(sc);
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
		rb->voffset = ribbon_policy_output(rb->view.h, rb->voffset,
		    rb->canvas);
		ribbon_scroll(rb);
	}
}

/*
 * The whole of it: the arithmetic above, and then the seam calls that push
 * it onto the window system.  They are apart because a monitor coming and
 * going is worth proving, and everything worth proving is in the half that
 * needs no display: "layout-probe outputs" replays a hotplug through
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
 * Move a window one place up or down the stack it already stands in.  The
 * column does not change, the row of columns does not change, and no other
 * stack is touched: this is the vertical half of ribbon_move_client(), and
 * the half that has no edge to spill over.  A window already at the top of
 * its stack stays there rather than crossing into a neighbouring column -
 * crossing columns is spelled ribbon-move-left and ribbon-move-right, and a
 * key that silently did both would leave no way to say which was meant.
 *
 * A height belongs to a slot in the stack and not to the window in it:
 * ribbon_policy_height() hands the remainder of the division to the last
 * window of a column, so exchanging the last two windows exchanges their
 * heights with them.  The window takes the slot it moved into.  That is the
 * only answer that leaves the column exactly as tall as it was, which is what
 * keeps the reorder from moving anything outside the column.
 *
 * Apart from ribbon_move_win() for the reason ribbon_insert() is apart from
 * ribbon_client_insert(): all of the model is on this side of the line, and
 * the model is what "layout-probe layout reorder=up" replays with no display
 * to draw on.
 *
 * Returns 1 when the stack changed, 0 when the window was already at the end
 * it was asked to move towards.
 */
int
ribbon_stack_reorder(struct ribbon_col *col, struct client_ctx *cc, int flags)
{
	struct client_ctx	*other;

	if ((col == NULL) || (cc == NULL))
		return 0;

	if (flags & CWM_UP) {
		if ((other = TAILQ_PREV(cc, rb_client_q, rbentry)) == NULL)
			return 0;
		TAILQ_REMOVE(&col->winq, cc, rbentry);
		TAILQ_INSERT_BEFORE(other, cc, rbentry);
	} else {
		if ((other = TAILQ_NEXT(cc, rbentry)) == NULL)
			return 0;
		TAILQ_REMOVE(&col->winq, cc, rbentry);
		TAILQ_INSERT_AFTER(&col->winq, other, cc, rbentry);
	}

	return 1;
}

/*
 * Exchange a column with the one beside it, everything it holds included -
 * its windows in their order, its width preset, its focus.  Nothing enters or
 * leaves a stack, and no column other than the two moves in the row.
 *
 * The two do not simply trade x coordinates.  A column carries its own preset
 * and therefore its own width, so a narrow column moving left of a wide one
 * takes the wide one's left edge while the wide one starts where the narrow
 * one ended; only when the presets match do the coordinates look exchanged.
 * That is ribbon_measure() laying the row out again from the new order rather
 * than anything decided here, and it is why the ribbon as a whole keeps its
 * length: the same widths and the same gaps, in another order.
 *
 * Returns 1 when the row changed, 0 when the column was already at the end of
 * the ribbon it was asked to move towards.  There is deliberately no wrap: a
 * ribbon is a row with two ends and not a ring, and the whole promise about
 * the viewport is written in terms of a column having a place on it.
 */
int
ribbon_col_reorder(struct ribbon *rb, struct ribbon_col *col, int flags)
{
	struct ribbon_col	*other;

	if ((rb == NULL) || (col == NULL))
		return 0;

	if (flags & CWM_LEFT) {
		if ((other = TAILQ_PREV(col, ribbon_col_q, entry)) == NULL)
			return 0;
		TAILQ_REMOVE(&rb->colq, col, entry);
		TAILQ_INSERT_BEFORE(other, col, entry);
	} else {
		if ((other = TAILQ_NEXT(col, entry)) == NULL)
			return 0;
		TAILQ_REMOVE(&rb->colq, col, entry);
		TAILQ_INSERT_AFTER(&rb->colq, other, col, entry);
	}

	return 1;
}

/*
 * Decide the place of a freshly managed window and put it there.  Called
 * from client_init(), that is from within the MapRequest handler and before
 * the window is mapped, so that it appears where it belongs instead of
 * appearing and then jumping.
 *
 * The last argument is the only one that comes from cwmrc rather than from
 * the window: conf_ribbonrule_match() answers with RIBBON_RULE_NONE unless a
 * "ribbonrule" line names this window's class.  client_init() has already
 * called client_class_hint() by this point, so WM_CLASS is there to match
 * against; before it was wired up this call passed RIBBON_RULE_NONE outright
 * and the policy's two rules about it were reachable only from the probe.
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
	    conf_ribbonrule_match(cc));

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
 * focus was given - a ribbon command, the pointer, or a client asking for it.
 *
 * A change of window costs a scroll as much as a change of column does: the
 * vertical axis follows the focused window, so focus landing further down a
 * stack that is taller than the screen has to bring the canvas with it.  Only
 * focus arriving where it already was costs nothing, and that case has to be
 * caught or every scroll would feed itself through the crossing events it
 * causes.
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

	if ((rb->focus == col) && (col->focus == cc))
		return;

	col->focus = cc;
	rb->focus = col;
	ribbon_scroll(rb);
	ribbon_sync(rb->sc);
}

/*
 * Give the keyboard to the column's own last-focused window - and only ever
 * to a window the ribbon is allowed to show.
 *
 * The last-focused window may have gone away with its group since it was
 * focused, and this is the one place that used to show it back regardless:
 * the ribbon never un-hides what it did not hide, and a window put away by
 * its group is not the ribbon's to bring back.  The column answers with the
 * first window it still has on the ribbon instead, and a column that has none
 * is left alone - the keyboard stays where it was rather than landing on a
 * window nobody can see.
 */
static void
ribbon_activate(struct ribbon_col *col)
{
	struct client_ctx	*cc, *old;

	if (((cc = col->focus) == NULL) || ribbon_win_away(cc)) {
		TAILQ_FOREACH(cc, &col->winq, rbentry) {
			if (!ribbon_win_away(cc))
				break;
		}
	}
	if (cc == NULL)
		return;

	if ((old = client_current(col->rb->sc)) != NULL)
		client_ptr_save(old);

	/*
	 * What is left to un-hide here is the ribbon's own parking, and the
	 * mark goes with the window it was on.
	 */
	if (cc->flags & CLIENT_HIDDEN) {
		client_show(cc);
		cc->flags &= ~CLIENT_RIBBON_PARKED;
	}
	client_raise(cc);
	client_set_active(cc);
	/*
	 * The keyboard is handed over on the line above and does not go
	 * through the pointer, so "ribbonwarp no" costs the jump and nothing
	 * else.  The settle stays outside the check on purpose: showing and
	 * raising a window under a resting pointer crosses into it whether or
	 * not the pointer was moved.
	 */
	if (Conf.ribbonwarp)
		client_ptr_warp(cc);
	wsi_settle();
}

/*
 * Put the pointer back inside the focused window after the ribbon moved
 * underneath it.  cwm follows the mouse, so without this a scroll slides
 * some other window under a resting pointer, the EnterNotify that follows
 * hands focus to it, and the viewport scrolls straight back where it came
 * from.
 *
 * "ribbonwarp no" gives that carrying up deliberately, for a pointer that
 * stays where it was put down.  The oscillation does not come back with it:
 * every caller reaches here through ribbon_sync(), and its wsi_settle() has
 * already swallowed the crossings the scroll itself caused.  What is left is
 * the next crossing the hand on the mouse makes, and giving that one the
 * focus is what following the pointer means.  cwmrc(5) states the price.
 */
static void
ribbon_warp(struct ribbon *rb)
{
	struct client_ctx	*cc;

	if (!Conf.ribbonwarp)
		return;
	if (rb->focus == NULL)
		return;
	if ((cc = rb->focus->focus) == NULL)
		cc = TAILQ_FIRST(&rb->focus->winq);
	if ((cc == NULL) || (cc->flags & CLIENT_HIDDEN))
		return;

	client_ptr_warp(cc);
	wsi_settle();
}

void
ribbon_focus_col(struct screen_ctx *sc, int flags)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (((rb = ribbon_current(sc)) == NULL) || (rb->focus == NULL))
		return;

	/*
	 * Step over a column that has windows but none of them on the ribbon:
	 * it takes no width on the row, and scrolling the viewport onto a
	 * column with nothing to show is the hole again, in the focus walk
	 * this time.
	 */
	col = rb->focus;
	do {
		if (flags & CWM_LEFT)
			col = TAILQ_PREV(col, ribbon_col_q, entry);
		else
			col = TAILQ_NEXT(col, entry);
	} while ((col != NULL) && (col->nwin > 0) &&
	    (ribbon_col_onribbon(col) == 0));
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

	/*
	 * The same rule down the stack: a window away with its group is
	 * not there to step onto.
	 */
	do {
		if (flags & CWM_UP)
			cc = TAILQ_PREV(cc, rb_client_q, rbentry);
		else
			cc = TAILQ_NEXT(cc, rbentry);
	} while ((cc != NULL) && ribbon_win_away(cc));
	if (cc == NULL)
		return;

	col->focus = cc;
	/*
	 * Walking the stack is what scrolls the canvas down, and it is how the
	 * bottom of a stack taller than the screen is reached at all.  Before
	 * the second axis this function moved the focus to a window that could
	 * be wholly below the edge and left it there.
	 */
	ribbon_scroll(rb);
	ribbon_sync(sc);
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
 * Carry the window up or down its own stack.  The counterpart of
 * ribbon_move_client(): that one crosses columns and this one never does, so
 * that a stack can be put in order without the window ever being at risk of
 * landing in the column next door.
 *
 * The pointer is warped for the same reason every other ribbon command warps
 * it.  cwm follows the mouse, and a reorder slides the other window of the
 * pair under a resting pointer; the EnterNotify that follows would hand focus
 * to it, and the window just moved would end the command unfocused.
 */
void
ribbon_move_win(struct client_ctx *cc, int flags)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;

	if (!(cc->flags & CLIENT_RIBBON))
		return;

	col = cc->rbcol;
	rb = col->rb;

	if (!ribbon_stack_reorder(col, cc, flags))
		return;

	col->focus = cc;
	rb->focus = col;
	ribbon_scroll(rb);
	ribbon_sync(rb->sc);
	ribbon_warp(rb);
}

/*
 * Exchange the focused column with its neighbour, the whole stack travelling
 * with it.  This is the command that puts the row in order, the way
 * ribbon_move_win() puts a stack in order.
 *
 * Focus does not move: the same window of the same column keeps it, which is
 * what makes the command repeatable - press it twice and the column has gone
 * two places, rather than the ribbon having swapped one pair back and forth.
 * The viewport follows because it always does, and it is the second promise
 * of the ribbon that makes it: the focused column has to end up wholly
 * visible, and after a swap it stands somewhere else.
 *
 * Taken from the screen and not from the client for the reason
 * ribbon_focus_col() is: what moves is the focused COLUMN, and it is still
 * the focused column when the window with the keyboard is a floating one
 * hovering over the ribbon.
 */
void
ribbon_swap_col(struct screen_ctx *sc, int flags)
{
	struct ribbon		*rb;

	if (((rb = ribbon_current(sc)) == NULL) || (rb->focus == NULL))
		return;

	if (!ribbon_col_reorder(rb, rb->focus, flags))
		return;

	ribbon_scroll(rb);
	ribbon_sync(sc);
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

/* Put the focus in the middle of the viewport, on both axes. */
void
ribbon_center(struct screen_ctx *sc)
{
	struct ribbon		*rb;
	struct ribbon_col	*col;
	int			 top, height;

	if (((rb = ribbon_current(sc)) == NULL) || (rb->focus == NULL))
		return;

	col = rb->focus;
	ribbon_measure(rb);
	ribbon_focus_extent(rb, &top, &height);
	rb->offset = ribbon_policy_output(rb->view.w,
	    col->x + (col->w / 2) - (rb->view.w / 2), rb->len);
	rb->voffset = ribbon_policy_output(rb->view.h,
	    top + (height / 2) - (rb->view.h / 2), rb->canvas);
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
