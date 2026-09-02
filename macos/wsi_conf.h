/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - the part of cwmrc that means anything on macOS
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
 * ONE FILE, TWO SYSTEMS, AND HALF OF IT MEANS NOTHING HERE.
 *
 * The macOS port reads the same file the X11 build reads - the same name in
 * the same place, found by the same order (confpath.h, compiled into both
 * builds) - and it has to, because a person with a Mac and a workstation has
 * one ribbon and should not keep two descriptions of it.  What it cannot do is pretend the whole file
 * applies: of the 35 keywords cwmrc has (parse.y, "keywords[]"), 13 describe
 * things macOS has no counterpart for, and reading them silently would be
 * worse than not reading them at all - the user would set a colour, see
 * nothing, and have no way to tell whether the setting was wrong or the
 * platform was.
 *
 * So this parser has three answers per line, and it says which out loud:
 *
 *   taken   - the directive changed something (every "ribbon*" setting, and
 *             bind-key/unbind-key);
 *   skipped - the directive is understood, and belongs to a part of the
 *             window system macOS does not have.  Named, with the reason, on
 *             stderr and in "digitwm -n";
 *   bad     - not a cwmrc directive at all, or malformed.  This is the only
 *             one that makes "digitwm -n" exit non-zero.
 *
 * The split, and where each half is argued, is doc/macos-install.md, "What of
 * cwmrc is read".  It is not a matter of effort: colours describe a border
 * the port cannot draw on another application's window, groups describe
 * Spaces that have no public API, struts describe an EWMH macOS has never
 * had.  doc/macos.md named all three as losses of the port before a line of
 * this file existed.
 *
 * This is not a second parser for the same grammar in the same binary: parse.y
 * builds into cwm, this builds into digitwm, and no build contains both.  The
 * grammar is written down once, in cwmrc.5, and both read it.
 */

#ifndef _WSI_CONF_H_
#define _WSI_CONF_H_

#include <stddef.h>

#include "confpath.h"

/*
 * The commands that mean something with no X server under them.  Deliberately
 * the ribbon's own list and nothing else: conf.c's name_to_func has 160-odd
 * entries, and all but these ask for a border, a group, a menu, a pointer warp
 * or an EWMH property - see doc/macos-install.md for the tally and the reason
 * against each.
 */
enum wsi_cmd {
	WSI_CMD_NONE = 0,
	WSI_CMD_FOCUS_LEFT,
	WSI_CMD_FOCUS_RIGHT,
	WSI_CMD_FOCUS_UP,
	WSI_CMD_FOCUS_DOWN,
	WSI_CMD_MOVE_LEFT,
	WSI_CMD_MOVE_RIGHT,
	WSI_CMD_MOVE_UP,
	WSI_CMD_MOVE_DOWN,
	WSI_CMD_SWAP_LEFT,
	WSI_CMD_SWAP_RIGHT,
	WSI_CMD_WIDTH_CYCLE,
	WSI_CMD_WIDTH_GROW,
	WSI_CMD_WIDTH_SHRINK,
	WSI_CMD_CENTER,
	WSI_CMD_FLOAT,
	WSI_CMD_QUIT,
	WSI_CMD_NITEMS
};

#define WSICONF_MAXBIND	64
#define WSICONF_KEYLEN	24

struct wsiconf_bind {
	unsigned int	 mods;		/* WSIK_* from macos/wsi_key.h */
	char		 key[WSICONF_KEYLEN];
	int		 cmd;		/* enum wsi_cmd */
};

/* What the last wsiconf_load() made of the file, for "digitwm -n". */
struct wsiconf_report {
	int		 lines;		/* directives seen */
	int		 taken;		/* acted on */
	int		 skipped;	/* understood, and X11's alone */
	int		 bad;		/* not understood, or malformed */
};

/*
 * The settings the port starts from, and the key table it starts with.  Must
 * be called before wsiconf_load(): a cwmrc is a set of changes to these, the
 * same way it is on X11.
 */
void		 wsiconf_default(void);

/*
 * Read one cwmrc.  0 when the file was read (whatever it contained), -1 when
 * it could not be opened at all.  A missing file is not an error to the
 * caller - it is the ordinary case of a person who has not written one.
 */
int		 wsiconf_load(const char *path, struct wsiconf_report *);

/*
 * The configuration file to read when none was named with -c, written into
 * buf, and through src which of the candidates it is - $DIGITWMRC,
 * ~/.digitable/digitwm/digitwmrc or cwm's own ~/.cwmrc.  src may be NULL.
 * The order is confpath_find()'s, which is to say the X11 build's: this is
 * the same file, found the same way, on both machines.
 */
const char	*wsiconf_file(char *buf, size_t len, enum confpath_src *src);

/* The binding table, in the order it will be registered. */
const struct wsiconf_bind	*wsiconf_binds(int *n);

/* "ribbon-focus-left" for WSI_CMD_FOCUS_LEFT. */
const char	*wsiconf_cmdname(int cmd);

/* "^~S-h" for a binding, for messages a person has to act on. */
const char	*wsiconf_bindstr(const struct wsiconf_bind *, char *, size_t);

#endif /* _WSI_CONF_H_ */
