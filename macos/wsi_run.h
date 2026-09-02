/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - the window manager, as a thing that can be started and stopped
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
 * Apart from macos/wsi_main.c for one reason and it is not tidiness: main() is
 * argument handling and a call, and everything under it is checked on this
 * machine by macos/runcheck.c against the window system of memory.  A window
 * manager whose start-up sequence exists only inside main() has a start-up
 * sequence nobody has ever run.
 */

#ifndef _WSI_RUN_H_
#define _WSI_RUN_H_

struct screen_ctx;

/*
 * Build the screen, take the permission, and take the windows that are already
 * open onto the ribbon.  0 on success; -1 when Accessibility has not been
 * granted, which is the one failure a person can do something about.
 */
int			 wsi_run_init(void);

/*
 * Take the key combinations the configuration asks for.  Returns the number
 * refused - which is not fatal and must not be silent: a combination another
 * application already holds is refused by the system, and the user has to be
 * told which one, or he will press it and conclude the manager is broken.
 * -1 when the keyboard could not be listened to at all.
 */
int			 wsi_run_keys(void);

/* Turn the loop until wsi_run_stop(), or until "quit" is pressed. */
void			 wsi_run_loop(void);
void			 wsi_run_stop(void);
int			 wsi_run_running(void);

/* One turn of the loop, for a harness that owns its own clock. */
void			 wsi_run_step(double ms);

/*
 * Go down the list of Apple calls this tree cannot check without a Mac, one at
 * a time, and print which of them answered.  Returns the number that did not.
 * This is what a first run that goes wrong is diagnosed with: see
 * doc/macos-install.md, "When it does not work, find out which name failed".
 */
int			 wsi_run_doctor(void);

struct screen_ctx	*wsi_run_screen(void);

#endif /* _WSI_RUN_H_ */
