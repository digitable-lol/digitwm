/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - the boundary between the macOS port and macOS itself
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
 * wsi.h is the seam between the ribbon and the window system.  This is the
 * second seam, one layer further down: between the part of the macOS port
 * that is ordinary C - and therefore compiles, runs and is checked on this
 * machine - and the part that can only exist where the Accessibility API
 * does.
 *
 * The split is drawn where it is for one reason: nobody on this project has
 * a Mac.  Everything above this header (macos/wsi_core.c) is checked here
 * against a window system made of memory (macos/wsi_fake.c); everything
 * below it (macos/wsi_ax.m) is checked here only for being consistent with
 * what we believe the Accessibility API to be (macos/stub-build.sh), and its
 * numbers wait for the owner.  So the header is drawn to keep the second set
 * small: ten calls down, three notices up, no decisions.
 *
 * Nothing below this line may know what a ribbon, a column or a client_ctx
 * is; nothing above it may know what an AXUIElementRef is.  A window is a
 * wsip_window - a number this port hands out itself, the same field
 * client_ctx keeps in cc->win, so that neither half needs the other's types.
 *
 * The calls are the ones the eleven of wsi.h decompose into, and no more.
 * Two of them are allowed to fail on macOS and say so at the call site:
 * wsip_pointer_warp(), which has no counterpart at all in the public API
 * (doc/portability.md:141 lists a source for every other line of this
 * header, and none for that one), and wsip_frame_set(), which is a round
 * trip into another process and can be refused by it.
 */

#ifndef _WSI_PLATFORM_H_
#define _WSI_PLATFORM_H_

#define WSIP_NAMELEN	32

struct wsip_rect {
	int	 x;
	int	 y;
	int	 w;
	int	 h;
};

/*
 * One display, as the ribbon needs it: a name that survives a cable being
 * pulled, and a rectangle a panel has already been subtracted from.  That is
 * wsi.h's one dependency that is not a call, and on macOS it is cheaper than
 * on X11 - NSScreen.visibleFrame already has the menu bar and the Dock off,
 * so there is no EWMH strut arithmetic to port at all.
 */
struct wsip_display {
	char			 name[WSIP_NAMELEN];
	struct wsip_rect	 view;	/* the whole display */
	struct wsip_rect	 work;	/* menu bar and Dock already off */
};

typedef unsigned long	 wsip_window;

/* Downwards: what the core asks of the platform. */

/*
 * Take the permission and start the event machinery.  0 on success, -1 when
 * Accessibility has not been granted - the one permission this port needs.
 */
int	 wsip_open(void);

/*
 * A monotonic clock in milliseconds, shared with whatever else measures on
 * this machine.  It is part of the boundary rather than a call to the libc
 * because the reverse mechanism above (macos/wsi_core.c) is timed, and a
 * timed thing that cannot be given a clock cannot be tested: the fake
 * platform hands out a virtual one and the test runs in no time at all.
 */
double	 wsip_now(void);

/* Where the window system currently has this window.  0 on success. */
int	 wsip_frame_get(wsip_window, struct wsip_rect *);

/*
 * Put the window here.  0 when the request went out, -1 when the platform
 * refused it.  Never waits for the window to arrive - see client_resize()
 * in wsi.h, which is not asked for that either.
 */
int	 wsip_frame_set(wsip_window, const struct wsip_rect *);

/* Above its siblings. */
int	 wsip_raise(wsip_window);

/* The keyboard goes here. */
int	 wsip_activate(wsip_window);

/* Subscribe (1) or unsubscribe (0) to this window's notifications. */
int	 wsip_watch(wsip_window, int);

/* Where the pointer is, in the same coordinates as the displays. */
int	 wsip_pointer(int *, int *);

/*
 * Put the pointer there.  Allowed to answer -1 for "this platform cannot",
 * and macOS is expected to: see client_ptr_warp() in wsi.h, which says in
 * as many words that a port unable to move the pointer still satisfies the
 * contract by doing nothing, provided focus does not follow the pointer
 * there either.  Those two are one decision, and this port takes it once.
 */
int	 wsip_pointer_warp(int, int);

/* Fill the array with attached displays; returns how many were written. */
int	 wsip_displays(struct wsip_display *, int);

/*
 * Upwards: what the platform tells the core.  Five, because that is
 * everything a window manager finds out rather than decides - a window
 * appeared, a window went away, a window moved, the focus moved, the
 * displays changed.  Two of them may be an echo of something this port did a
 * moment ago, and telling those apart is the whole of macos/wsi_core.c's
 * reverse mechanism; the platform layer does not try, it just reports.
 *
 * Note where the deciding is: wsi_note_open() gets four numbers and an
 * identifier, and everything that follows from them - making a client,
 * asking the ribbon where it goes, pushing the answer back out - happens
 * above this line, where it is checked.  The platform layer's share of a new
 * window is finding out that there is one.
 */
void	 wsi_note_open(wsip_window, const struct wsip_rect *);
void	 wsi_note_close(wsip_window);
void	 wsi_note_frame(wsip_window, const struct wsip_rect *);
void	 wsi_note_focus(wsip_window);
void	 wsi_note_displays(void);

#endif /* _WSI_PLATFORM_H_ */
