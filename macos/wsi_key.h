/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - the boundary between the ribbon's commands and the Mac's keyboard
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
 * The third seam, and the smallest.  macos/wsi_platform.h is the boundary for
 * windows; this is the boundary for keys, and it is drawn for the same reason:
 * above it is ordinary C that is checked on this machine, below it is an API
 * that exists only on a Mac.
 *
 * WHY THIS API AND NOT THE OTHER ONE - and it is a decision about permissions
 * rather than about code.  macOS offers two ways to take a key combination
 * away from every application at once:
 *
 *   1. RegisterEventHotKey (Carbon Event Manager).  The system delivers the
 *      combination to the registering process and to nobody else - the key is
 *      swallowed, which is what a window manager needs: Control-Option-L must
 *      not also reach the editor underneath.  Apple documents no privacy
 *      permission for it; the port therefore keeps asking the user for ONE
 *      grant (Accessibility) rather than two.
 *
 *   2. CGEventTap on kCGEventKeyDown.  Since macOS 10.15 a tap that listens to
 *      keyboard events needs the separate "Input Monitoring" grant - a second
 *      dialogue, a second row in System Settings, a second thing to lose on a
 *      migration.  See doc/macos-install.md, "Two permissions, and why you are
 *      only asked for one".
 *
 * So: RegisterEventHotKey.  There is a second reason and it is not
 * permissions.  doc/macos.md decided already that this port installs no
 * CGEventTap at all, because it cannot move the pointer (wsip_pointer_warp
 * returns -1) and focus that follows a pointer it cannot drag oscillates.  A
 * tap for keys would put the machinery in the process anyway, one #ifdef away
 * from the decision that was made.
 *
 * Nothing below this line knows what a ribbon or a column is: a binding is an
 * integer the caller chose, and what comes back up is that same integer.
 */

#ifndef _WSI_KEY_H_
#define _WSI_KEY_H_

/*
 * The four modifiers a Mac keyboard has that a window manager may take.  Their
 * values are ours; the platform layer translates them.  Fn and CapsLock are
 * not here: Carbon's hot key registration does not take them.
 */
#define WSIK_SHIFT	0x01
#define WSIK_CONTROL	0x02
#define WSIK_OPTION	0x04	/* Alt */
#define WSIK_COMMAND	0x08

/*
 * Start listening.  0 on success, -1 when the process could not install the
 * handler at all - which on macOS means it has no connection to the window
 * server, i.e. it was started outside a logged-in graphical session.
 */
int		 wsik_open(void);

/*
 * Take one combination for this identifier.
 *
 * The key is named the way cwmrc names it - "h", "equal", "Return" - and the
 * table that turns that name into a Mac key code lives below the line, because
 * the codes are Apple's constants and a Mac compiler is the thing that can
 * check them.
 *
 * 0 on success; -1 when the name is unknown or the system refused the
 * combination, which it does when something else already holds it.  The caller
 * is expected to say which one out loud rather than start with a key that
 * silently does nothing.
 */
int		 wsik_bind(int id, unsigned int mods, const char *key);

/* Give every combination back.  Safe to call when none was ever taken. */
void		 wsik_close(void);

/*
 * Upwards: the one notice.  Called from the platform's own event delivery,
 * which on macOS is the same run loop wsip_pump() turns - so a caller that
 * pumps for windows is pumping for keys too, and there is no second loop.
 */
void		 wsi_note_key(int id);

#endif /* _WSI_KEY_H_ */
