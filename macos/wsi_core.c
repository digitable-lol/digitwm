/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - the eleven of wsi.h on macOS, in the half that is not macOS
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
 * This file is the whole of the macOS port that is not macOS: the third
 * implementation of wsi.h after X11 (client.c, screen.c, xutil.c) and the
 * browser (tools/wasm-layout/shim.c), and the first one written for a window
 * system nobody here can run.  It compiles and is checked on Linux, against
 * a window system made of memory - macos/wsi_fake.c - by macos/wsicheck.c.
 * Everything it cannot do without macOS it asks macos/wsi_platform.h for,
 * and that header is ten calls wide.
 *
 * The reason to draw the line here rather than lower is arithmetic: what is
 * above the line is checked today, what is below waits for a machine.  So
 * every decision the port makes is above it and only the doing is below.
 *
 *
 * THE ONE MECHANISM THAT IS NOT A TRANSLATION
 *
 * Ten of the eleven calls port across by rewriting: XMoveResizeWindow
 * becomes two attribute writes, XRaiseWindow becomes kAXRaiseAction, and so
 * on.  The eleventh, wsi_settle(), does not, and wsi.h says why at length in
 * its own words: on X11 the guarantee rests on synchronous delivery - XSync
 * is a round trip, so "already caused" and "already in our queue" are the
 * same set, and the set can be drained.  On macOS AXObserverCallback is
 * declared void, the notification arrives after the fact, and no timeout
 * closes the set.
 *
 * So the same guarantee is built backwards here, exactly as wsi.h prescribes
 * ("the mover tags the moves it makes, and the notification handler drops
 * what carries a tag"):
 *
 *   - every geometry this port hands to the platform is tagged: the window
 *     it went to, the rectangle, and the moment;
 *   - every frame notification that comes back is matched against the tags,
 *     and a match is dropped instead of being taken for the user;
 *   - wsi_settle() itself does nothing but close the batch and restart the
 *     clock the tags expire on.  It waits for nothing, because waiting is
 *     the thing that does not work here.
 *
 * A tag is not the same thing as a matching rectangle, and the difference is
 * the point.  An application may accept our size approximately - a terminal
 * snapping to whole character cells does exactly that - so the rectangle
 * that comes back is not the one that went out, and yet the notification is
 * still ours.  Matching therefore has two arms: an exact rectangle we wrote
 * (definitive), or an outstanding write to that window inside the echo
 * window (WSI_ECHO_MS, and its comment says where the number is from and
 * which command replaces it).  Neither arm alone is enough; both together
 * are what "tag" means here.
 *
 * What this costs when it is wrong: a user's own drag that lands inside the
 * echo window of a move we made to the same window is mistaken for ours and
 * ignored.  The next ribbon_sync() puts the window back where the model says
 * anyway, so the user sees a drag that did not take - not a corrupted
 * layout.  The opposite error, taking our own echo for the user's drag,
 * costs a write per notification per window for as long as the pointer
 * rests, which is the oscillation wsi.h describes; macos/wsicheck.c shows
 * both, in numbers, by running the same scenario with the tagging off.
 */

#include <sys/types.h>
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calmwm.h"
#include "wsi_core.h"

#define WSI_MAXDISPLAY	8

/*
 * How much of a parked window stays visible.  Not a choice: macOS will not
 * place a window wholly outside the visible area, and AeroSpace - which
 * parks windows the same way, having no other option either - documents the
 * remainder as "a 1 pixel vertical line" (docs/guide.adoc:438-455, quoted in
 * doc/portability.md).  The ribbon's client_hide() promises the window stops
 * being drawn; on macOS it stops being drawn but for this, and that is a
 * loss of the port rather than a bug of this file.
 */
#define WSI_PARK_LEAK	1

struct wsi_tag {
	struct wsip_rect	 rect;
	int			 used;
};

struct wsi_win {
	struct client_ctx	*cc;
	wsip_window		 id;
	int			 inuse;
	int			 parked;
	int			 pending;	/* writes not yet echoed */
	double			 deadline;	/* echoes are ours until here */
	struct wsi_tag		 tag[WSI_TAGDEPTH];
	int			 tagnext;
};

static struct wsi_win		 wsi_win[WSI_MAXWIN];
static struct screen_ctx	*wsi_sc;
static struct client_ctx	*wsi_active;
static struct wsi_stats		 wsi_st;
static int			 wsi_tagging = 1;
static int			 wsi_border;

/*
 * Focus is tagged the same way geometry is, and for the same reason - but
 * with a ring rather than a single slot, and that is not a detail.  Two
 * activations in a row are ordinary (the ribbon activates a column, then a
 * window of it), their two notifications arrive later and in whatever order
 * macOS feels like, and a port remembering only the last request takes the
 * first echo for the user and hands the keyboard back to the window it just
 * left.  The check drives exactly that case; it failed the first time it was
 * run, which is why the ring is here.
 */
static wsip_window		 wsi_focus_tag[WSI_TAGDEPTH];
static int			 wsi_focus_used[WSI_TAGDEPTH];
static int			 wsi_focus_next;
static int			 wsi_focus_pending;

static struct region_ctx	 wsi_region[WSI_MAXDISPLAY];
static char			 wsi_region_name[WSI_MAXDISPLAY][WSIP_NAMELEN];
static int			 wsi_nregion;

static struct wsi_win	*wsi_slot(wsip_window);
static void		 wsi_tag_add(struct wsi_win *, const struct wsip_rect *);
static int		 wsi_tag_match(struct wsi_win *, const struct wsip_rect *,
			     double);
static void		 wsi_write(struct wsi_win *, const struct wsip_rect *);
static int		 wsi_rect_eq(const struct wsip_rect *,
			     const struct wsip_rect *);
static struct wsip_rect	 wsi_park_rect(const struct wsip_rect *);

static int
wsi_rect_eq(const struct wsip_rect *a, const struct wsip_rect *b)
{
	return (a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h);
}

static struct wsi_win *
wsi_slot(wsip_window id)
{
	int	 i;

	for (i = 0; i < WSI_MAXWIN; i++) {
		if (wsi_win[i].inuse && wsi_win[i].id == id)
			return &wsi_win[i];
	}
	return NULL;
}

static struct wsi_win *
wsi_of(struct client_ctx *cc)
{
	int	 i;

	if (cc == NULL)
		return NULL;
	for (i = 0; i < WSI_MAXWIN; i++) {
		if (wsi_win[i].inuse && wsi_win[i].cc == cc)
			return &wsi_win[i];
	}
	return NULL;
}

int
wsi_core_init(struct screen_ctx *sc)
{
	(void)memset(wsi_win, 0, sizeof(wsi_win));
	(void)memset(&wsi_st, 0, sizeof(wsi_st));
	wsi_sc = sc;
	wsi_active = NULL;
	(void)memset(wsi_focus_tag, 0, sizeof(wsi_focus_tag));
	(void)memset(wsi_focus_used, 0, sizeof(wsi_focus_used));
	wsi_focus_next = 0;
	wsi_focus_pending = 0;
	wsi_nregion = 0;

	if (wsip_open() != 0)
		return -1;

	wsi_regions_update(sc);
	return 0;
}

int
wsi_manage(struct client_ctx *cc, wsip_window id)
{
	int	 i;

	if (cc == NULL || wsi_slot(id) != NULL)
		return -1;
	for (i = 0; i < WSI_MAXWIN; i++) {
		if (wsi_win[i].inuse)
			continue;
		(void)memset(&wsi_win[i], 0, sizeof(wsi_win[i]));
		wsi_win[i].inuse = 1;
		wsi_win[i].cc = cc;
		wsi_win[i].id = id;
		cc->win = id;
		(void)wsip_watch(id, 1);
		return 0;
	}
	return -1;
}

void
wsi_unmanage(struct client_ctx *cc)
{
	struct wsi_win	*w;

	if ((w = wsi_of(cc)) == NULL)
		return;
	(void)wsip_watch(w->id, 0);
	if (wsi_active == cc)
		wsi_active = NULL;
	(void)memset(w, 0, sizeof(*w));
}

struct client_ctx *
wsi_lookup(wsip_window id)
{
	struct wsi_win	*w = wsi_slot(id);

	return (w == NULL) ? NULL : w->cc;
}

const struct wsi_stats *
wsi_core_stats(void)
{
	return &wsi_st;
}

void
wsi_core_stats_reset(void)
{
	(void)memset(&wsi_st, 0, sizeof(wsi_st));
}

void
wsi_core_tagging(int on)
{
	wsi_tagging = on;
}

void
wsi_core_border(int n)
{
	wsi_border = n;
}

/*
 * The displays, turned into the region list the ribbon binds itself to by
 * name.  The names are copied into storage of our own because struct
 * region_ctx keeps a pointer and the platform's array is a scratch buffer.
 */
void
wsi_regions_update(struct screen_ctx *sc)
{
	struct wsip_display	 disp[WSI_MAXDISPLAY];
	int			 i, n;

	if (sc == NULL)
		return;

	n = wsip_displays(disp, WSI_MAXDISPLAY);
	if (n < 0)
		n = 0;
	if (n > WSI_MAXDISPLAY)
		n = WSI_MAXDISPLAY;

	TAILQ_INIT(&sc->regionq);
	wsi_nregion = n;
	for (i = 0; i < n; i++) {
		(void)memset(&wsi_region[i], 0, sizeof(wsi_region[i]));
		(void)strlcpy(wsi_region_name[i], disp[i].name, WSIP_NAMELEN);
		wsi_region[i].num = i;
		wsi_region[i].name = wsi_region_name[i];
		wsi_region[i].view.x = disp[i].view.x;
		wsi_region[i].view.y = disp[i].view.y;
		wsi_region[i].view.w = disp[i].view.w;
		wsi_region[i].view.h = disp[i].view.h;
		wsi_region[i].work.x = disp[i].work.x;
		wsi_region[i].work.y = disp[i].work.y;
		wsi_region[i].work.w = disp[i].work.w;
		wsi_region[i].work.h = disp[i].work.h;
		TAILQ_INSERT_TAIL(&sc->regionq, &wsi_region[i], entry);
	}

	if (n > 0) {
		sc->view = wsi_region[0].view;
		sc->work = wsi_region[0].work;
	}
}

/*
 * A window appeared.
 *
 * This is where doc/portability.md's loss 1 lands, and it is worth being
 * exact about what is lost and what is not.  On X11 the same work happens
 * inside the MapRequest handler, before the window is on the screen: the
 * manager is answering the request instead of the server, so the window's
 * first appearance is already in its column.  Here the window is on the
 * screen before the notification is written, AXObserverCallback returns
 * void, and there is nothing to answer.  So the window appears where its
 * application put it and then moves - one round trip and one notification
 * later.
 *
 * What is NOT lost is everything below this line: which column it belongs
 * in, what that does to its neighbours, and the promise that it changes the
 * ribbon geometry of no window already open.  That is arithmetic, it is the
 * same call in the same order as client.c makes, and it is checked here.
 */
void
wsi_note_open(wsip_window id, const struct wsip_rect *r)
{
	struct client_ctx	*cc;

	if (wsi_sc == NULL || wsi_lookup(id) != NULL)
		return;

	cc = xcalloc(1, sizeof(*cc));
	cc->sc = wsi_sc;
	cc->bwidth = wsi_border;
	cc->geom.x = r->x;
	cc->geom.y = r->y;
	cc->geom.w = r->w;
	cc->geom.h = r->h;
	cc->savegeom = cc->geom;

	TAILQ_INSERT_TAIL(&wsi_sc->clientq, cc, entry);
	if (wsi_manage(cc, id) != 0) {
		TAILQ_REMOVE(&wsi_sc->clientq, cc, entry);
		free(cc);
		return;
	}

	/*
	 * The order is client.c's, not one of our own: place, move the
	 * neighbours, then hand over the keyboard.  A port that focused first
	 * would show the window in its old place with a border round it.
	 */
	if (!ribbon_client_insert(cc)) {
		wsi_unmanage(cc);
		TAILQ_REMOVE(&wsi_sc->clientq, cc, entry);
		free(cc);
		return;
	}
	wsi_st.opened++;
	ribbon_sync(wsi_sc);
	client_set_active(cc);
	ribbon_client_focus(cc);
	ribbon_sync(wsi_sc);
}

void
wsi_note_close(wsip_window id)
{
	struct client_ctx	*cc;

	if (wsi_sc == NULL || (cc = wsi_lookup(id)) == NULL)
		return;

	ribbon_client_remove(cc);
	TAILQ_REMOVE(&wsi_sc->clientq, cc, entry);
	wsi_unmanage(cc);
	free(cc);
	wsi_st.closed++;
	ribbon_sync(wsi_sc);
}

void
wsi_note_displays(void)
{
	if (wsi_sc == NULL)
		return;
	wsi_regions_update(wsi_sc);
	ribbon_screen_relayout(wsi_sc);
}

/*
 * The tags.
 */

static void
wsi_tag_add(struct wsi_win *w, const struct wsip_rect *r)
{
	w->tag[w->tagnext].rect = *r;
	w->tag[w->tagnext].used = 1;
	w->tagnext = (w->tagnext + 1) % WSI_TAGDEPTH;
	w->pending++;
	w->deadline = wsip_now() + WSI_ECHO_MS;
}

static int
wsi_tag_match(struct wsi_win *w, const struct wsip_rect *r, double now)
{
	int	 i;

	if (!wsi_tagging || w->pending <= 0)
		return 0;

	/* An exact rectangle we wrote is ours whatever the clock says. */
	for (i = 0; i < WSI_TAGDEPTH; i++) {
		if (w->tag[i].used && wsi_rect_eq(&w->tag[i].rect, r)) {
			w->tag[i].used = 0;
			w->pending--;
			return 1;
		}
	}

	/*
	 * Otherwise it is ours if a write to this window is still outstanding
	 * and the echo window has not run out: the application took our move
	 * and adjusted it, which is a thing applications do and not a thing
	 * users do.
	 */
	if (now <= w->deadline) {
		w->pending--;
		return 1;
	}
	return 0;
}

static void
wsi_write(struct wsi_win *w, const struct wsip_rect *r)
{
	wsi_tag_add(w, r);
	wsi_st.frame_set++;
	(void)wsip_frame_set(w->id, r);
}

/*
 * The park slot: hard against the right edge of the display the window is
 * on, all but WSI_PARK_LEAK of it outside.  The model geometry is not
 * touched - the ribbon owns that outright, and client_show() puts the window
 * back from it.
 */
static struct wsip_rect
wsi_park_rect(const struct wsip_rect *from)
{
	struct wsip_rect	 r = *from;
	int			 i, best = 0;

	for (i = 0; i < wsi_nregion; i++) {
		if (from->x >= wsi_region[i].view.x &&
		    from->x < wsi_region[i].view.x + wsi_region[i].view.w)
			best = i;
	}
	if (wsi_nregion > 0)
		r.x = wsi_region[best].view.x + wsi_region[best].view.w -
		    WSI_PARK_LEAK;
	else
		r.x = from->x + from->w;
	return r;
}

/*
 * The eleven of wsi.h.
 */

/*
 * Has this window already been told to stand where cc->geom now says?
 *
 * The same two lines as client.c:587 and tools/wasm-layout/shim.c, and they
 * are copied rather than shared because the three implementations of wsi.h
 * do not link against each other - that is what an implementation of a
 * contract is.  wsi.h calls this "the one call in the contract that exists
 * purely to avoid another one", and names macOS as the reason it exists: a
 * skipped resize here is a skipped round trip into another process.
 */
int
client_geom_current(struct client_ctx *cc)
{
	if (!(cc->flags & CLIENT_GEOM_SENT))
		return 0;

	return (cc->geom.x == cc->sentgeom.x && cc->geom.y == cc->sentgeom.y &&
	    cc->geom.w == cc->sentgeom.w && cc->geom.h == cc->sentgeom.h);
}

void
client_resize(struct client_ctx *cc, int reset)
{
	struct wsi_win		*w;
	struct wsip_rect	 r;

	/*
	 * The second argument is "do not clear the maximised state while you
	 * are at it", and the ribbon passes 0 always (wsi.h).  This port has
	 * no maximised state of its own to clear - a zoomed window on macOS
	 * is the application's business, not ours - so there is nothing to
	 * do with it either way.
	 */
	(void)reset;

	if ((w = wsi_of(cc)) == NULL)
		return;

	r.x = cc->geom.x;
	r.y = cc->geom.y;
	r.w = cc->geom.w;
	r.h = cc->geom.h;

	cc->sentgeom = cc->geom;
	cc->flags |= CLIENT_GEOM_SENT;
	wsi_write(w, &r);
}

/*
 * Off the screen: past the edge, because macOS will not unmap another
 * application's window and minimising to the Dock is an action the user
 * sees, which client_hide() is explicitly not asked for.
 *
 * Stacking survives for free, and that is worth naming: client_show()
 * promises the window comes back "above what it was above before", and
 * parking is a move rather than an unmap, so nothing in the stack ever
 * changed and the promise costs nothing to keep.  On X11 the same promise
 * costs an XMapRaised.
 */
void
client_hide(struct client_ctx *cc)
{
	struct wsi_win		*w;
	struct wsip_rect	 from, park;

	if ((w = wsi_of(cc)) == NULL || w->parked)
		return;

	from.x = cc->geom.x;
	from.y = cc->geom.y;
	from.w = cc->geom.w;
	from.h = cc->geom.h;
	park = wsi_park_rect(&from);

	w->parked = 1;
	cc->flags |= CLIENT_HIDDEN;
	cc->flags &= ~CLIENT_GEOM_SENT;
	wsi_st.park++;
	wsi_write(w, &park);

	/*
	 * "If it held the input focus, the focus goes away with it rather
	 * than to some window of the window system's choosing" - wsi.h,
	 * client_hide().  This is the one promise in the header that this
	 * port cannot keep from anything the tree can cite.  There is no call
	 * in doc/portability.md's table, nor in the Accessibility headers it
	 * quotes, for "let no window have the keyboard": focus on macOS is a
	 * front process plus a focused window of it, and there is no empty
	 * value for the pair.  So the port does the half it can - forgets its
	 * own record, so client_current() does not lie - and macOS gives the
	 * keyboard to whatever it likes.  The ribbon then hears that as a
	 * foreign focus notice, which is a path it has and survives; what it
	 * does not get is the guarantee as written.  Reported, not worked
	 * around: see the report of this branch.
	 */
	if (wsi_active == cc)
		wsi_active = NULL;
}

void
client_show(struct client_ctx *cc)
{
	struct wsi_win		*w;
	struct wsip_rect	 r;

	if ((w = wsi_of(cc)) == NULL || !w->parked)
		return;

	r.x = cc->geom.x;
	r.y = cc->geom.y;
	r.w = cc->geom.w;
	r.h = cc->geom.h;

	w->parked = 0;
	cc->flags &= ~CLIENT_HIDDEN;
	cc->sentgeom = cc->geom;
	cc->flags |= CLIENT_GEOM_SENT;
	wsi_st.unpark++;
	wsi_write(w, &r);
}

void
client_raise(struct client_ctx *cc)
{
	struct wsi_win	*w;

	if ((w = wsi_of(cc)) == NULL)
		return;
	(void)wsip_raise(w->id);
}

void
client_set_active(struct client_ctx *cc)
{
	struct wsi_win	*w;

	if (wsi_active != NULL)
		wsi_active->flags &= ~CLIENT_ACTIVE;
	wsi_active = cc;
	if (cc == NULL)
		return;
	cc->flags |= CLIENT_ACTIVE;

	if ((w = wsi_of(cc)) == NULL)
		return;

	/*
	 * Tagged like a move, and for the identical reason: the focus change
	 * we just asked for comes back to us as a notification some
	 * milliseconds later, and a port that took its own echo for the
	 * user's doing would answer it with another focus change.
	 */
	wsi_focus_tag[wsi_focus_next] = w->id;
	wsi_focus_used[wsi_focus_next] = 1;
	wsi_focus_next = (wsi_focus_next + 1) % WSI_TAGDEPTH;
	wsi_focus_pending++;
	(void)wsip_activate(w->id);
}

struct client_ctx *
client_current(struct screen_ctx *sc)
{
	if (wsi_active == NULL)
		return NULL;
	if (sc != NULL && wsi_active->sc != sc)
		return NULL;
	return wsi_active;
}

/*
 * The pointer.
 *
 * One decision, taken once, for both of these: this port does not move the
 * pointer, and it does not let focus follow the pointer.  wsi.h allows
 * exactly that pairing in as many words under client_ptr_warp(), and macOS
 * forces it: doc/portability.md lists a source for every other line of the
 * contract and none at all for putting the pointer somewhere, while focus
 * following the pointer is a thing this port would have to install on
 * purpose (a CGEventTap) and therefore simply does not.
 *
 * The saving is still real: it is what the ribbon uses to remember where in
 * a window the pointer stood, and the day a call for warping is found with a
 * source, the memory is already kept.
 */
void
client_ptr_save(struct client_ctx *cc)
{
	int	 x, y;

	if (wsip_pointer(&x, &y) != 0) {
		cc->ptr.x = cc->geom.w / 2;
		cc->ptr.y = cc->geom.h / 2;
		return;
	}

	x -= cc->geom.x;
	y -= cc->geom.y;

	/*
	 * A pointer that is not inside the window at all is remembered as its
	 * centre - wsi.h, client_ptr_save(): "the point of the memory is that
	 * the window keeps a sensible spot, not that the fact is recorded
	 * faithfully."
	 */
	if (x < 0 || y < 0 || x > cc->geom.w || y > cc->geom.h) {
		x = cc->geom.w / 2;
		y = cc->geom.h / 2;
	}
	cc->ptr.x = x;
	cc->ptr.y = y;
}

void
client_ptr_warp(struct client_ctx *cc)
{
	if (wsip_pointer_warp(cc->geom.x + cc->ptr.x,
	    cc->geom.y + cc->ptr.y) != 0)
		wsi_st.warp_refused++;
}

struct region_ctx *
region_pointer(struct screen_ctx *sc)
{
	int	 x, y, i;

	(void)sc;

	if (wsip_pointer(&x, &y) != 0)
		return NULL;

	for (i = 0; i < wsi_nregion; i++) {
		if (x >= wsi_region[i].view.x &&
		    x < wsi_region[i].view.x + wsi_region[i].view.w &&
		    y >= wsi_region[i].view.y &&
		    y < wsi_region[i].view.y + wsi_region[i].view.h)
			return &wsi_region[i];
	}

	/*
	 * "no answer at all is a legal one: the caller then falls back to the
	 * first attached ribbon" - wsi.h, region_pointer().
	 */
	return NULL;
}

/*
 * Let the moves just made settle.
 *
 * On X11 this is a round trip and a drain.  Here it is neither, and the
 * reason is the whole of the paragraph wsi.h spends on this call: there is
 * no round trip to make and the set of notifications caused by what we just
 * sent is not closed at any timeout, so waiting cannot be made to mean
 * anything.  What is left to do at this point is bookkeeping, and it is not
 * nothing:
 *
 *   - the batch is closed, so the echo window of every outstanding write is
 *     restarted from now.  A sync over forty windows takes as long as it
 *     takes; measuring each write's patience from the write itself would
 *     make the first window of a long batch impatient by the time the last
 *     one is sent.
 *
 * The guarantee itself - "no focus change caused by geometry the ribbon
 * itself has just pushed is still on its way in" - was made earlier, when
 * each of those moves was tagged.  It is kept later, when the echoes arrive
 * and are dropped.  This call is the seam between the two halves and the
 * proof that the contract survived the port: same promise, other mechanism,
 * and the ribbon above it did not change by one line.
 */
void
wsi_settle(void)
{
	double	 now = wsip_now();
	int	 i;

	wsi_st.settle++;

	for (i = 0; i < WSI_MAXWIN; i++) {
		if (wsi_win[i].inuse && wsi_win[i].pending > 0)
			wsi_win[i].deadline = now + WSI_ECHO_MS;
	}
}

/*
 * What comes back from the window system.
 */

void
wsi_note_frame(wsip_window id, const struct wsip_rect *r)
{
	struct wsi_win	*w;

	if ((w = wsi_slot(id)) == NULL)
		return;

	wsi_st.note_frame++;

	if (wsi_tag_match(w, r, wsip_now())) {
		wsi_st.echo_own++;
		return;
	}

	/*
	 * Nobody but us was supposed to move this window, so somebody did:
	 * the user dragged it, or the application resized itself.  The port
	 * does not fight over it here - it records that the window is no
	 * longer where it was told to stand, which makes client_geom_current()
	 * answer 0, which makes the next ribbon_sync() send the geometry
	 * again.  One place decides, and it is the ribbon.
	 */
	wsi_st.echo_foreign++;
	if (w->cc != NULL)
		w->cc->flags &= ~CLIENT_GEOM_SENT;
}

static int
wsi_focus_match(wsip_window id)
{
	int	 i;

	if (!wsi_tagging || wsi_focus_pending <= 0)
		return 0;

	/*
	 * Exact, and only exact: a focus change is a window, not a rectangle,
	 * so there is no "the application obeyed approximately" arm here.  A
	 * notice naming a window we did not ask for, while we are waiting for
	 * ones we did, is the user reaching past us - and it must get through.
	 *
	 * And no clock either, which is the one place this differs from the
	 * geometry tags above.  The clock there is a second arm, for the case
	 * where the rectangle that comes back is not the one that went out;
	 * here the match is exact, so a deadline could only ever turn a right
	 * answer into a wrong one - and it would, because the deadline that
	 * matters is another application's, and wsi.h's whole argument about
	 * macOS is that no timeout closes that set.  Two applications answer
	 * at their own speeds and in their own order; counting is the only
	 * thing that survives it.
	 *
	 * What an entry costs when its notification never arrives: it sits in
	 * the ring until eight further activations push it out, and in that
	 * time it would swallow one genuine focus of the same window.  The
	 * ribbon's model already has that window focused - we asked for it -
	 * so swallowing it changes nothing the user can see.  The opposite
	 * bound, a deadline, costs the keyboard jumping back to the window it
	 * just left, which the user certainly can.
	 */
	for (i = 0; i < WSI_TAGDEPTH; i++) {
		if (wsi_focus_used[i] && wsi_focus_tag[i] == id) {
			wsi_focus_used[i] = 0;
			wsi_focus_pending--;
			return 1;
		}
	}
	return 0;
}

void
wsi_note_focus(wsip_window id)
{
	struct wsi_win	*w;

	wsi_st.note_focus++;

	if (wsi_focus_match(id)) {
		wsi_st.focus_own++;
		return;
	}

	if ((w = wsi_slot(id)) == NULL)
		return;

	wsi_st.focus_foreign++;
	if (wsi_active != NULL)
		wsi_active->flags &= ~CLIENT_ACTIVE;
	wsi_active = w->cc;
	if (w->cc == NULL)
		return;
	w->cc->flags |= CLIENT_ACTIVE;
	ribbon_client_focus(w->cc);
}
