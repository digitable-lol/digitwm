/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - the window system made of memory, for checking the macOS port
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

#ifndef _WSI_FAKE_H_
#define _WSI_FAKE_H_

#include "wsi_platform.h"

/* Start over: no windows, no pending notices, clock at zero. */
void	 fake_reset(void);

/* One display, added in order; the first one is the screen's own. */
void	 fake_display(const char *, int, int, int, int, int, int, int, int);

/* A window that exists as far as the platform is concerned. */
void	 fake_open(wsip_window, int, int, int, int);
void	 fake_close(wsip_window);

/* Where the platform actually holds it - the thing the check compares. */
int	 fake_frame(wsip_window, struct wsip_rect *);

/* Whether the platform believes this window has the keyboard. */
wsip_window	 fake_focused(void);

/*
 * How late a notification is, and how sloppily the application obeys.
 *
 * Latency is why the macOS port exists in this shape at all: on X11 the
 * answer arrives before the next request, here it arrives after the fact.
 * The default is deliberately larger than the only figure the tree has for
 * the real thing (7.1 ms floor, p99 12.7 - doc/portability.md:238), because
 * a check that passes only when the platform is quick is not a check.
 *
 * "Slop" makes the application accept the size approximately, the way a
 * terminal snapping to whole character cells does.  It is the case that
 * tells a tag apart from a matching rectangle, and without it the reverse
 * mechanism would look simpler than it is.
 */
void	 fake_latency(double);
void	 fake_slop(int);

/*
 * One window's own latency, overriding the common one.  Two windows of two
 * applications are two processes answering at their own speeds, so their
 * notifications race - and the order they arrive in is not the order they
 * were asked for.  That is the case that decided the shape of the focus tags
 * in macos/wsi_core.c, so it has to be reachable from a check.
 */
void	 fake_latency_win(wsip_window, double);

/* Somebody who is not us moves or focuses a window. */
void	 fake_user_move(wsip_window, int, int, int, int);
void	 fake_user_focus(wsip_window);

/* Where the pointer is; this platform will not let it be moved. */
void	 fake_pointer(int, int);

/* Run the clock forward and deliver everything that comes due. */
int	 fake_pump(double);

/* How many notices the platform has sent, of both kinds. */
unsigned long	 fake_sent(void);

#endif /* _WSI_FAKE_H_ */
