/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - reading cwmrc where half of cwmrc has nothing to describe
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
 * No X11 name appears in this file and none may: it is checked by the same
 * "nm -u" step macos/check.sh runs over the rest of the port.  What it needs
 * from calmwm.h is one structure, struct conf, because that is the record the
 * ribbon reads its settings out of - the ribbon is not being changed for
 * macOS, so the record it reads is not either.
 */

#include <sys/types.h>
#include "queue.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calmwm.h"
#include "confpath.h"
#include "wsi_conf.h"
#include "wsi_key.h"

/*
 * THE DEFAULT KEYS ARE NOT THE X11 DEFAULT KEYS, AND THAT IS A DECISION.
 *
 * conf.c binds the ribbon to Mod4 - the key a PC keyboard prints a window on
 * and a Mac keyboard prints Command on - "because upstream cwm leaves it
 * entirely free" (conf.c:280).  On a Mac it is the opposite of free: Command-H
 * hides an application, Command-Q quits it, Command-C copies, and
 * RegisterEventHotKey takes a combination away from every application at once.
 * A port that kept the X11 table would, on its first run, take Command-H,
 * Command-J, Command-K, Command-L, Command-C, Command-F, Command-R,
 * Command-minus and Command-equal away from the entire desk.  That is not a
 * window manager, that is a broken Mac.
 *
 * So the base is Control-Option, which macOS itself uses for almost nothing,
 * and the shape of the X11 table is kept exactly: base for focus, base+Shift
 * to carry a window, base+Command to exchange whole columns.  A person who
 * wants the X11 keys back writes them in his digitwmrc, where "4-" means
 * Command, and gets what he asked for including the consequence.
 */
static const struct {
	unsigned int	 mods;
	const char	*key;
	int		 cmd;
} default_binds[] = {
	{ WSIK_CONTROL|WSIK_OPTION,			"h",
	    WSI_CMD_FOCUS_LEFT },
	{ WSIK_CONTROL|WSIK_OPTION,			"l",
	    WSI_CMD_FOCUS_RIGHT },
	{ WSIK_CONTROL|WSIK_OPTION,			"k",
	    WSI_CMD_FOCUS_UP },
	{ WSIK_CONTROL|WSIK_OPTION,			"j",
	    WSI_CMD_FOCUS_DOWN },
	{ WSIK_CONTROL|WSIK_OPTION|WSIK_SHIFT,		"h",
	    WSI_CMD_MOVE_LEFT },
	{ WSIK_CONTROL|WSIK_OPTION|WSIK_SHIFT,		"l",
	    WSI_CMD_MOVE_RIGHT },
	{ WSIK_CONTROL|WSIK_OPTION|WSIK_SHIFT,		"k",
	    WSI_CMD_MOVE_UP },
	{ WSIK_CONTROL|WSIK_OPTION|WSIK_SHIFT,		"j",
	    WSI_CMD_MOVE_DOWN },
	{ WSIK_CONTROL|WSIK_OPTION|WSIK_COMMAND,	"h",
	    WSI_CMD_SWAP_LEFT },
	{ WSIK_CONTROL|WSIK_OPTION|WSIK_COMMAND,	"l",
	    WSI_CMD_SWAP_RIGHT },
	{ WSIK_CONTROL|WSIK_OPTION,			"r",
	    WSI_CMD_WIDTH_CYCLE },
	{ WSIK_CONTROL|WSIK_OPTION,			"equal",
	    WSI_CMD_WIDTH_GROW },
	{ WSIK_CONTROL|WSIK_OPTION,			"minus",
	    WSI_CMD_WIDTH_SHRINK },
	{ WSIK_CONTROL|WSIK_OPTION,			"c",
	    WSI_CMD_CENTER },
	{ WSIK_CONTROL|WSIK_OPTION,			"f",
	    WSI_CMD_FLOAT },
	{ WSIK_CONTROL|WSIK_OPTION,			"q",
	    WSI_CMD_QUIT },
};

static const struct {
	const char	*name;
	int		 cmd;
} cmd_names[] = {
	{ "ribbon-focus-left",		WSI_CMD_FOCUS_LEFT },
	{ "ribbon-focus-right",		WSI_CMD_FOCUS_RIGHT },
	{ "ribbon-focus-up",		WSI_CMD_FOCUS_UP },
	{ "ribbon-focus-down",		WSI_CMD_FOCUS_DOWN },
	{ "ribbon-move-left",		WSI_CMD_MOVE_LEFT },
	{ "ribbon-move-right",		WSI_CMD_MOVE_RIGHT },
	{ "ribbon-move-up",		WSI_CMD_MOVE_UP },
	{ "ribbon-move-down",		WSI_CMD_MOVE_DOWN },
	{ "ribbon-column-swap-left",	WSI_CMD_SWAP_LEFT },
	{ "ribbon-column-swap-right",	WSI_CMD_SWAP_RIGHT },
	{ "ribbon-width-cycle",		WSI_CMD_WIDTH_CYCLE },
	{ "ribbon-width-grow",		WSI_CMD_WIDTH_GROW },
	{ "ribbon-width-shrink",	WSI_CMD_WIDTH_SHRINK },
	{ "ribbon-center",		WSI_CMD_CENTER },
	{ "ribbon-float-toggle",	WSI_CMD_FLOAT },
	{ "quit",			WSI_CMD_QUIT },
};

/*
 * Understood, and belonging to a part of the window system this platform does
 * not have.  Each carries the reason, because "ignored" without a reason is
 * indistinguishable from a bug, and the person reading it is trying to decide
 * whether to keep the line in his file.
 *
 * doc/macos.md argued every one of these as a loss of the port before this
 * file existed; nothing new is decided here.
 */
static const struct {
	const char	*name;
	const char	*why;
} skipped_keys[] = {
	{ "activeborder",	"no border: macOS has no writable attribute "
				"for another application's frame" },
	{ "inactiveborder",	"no border" },
	{ "urgencyborder",	"no border" },
	{ "groupborder",	"no border, and no groups" },
	{ "ungroupborder",	"no border, and no groups" },
	{ "borderwidth",	"no border; the ribbon computes with zero" },
	{ "color",		"no border and no menu to colour" },
	{ "font",		"no menu to draw" },
	{ "fontname",		"no menu to draw" },
	{ "selfont",		"no menu to draw" },
	{ "menubg",		"no menu" },
	{ "menufg",		"no menu" },
	{ "gap",		"no EWMH struts: NSScreen.visibleFrame has "
				"the Dock and the menu bar off already" },
	{ "snapdist",		"no pointer-driven move or resize" },
	{ "moveamount",		"no pointer-driven move or resize" },
	{ "htile",		"tiling by key is an X11 window operation" },
	{ "vtile",		"tiling by key is an X11 window operation" },
	{ "sticky",		"no groups: Spaces have no public API" },
	{ "autogroup",		"no groups" },
	{ "ignore",		"needs a window class the port does not read" },
	{ "ribbonrule",		"needs a window class the port does not read" },
	{ "command",		"the Dock and Spotlight launch programs here" },
	{ "wm",			"there is no other window manager to hand to" },
	{ "bind-mouse",		"no pointer grab: the port installs no event "
				"tap, see wsip_pointer_warp()" },
	{ "unbind-mouse",	"no pointer grab" },
};

static struct wsiconf_bind	 binds[WSICONF_MAXBIND];
static int			 nbinds;

/* The file being read, so that a complaint names it rather than its kind. */
static const char		*conf_name = "digitwmrc";

static int	 conf_line(char *, int, struct wsiconf_report *);
static int	 conf_tokens(char *, char **, int);
static int	 conf_yesno(const char *, int *);
static int	 conf_number(const char *, int, int, int *);
static int	 conf_bind(const char *, unsigned int *, const char **);
static int	 conf_cmd(const char *);
static void	 conf_unbind(unsigned int, const char *);
static int	 conf_addbind(unsigned int, const char *, int);

void
wsiconf_default(void)
{
	unsigned int	 i;

	(void)memset(&Conf, 0, sizeof(Conf));

	/*
	 * The same numbers conf.c:334-343 starts from.  They are the ribbon's
	 * defaults rather than X11's, so they carry over whole - except
	 * bwidth, which is zero here and not a choice (doc/macos.md, loss of
	 * the border) and ribbonwarp, which is off because this port cannot
	 * move the pointer at all: wsip_pointer_warp() answers -1 and wsi.h
	 * permits it only in company with "focus does not follow the pointer".
	 */
	Conf.ribbon = 1;
	Conf.ribbonhide = 0;
	Conf.ribbonwarp = 0;
	Conf.ribbongap = 8;
	Conf.ribbonminw = 120;
	Conf.ribbonminh = 60;
	Conf.ribbonwidth[0] = 33;
	Conf.ribbonwidth[1] = 50;
	Conf.ribbonwidth[2] = 67;
	Conf.ribbonwidth[3] = 100;
	Conf.bwidth = 0;

	nbinds = 0;
	(void)memset(binds, 0, sizeof(binds));
	for (i = 0; i < sizeof(default_binds) / sizeof(default_binds[0]); i++)
		(void)conf_addbind(default_binds[i].mods, default_binds[i].key,
		    default_binds[i].cmd);
}

/*
 * Where the settings live is not this port's decision to make on its own:
 * conf.c has to arrive at the same file, or the promise that one file
 * describes both machines is only a sentence in a document.  So the order
 * lives in confpath.c, which both builds compile, and this is the port's
 * doorway to it.
 */
const char *
wsiconf_file(char *buf, size_t len, enum confpath_src *src)
{
	enum confpath_src	 found;

	found = confpath_find(buf, len);
	if (src != NULL)
		*src = found;
	return buf;
}

const struct wsiconf_bind *
wsiconf_binds(int *n)
{
	if (n != NULL)
		*n = nbinds;
	return binds;
}

const char *
wsiconf_cmdname(int cmd)
{
	unsigned int	 i;

	for (i = 0; i < sizeof(cmd_names) / sizeof(cmd_names[0]); i++) {
		if (cmd_names[i].cmd == cmd)
			return cmd_names[i].name;
	}
	return "?";
}

const char *
wsiconf_bindstr(const struct wsiconf_bind *b, char *buf, size_t len)
{
	char	 mods[8];
	size_t	 n = 0;

	if (b->mods & WSIK_CONTROL)
		mods[n++] = 'C';
	if (b->mods & WSIK_OPTION)
		mods[n++] = 'M';
	if (b->mods & WSIK_SHIFT)
		mods[n++] = 'S';
	if (b->mods & WSIK_COMMAND)
		mods[n++] = '4';
	mods[n] = '\0';

	(void)snprintf(buf, len, "%s%s%s", mods, (n > 0) ? "-" : "", b->key);
	return buf;
}

int
wsiconf_load(const char *path, struct wsiconf_report *rep)
{
	FILE	*fp;
	char	 line[1024];
	int	 lineno = 0;

	if (rep != NULL)
		(void)memset(rep, 0, sizeof(*rep));

	conf_name = path;
	if ((fp = fopen(path, "r")) == NULL)
		return -1;

	while (fgets(line, sizeof(line), fp) != NULL) {
		lineno++;
		if (conf_line(line, lineno, rep) != 0)
			continue;
	}
	(void)fclose(fp);
	return 0;
}

/*
 * One line.  Comments are stripped, then the line is cut into words, then the
 * first word decides.  Three outcomes and all three are counted, because
 * "digitwm -n" prints the three numbers and a person deciding whether his
 * file is being read needs them.
 */
static int
conf_line(char *line, int lineno, struct wsiconf_report *rep)
{
	char		*tok[8];
	const char	*key, *why = NULL;
	unsigned int	 i, mods;
	int		 n, v, a, b, c, d;

	n = conf_tokens(line, tok, 8);
	if (n <= 0)
		return 0;
	if (rep != NULL)
		rep->lines++;

	for (i = 0; i < sizeof(skipped_keys) / sizeof(skipped_keys[0]); i++) {
		if (strcmp(tok[0], skipped_keys[i].name) == 0) {
			why = skipped_keys[i].why;
			break;
		}
	}
	if (why != NULL) {
		(void)fprintf(stderr,
		    "digitwm: %s:%d: \"%s\" is X11's alone - %s\n",
		    conf_name, lineno, tok[0], why);
		if (rep != NULL)
			rep->skipped++;
		return 0;
	}

	if (strcmp(tok[0], "ribbon") == 0 && n == 2 &&
	    conf_yesno(tok[1], &v) == 0)
		Conf.ribbon = v;
	else if (strcmp(tok[0], "ribbonhide") == 0 && n == 2 &&
	    conf_yesno(tok[1], &v) == 0)
		Conf.ribbonhide = v;
	else if (strcmp(tok[0], "ribbonwarp") == 0 && n == 2 &&
	    conf_yesno(tok[1], &v) == 0) {
		/*
		 * Allowed to be set, and allowed to do nothing: the ribbon
		 * asks client_ptr_warp() to carry the pointer, macos/wsi_ax.m
		 * answers -1 every time, and the counter that says so is
		 * warp_refused in struct wsi_stats.  Refusing the setting
		 * outright would be a lie in the other direction - the line is
		 * meaningful, the platform is not able.
		 */
		Conf.ribbonwarp = v;
		if (v)
			(void)fprintf(stderr, "digitwm: %s:%d: ribbonwarp yes: "
			    "this platform cannot move the pointer, so it "
			    "will not be carried\n", conf_name, lineno);
	} else if (strcmp(tok[0], "ribbongap") == 0 && n == 2 &&
	    conf_number(tok[1], 0, INT_MAX, &v) == 0)
		Conf.ribbongap = v;
	else if (strcmp(tok[0], "ribbonminwidth") == 0 && n == 2 &&
	    conf_number(tok[1], 1, INT_MAX, &v) == 0)
		Conf.ribbonminw = v;
	else if (strcmp(tok[0], "ribbonminheight") == 0 && n == 2 &&
	    conf_number(tok[1], 1, INT_MAX, &v) == 0)
		Conf.ribbonminh = v;
	else if (strcmp(tok[0], "ribbonwidths") == 0 && n == 5 &&
	    conf_number(tok[1], 1, 100, &a) == 0 &&
	    conf_number(tok[2], 1, 100, &b) == 0 &&
	    conf_number(tok[3], 1, 100, &c) == 0 &&
	    conf_number(tok[4], 1, 100, &d) == 0) {
		Conf.ribbonwidth[0] = a;
		Conf.ribbonwidth[1] = b;
		Conf.ribbonwidth[2] = c;
		Conf.ribbonwidth[3] = d;
	} else if (strcmp(tok[0], "unbind-key") == 0 && n == 2) {
		if (strcmp(tok[1], "all") == 0)
			nbinds = 0;
		else if (conf_bind(tok[1], &mods, &key) == 0)
			conf_unbind(mods, key);
		else
			goto bad;
	} else if (strcmp(tok[0], "bind-key") == 0 && n == 3) {
		if (conf_bind(tok[1], &mods, &key) != 0)
			goto bad;
		if ((v = conf_cmd(tok[2])) == WSI_CMD_NONE) {
			(void)fprintf(stderr, "digitwm: %s:%d: \"%s\" is not a "
			    "command this platform has; the ribbon's commands "
			    "are the ones \"digitwm -k\" lists\n",
			    conf_name, lineno, tok[2]);
			if (rep != NULL)
				rep->skipped++;
			return 0;
		}
		conf_unbind(mods, key);
		if (conf_addbind(mods, key, v) != 0) {
			(void)fprintf(stderr, "digitwm: %s:%d: more than %d "
			    "bindings\n", conf_name, lineno, WSICONF_MAXBIND);
			goto bad;
		}
		if (mods == WSIK_COMMAND)
			(void)fprintf(stderr, "digitwm: %s:%d: Command-%s will "
			    "be taken from every application on this Mac\n",
			    conf_name, lineno, key);
		/*
		 * Apple's rule, not ours: macOS Sequoia refuses to register a
		 * hot key whose modifiers are only Shift and Option, and one
		 * with no modifier at all was never registrable.  Said here,
		 * at the line, rather than left to come back as a refusal with
		 * no reason attached.
		 */
		if ((mods & ~(WSIK_SHIFT | WSIK_OPTION)) == 0)
			(void)fprintf(stderr, "digitwm: %s:%d: this Mac may "
			    "refuse the combination: macOS wants at least one "
			    "modifier that is not Shift and not Option - add "
			    "C (Control) or 4 (Command)\n",
			    conf_name, lineno);
	} else
		goto bad;

	if (rep != NULL)
		rep->taken++;
	return 0;

bad:
	(void)fprintf(stderr, "digitwm: %s:%d: cannot make sense of \"%s\"\n",
	    conf_name, lineno, tok[0]);
	if (rep != NULL)
		rep->bad++;
	return -1;
}

/*
 * Cut a line into words.  Double quotes hold a word together, "#" outside them
 * starts a comment.  This is cwmrc's lexer as far as the directives above use
 * it and no further: macro definitions and includes are parse.y's and belong
 * to a file the X11 build reads.
 */
static int
conf_tokens(char *line, char **tok, int max)
{
	int	 n = 0;
	char	*p = line;

	while (*p != '\0' && n < max) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (*p == '\0' || *p == '#')
			break;
		if (*p == '"') {
			tok[n++] = ++p;
			while (*p != '\0' && *p != '"')
				p++;
		} else {
			tok[n++] = p;
			while (*p != '\0' && *p != ' ' && *p != '\t' &&
			    *p != '\n' && *p != '\r')
				p++;
		}
		if (*p != '\0')
			*p++ = '\0';
	}
	return n;
}

static int
conf_yesno(const char *s, int *out)
{
	if (strcmp(s, "yes") == 0) {
		*out = 1;
		return 0;
	}
	if (strcmp(s, "no") == 0) {
		*out = 0;
		return 0;
	}
	return -1;
}

static int
conf_number(const char *s, int lo, int hi, int *out)
{
	char		*end;
	long		 v;

	v = strtol(s, &end, 10);
	if (end == s || *end != '\0' || v < (long)lo || v > (long)hi)
		return -1;
	*out = (int)v;
	return 0;
}

/*
 * "4CS-h" into modifiers and a key name, the way conf_bind_mask() does it, and
 * with the same letters - because the file is the same file.  The one letter
 * that has no Mac counterpart is "5" (Mod5): a Mac keyboard has four modifiers
 * a hot key may use, and inventing a fifth would be inventing.
 */
static int
conf_bind(const char *name, unsigned int *mods, const char **key)
{
	const char	*dash, *ch;

	*mods = 0;
	if ((dash = strchr(name, '-')) == NULL) {
		*key = name;
		return (*name == '\0') ? -1 : 0;
	}
	for (ch = name; ch < dash; ch++) {
		switch (*ch) {
		case 'S':
			*mods |= WSIK_SHIFT;
			break;
		case 'C':
			*mods |= WSIK_CONTROL;
			break;
		case 'M':
			*mods |= WSIK_OPTION;
			break;
		case '4':
			*mods |= WSIK_COMMAND;
			break;
		default:
			return -1;
		}
	}
	*key = dash + 1;
	return (**key == '\0') ? -1 : 0;
}

static int
conf_cmd(const char *name)
{
	unsigned int	 i;

	for (i = 0; i < sizeof(cmd_names) / sizeof(cmd_names[0]); i++) {
		if (strcmp(cmd_names[i].name, name) == 0)
			return cmd_names[i].cmd;
	}
	return WSI_CMD_NONE;
}

static void
conf_unbind(unsigned int mods, const char *key)
{
	int	 i, j;

	for (i = 0; i < nbinds; i++) {
		if (binds[i].mods != mods || strcmp(binds[i].key, key) != 0)
			continue;
		for (j = i; j < nbinds - 1; j++)
			binds[j] = binds[j + 1];
		nbinds--;
		i--;
	}
}

static int
conf_addbind(unsigned int mods, const char *key, int cmd)
{
	if (nbinds >= WSICONF_MAXBIND)
		return -1;
	if (strlen(key) >= WSICONF_KEYLEN)
		return -1;
	binds[nbinds].mods = mods;
	(void)snprintf(binds[nbinds].key, WSICONF_KEYLEN, "%s", key);
	binds[nbinds].cmd = cmd;
	nbinds++;
	return 0;
}

/*
 * The ribbon asks this on every insertion.  The answer is the same one
 * tools/wasm-layout/shim.c and macos/wsicheck.c give, and for the same reason:
 * a "ribbonrule" line names a window by its class, the port does not read a
 * class off a window, and an answer invented out of nothing would put windows
 * in columns nobody asked for.  The directive is reported as skipped when it
 * appears in a file, so the silence is a named silence.
 */
int
conf_ribbonrule_match(struct client_ctx *cc)
{
	(void)cc;
	return RIBBON_RULE_NONE;
}
