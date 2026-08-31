/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - the macOS port's own half of the contract, and its counters
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

#ifndef _WSI_CORE_H_
#define _WSI_CORE_H_

#include "wsi_platform.h"

struct client_ctx;
struct screen_ctx;

/*
 * How long after a batch of moves an incoming notification is still taken to
 * be an echo of one of them.
 *
 * The number is not measured by us and must not be presented as if it were.
 * The only figure in the tree for how late macOS is with a per-window
 * notification is third-party: never earlier than 7.1 ms, p50 7.2, p99 12.7
 * (alt-tab-macos on macOS 26.5, doc/portability.md:238).  40 ms is that p99
 * with room over it, and the command that replaces the guess with a
 * measurement is already written: "sh tools/macos-flicker/run.sh", whose
 * "notice" column is exactly this quantity.  Re-derive it on the first Mac.
 *
 * The window matters in one direction only.  Too long, and a move the user
 * made during the window is mistaken for ours and ignored - one lost drag,
 * corrected by the next sync.  Too short, and our own echo is taken for the
 * user's, which is the oscillation this whole mechanism exists to prevent.
 * So it errs long, and exactness matters less than the sign of the error.
 */
#define WSI_ECHO_MS	40.0

/* How many exact rectangles per window are remembered as ours. */
#define WSI_TAGDEPTH	8

#define WSI_MAXWIN	256

/*
 * What the port counts about itself.  These are not decoration: the check in
 * macos/wsicheck.c states its result in them, and "the reverse mechanism
 * works" is the sentence "echo_own went up and echo_foreign did not".
 */
struct wsi_stats {
	unsigned long	 opened;	/* windows taken onto the ribbon */
	unsigned long	 closed;	/* windows let go */
	unsigned long	 frame_set;	/* geometry handed to the platform */
	unsigned long	 frame_skipped;	/* client_geom_current() said no need */
	unsigned long	 note_frame;	/* frame notices received */
	unsigned long	 echo_own;	/* ...of them dropped as our own */
	unsigned long	 echo_foreign;	/* ...of them passed on as foreign */
	unsigned long	 note_focus;	/* focus notices received */
	unsigned long	 focus_own;	/* ...of them dropped as our own */
	unsigned long	 focus_foreign;	/* ...of them passed to the ribbon */
	unsigned long	 settle;	/* wsi_settle() calls */
	unsigned long	 park;		/* client_hide() */
	unsigned long	 unpark;	/* client_show() */
	unsigned long	 warp_refused;	/* the platform cannot move the pointer */
};

/*
 * Start the port over a screen.  The screen's region list is filled from the
 * platform's displays, so a ribbon can be bound to an output by name.
 */
int	 wsi_core_init(struct screen_ctx *);

/* Take a window into the port's care, and let it go again. */
int	 wsi_manage(struct client_ctx *, wsip_window);
void	 wsi_unmanage(struct client_ctx *);

/*
 * The border width to give a new window.  Zero on macOS and not a choice:
 * you cannot draw a border on another application's window there, which is
 * one of the three places doc/portability.md says the ribbon itself changes.
 * Settable so that the check can drive the arithmetic with borders too - the
 * ribbon's height policy takes bwidth, and a check that only ever passed it
 * zero would leave that argument untested.
 */
void	 wsi_core_border(int);

struct client_ctx	*wsi_lookup(wsip_window);

/* Rebuild the screen's region list from the platform's displays. */
void	 wsi_regions_update(struct screen_ctx *);

const struct wsi_stats	*wsi_core_stats(void);
void			 wsi_core_stats_reset(void);

/*
 * Switch the reverse mechanism off.  Exists for one purpose: so that the
 * check can show what happens without it, in numbers, on the same run.  A
 * port that cannot demonstrate the failure it prevents has not demonstrated
 * anything.
 */
void	 wsi_core_tagging(int);

#endif /* _WSI_CORE_H_ */
