/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - macos/wsi_platform.h without macOS: windows in memory
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
 * What this is honest about, said before the code, the way
 * tools/wasm-layout/shim.c says it: the windows here are not windows.  There
 * are no applications, no window server and no Accessibility API - a window
 * is four numbers in an array.  Nothing measured against this file is a
 * measurement of macOS, and no number out of macos/wsicheck.c may ever be
 * quoted as one.
 *
 * What it is faithfully, and what it is for: the SHAPE of macOS, in the one
 * respect the port turns on.
 *
 *   - a geometry write returns immediately and takes effect later;
 *   - the notification that it took effect arrives after the fact, through
 *     the same door the user's own drags arrive through, and carries nothing
 *     to say which of the two it was;
 *   - the application is allowed to obey approximately, so the rectangle
 *     that comes back need not be the one that went out;
 *   - the pointer cannot be moved.
 *
 * Every one of those is a line of doc/portability.md about the real thing,
 * and together they are exactly the conditions under which wsi_settle()
 * cannot be a wait.  A port that passes against this file is not proven to
 * work on macOS; a port that fails against it is proven not to.
 *
 * The clock is virtual, which is why the check runs in milliseconds instead
 * of minutes: the port takes its time from wsip_now(), so time here is a
 * variable that fake_pump() adds to.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wsi_fake.h"

#define FAKE_MAXWIN	512
#define FAKE_MAXNOTE	4096
#define FAKE_MAXDISP	8

struct fake_win {
	wsip_window		 id;
	struct wsip_rect	 rect;
	int			 inuse;
	int			 watched;
	double			 latency;	/* < 0: use the common one */
};

#define FAKE_NOTE_FRAME	0
#define FAKE_NOTE_FOCUS	1

struct fake_note {
	int			 kind;
	wsip_window		 id;
	struct wsip_rect	 rect;
	double			 due;
};

static struct fake_win		 fk_win[FAKE_MAXWIN];
static struct fake_note		 fk_note[FAKE_MAXNOTE];
static int			 fk_nnote;
static struct wsip_display	 fk_disp[FAKE_MAXDISP];
static int			 fk_ndisp;
static double			 fk_clock;
static double			 fk_latency = 20.0;
static int			 fk_slop;
static int			 fk_ptr_x, fk_ptr_y;
static wsip_window		 fk_focus;
static unsigned long		 fk_sent;

static struct fake_win *
fk_find(wsip_window id)
{
	int	 i;

	for (i = 0; i < FAKE_MAXWIN; i++) {
		if (fk_win[i].inuse && fk_win[i].id == id)
			return &fk_win[i];
	}
	return NULL;
}

static void
fk_queue(int kind, wsip_window id, const struct wsip_rect *r)
{
	struct fake_note	*n;
	struct fake_win		*w;

	if (fk_nnote >= FAKE_MAXNOTE) {
		(void)fprintf(stderr, "fake: notice queue overflow\n");
		exit(1);
	}
	w = fk_find(id);
	n = &fk_note[fk_nnote++];
	n->kind = kind;
	n->id = id;
	n->due = fk_clock + ((w != NULL && w->latency >= 0.0) ? w->latency :
	    fk_latency);
	if (r != NULL)
		n->rect = *r;
	else
		(void)memset(&n->rect, 0, sizeof(n->rect));
}

void
fake_reset(void)
{
	(void)memset(fk_win, 0, sizeof(fk_win));
	(void)memset(fk_note, 0, sizeof(fk_note));
	(void)memset(fk_disp, 0, sizeof(fk_disp));
	fk_nnote = 0;
	fk_ndisp = 0;
	fk_clock = 0.0;
	fk_latency = 20.0;
	fk_slop = 0;
	fk_ptr_x = 0;
	fk_ptr_y = 0;
	fk_focus = 0;
	fk_sent = 0;
}

void
fake_display(const char *name, int vx, int vy, int vw, int vh,
    int wx, int wy, int ww, int wh)
{
	struct wsip_display	*d;

	if (fk_ndisp >= FAKE_MAXDISP)
		return;
	d = &fk_disp[fk_ndisp++];
	(void)memset(d, 0, sizeof(*d));
	(void)snprintf(d->name, sizeof(d->name), "%s", name);
	d->view.x = vx;
	d->view.y = vy;
	d->view.w = vw;
	d->view.h = vh;
	d->work.x = wx;
	d->work.y = wy;
	d->work.w = ww;
	d->work.h = wh;
}

void
fake_open(wsip_window id, int x, int y, int w, int h)
{
	int	 i;

	for (i = 0; i < FAKE_MAXWIN; i++) {
		if (fk_win[i].inuse)
			continue;
		fk_win[i].inuse = 1;
		fk_win[i].watched = 0;
		fk_win[i].id = id;
		fk_win[i].rect.x = x;
		fk_win[i].rect.y = y;
		fk_win[i].rect.w = w;
		fk_win[i].rect.h = h;
		fk_win[i].latency = -1.0;
		return;
	}
	(void)fprintf(stderr, "fake: out of windows\n");
	exit(1);
}

void
fake_close(wsip_window id)
{
	struct fake_win	*w = fk_find(id);

	if (w != NULL)
		(void)memset(w, 0, sizeof(*w));
}

int
fake_frame(wsip_window id, struct wsip_rect *r)
{
	struct fake_win	*w = fk_find(id);

	if (w == NULL)
		return -1;
	*r = w->rect;
	return 0;
}

wsip_window
fake_focused(void)
{
	return fk_focus;
}

void
fake_latency(double ms)
{
	fk_latency = ms;
}

void
fake_slop(int px)
{
	fk_slop = px;
}

void
fake_latency_win(wsip_window id, double ms)
{
	struct fake_win	*w = fk_find(id);

	if (w != NULL)
		w->latency = ms;
}

void
fake_user_move(wsip_window id, int x, int y, int w, int h)
{
	struct fake_win	*win = fk_find(id);

	if (win == NULL)
		return;
	win->rect.x = x;
	win->rect.y = y;
	win->rect.w = w;
	win->rect.h = h;
	if (win->watched)
		fk_queue(FAKE_NOTE_FRAME, id, &win->rect);
}

void
fake_user_focus(wsip_window id)
{
	fk_focus = id;
	fk_queue(FAKE_NOTE_FOCUS, id, NULL);
}

void
fake_pointer(int x, int y)
{
	fk_ptr_x = x;
	fk_ptr_y = y;
}

/*
 * Deliver everything that has come due within the next ms of virtual time.
 * Notices go out in the order they were queued, which is what a run loop
 * does with a single source.
 */
int
fake_pump(double ms)
{
	struct fake_note	 out[FAKE_MAXNOTE];
	int			 i, n = 0, keep = 0;

	fk_clock += ms;

	for (i = 0; i < fk_nnote; i++) {
		if (fk_note[i].due <= fk_clock)
			out[n++] = fk_note[i];
		else
			fk_note[keep++] = fk_note[i];
	}
	fk_nnote = keep;

	/*
	 * By the moment each one came due, not by the order they were asked
	 * for.  A slow application's answer arrives after a fast one's even
	 * when it was asked first, and a port that quietly assumed otherwise
	 * would pass a check that never showed it the case.
	 */
	for (i = 1; i < n; i++) {
		struct fake_note	 t = out[i];
		int			 j = i - 1;

		while (j >= 0 && out[j].due > t.due) {
			out[j + 1] = out[j];
			j--;
		}
		out[j + 1] = t;
	}

	for (i = 0; i < n; i++) {
		fk_sent++;
		if (out[i].kind == FAKE_NOTE_FRAME)
			wsi_note_frame(out[i].id, &out[i].rect);
		else
			wsi_note_focus(out[i].id);
	}
	return n;
}

unsigned long
fake_sent(void)
{
	return fk_sent;
}

/*
 * macos/wsi_platform.h, on this side of the line.
 */

int
wsip_open(void)
{
	return 0;
}

double
wsip_now(void)
{
	return fk_clock;
}

int
wsip_frame_get(wsip_window id, struct wsip_rect *r)
{
	return fake_frame(id, r);
}

/*
 * The write takes effect at once here - a real window server would take a
 * frame or two - and the notification of it does not.  That gap is the whole
 * reason macos/wsi_core.c is shaped the way it is, so it is the one thing
 * this file must not simplify.
 */
int
wsip_frame_set(wsip_window id, const struct wsip_rect *r)
{
	struct fake_win	*w = fk_find(id);

	if (w == NULL)
		return -1;

	w->rect = *r;
	if (fk_slop > 0) {
		w->rect.w -= (w->rect.w % (fk_slop + 1));
		w->rect.h -= (w->rect.h % (fk_slop + 1));
	}
	if (w->watched)
		fk_queue(FAKE_NOTE_FRAME, id, &w->rect);
	return 0;
}

int
wsip_raise(wsip_window id)
{
	return (fk_find(id) == NULL) ? -1 : 0;
}

int
wsip_activate(wsip_window id)
{
	if (fk_find(id) == NULL)
		return -1;
	fk_focus = id;
	fk_queue(FAKE_NOTE_FOCUS, id, NULL);
	return 0;
}

int
wsip_watch(wsip_window id, int on)
{
	struct fake_win	*w = fk_find(id);

	if (w == NULL)
		return -1;
	w->watched = on;
	return 0;
}

int
wsip_pointer(int *x, int *y)
{
	*x = fk_ptr_x;
	*y = fk_ptr_y;
	return 0;
}

/*
 * Refused, and that is the faithful answer rather than a shortcut: the tree
 * has a cited macOS call for every other line of wsi_platform.h and none for
 * this one (doc/portability.md's table, and doc/macos.md:76 which names the
 * gap outright).  wsi.h allows a port to do nothing here provided focus does
 * not follow the pointer there, and this port takes that pair.
 */
int
wsip_pointer_warp(int x, int y)
{
	(void)x;
	(void)y;
	return -1;
}

int
wsip_pump(double ms)
{
	return fake_pump(ms);
}

int
wsip_displays(struct wsip_display *d, int max)
{
	int	 i;

	for (i = 0; i < fk_ndisp && i < max; i++)
		d[i] = fk_disp[i];
	return i;
}
