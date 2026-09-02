/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - which configuration file to read, and in which order to look
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
 * The reasoning behind the order is in confpath.h; this file is only the
 * order itself.  Nothing here knows about X11, about macOS, or about the
 * rest of the tree: it is the one file both builds compile unchanged, and it
 * must stay compilable by a compiler that has neither Xlib nor Objective-C.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "confpath.h"

/*
 * The two names, in one place each.  ~/.digitable/ is the family directory
 * (digitdisk(1) keeps ~/.digitable/digitdisk/settings.conf in it); "digitwm"
 * is our room in it, and "digitwmrc" the file.  Changing what a person's
 * configuration is called is changing these three lines and nothing else.
 */
#define CONFPATH_FAMILY	".digitable"
#define CONFPATH_TOOL	"digitwm"
#define CONFPATH_NAME	"digitwmrc"

/* The name cwm reads, kept so that a configuration written for cwm works. */
#define CONFPATH_CWM	".cwmrc"

/* The variable that overrides the search without touching the command line. */
#define CONFPATH_ENVVAR	"DIGITWMRC"

const char *
confpath_home(void)
{
	const char	*home;
	struct passwd	*pw;

	home = getenv("HOME");
	if ((home != NULL) && (*home != '\0'))
		return home;

	pw = getpwuid(getuid());
	if ((pw != NULL) && (pw->pw_dir != NULL) && (*pw->pw_dir != '\0'))
		return pw->pw_dir;

	return ".";
}

/*
 * Join and answer whether it fit.  A truncated path is not a path: acting on
 * the front half of one would read a different file than the person named.
 */
static int
confpath_join(char *buf, size_t len, const char *dir, const char *tail)
{
	int	 n;

	n = snprintf(buf, len, "%s/%s", dir, tail);
	return ((n > 0) && ((size_t)n < len)) ? 0 : -1;
}

/*
 * A file that can be read - not a directory of that name, and not a name
 * that merely exists.  This is the whole of "the file is there".
 */
static int
confpath_readable(const char *path)
{
	struct stat	 sb;

	if (stat(path, &sb) == -1)
		return 0;
	if (!S_ISREG(sb.st_mode))
		return 0;
	return (access(path, R_OK) == 0);
}

enum confpath_src
confpath_find(char *buf, size_t len)
{
	const char	*home, *env;
	char		 legacy[1024];

	if (len == 0)
		return CONFPATH_NONE;
	buf[0] = '\0';

	/*
	 * Named on purpose, so it is taken on purpose: a variable that points
	 * at a file which is not there is a mistake worth hearing about from
	 * the parser, not a reason to quietly read something else.
	 */
	env = getenv(CONFPATH_ENVVAR);
	if ((env != NULL) && (*env != '\0')) {
		if ((size_t)snprintf(buf, len, "%s", env) < len)
			return CONFPATH_ENV;
	}

	home = confpath_home();

	if (confpath_join(buf, len, home,
	    CONFPATH_FAMILY "/" CONFPATH_TOOL "/" CONFPATH_NAME) == -1) {
		buf[0] = '\0';
		return CONFPATH_NONE;
	}
	if (confpath_readable(buf))
		return CONFPATH_OWN;

	/*
	 * Ours is not there.  cwm's name is the fallback, and buf keeps ours
	 * until the fallback is known to exist: when neither exists the answer
	 * is ours, so that a complaint about a file that is not there names
	 * the file we would like written.
	 */
	if (confpath_join(legacy, sizeof(legacy), home, CONFPATH_CWM) == -1)
		return CONFPATH_NONE;
	if (!confpath_readable(legacy))
		return CONFPATH_NONE;
	if ((size_t)snprintf(buf, len, "%s", legacy) >= len)
		return CONFPATH_NONE;

	return CONFPATH_LEGACY;
}

void
confpath_say(enum confpath_src src, const char *path)
{
	static int	 said;
	char		 own[1024];

	if ((src != CONFPATH_LEGACY) || said)
		return;
	said = 1;

	if (confpath_join(own, sizeof(own), confpath_home(),
	    CONFPATH_FAMILY "/" CONFPATH_TOOL "/" CONFPATH_NAME) == -1)
		(void)snprintf(own, sizeof(own), "~/" CONFPATH_FAMILY "/"
		    CONFPATH_TOOL "/" CONFPATH_NAME);

	(void)fprintf(stderr, "digitwm: reading %s, which is cwm's name kept "
	    "for compatibility; ours is %s\n", path, own);
}

/*
 * The seed itself.  Every line is a default, commented out, with the prose
 * above it - the same shape digitdisk writes, and for the same reason: a
 * settings file a person opens and closes again because it is empty has
 * taught them nothing.
 *
 * The values here are not invented: they are conf_init()'s, and `digitwm -n`
 * prints them, so a file that drifted from the code would be caught by
 * running the program.
 */
static const char confpath_template[] =
"# digitwm - the ribbon window manager\n"
"#\n"
"# Written by digitwm the first time it ran, because there was nothing here.\n"
"# It is never rewritten: what you put below survives every upgrade.\n"
"#\n"
"# Every line is a default, commented out.  Uncomment one and change it.\n"
"# \"digitwm -n\" prints what was made of this file; \"man 5 cwmrc\" describes\n"
"# every directive, including the ones this file does not mention.\n"
"#\n"
"# The same file is read by cwm on X11, so a machine of each can share it.\n"
"\n"
"# Lay windows out on the ribbon at all.  \"no\" leaves every window where the\n"
"# application put it, and digitwm becomes a key-binding daemon.\n"
"#ribbon yes\n"
"\n"
"# Park windows that are off the viewport just past the edge instead of\n"
"# leaving them where they are.  On macOS a parked window still shows a\n"
"# 1-pixel strip: the system will not let a window off the screen entirely.\n"
"#ribbonhide no\n"
"\n"
"# Move the pointer to the window that takes the focus.  macOS has no public\n"
"# call that moves the pointer, so on a Mac this does nothing whichever way\n"
"# it is set - see doc/macos-install.md.\n"
"#ribbonwarp no\n"
"\n"
"# Pixels between columns, and between a column and the screen edge.\n"
"#ribbongap 8\n"
"\n"
"# A column narrower or shorter than this is not offered: below it a window\n"
"# is not a window, it is a sliver.\n"
"#ribbonminwidth 120\n"
"#ribbonminheight 60\n"
"\n"
"# The four column widths, as percentages of the viewport, that the width key\n"
"# cycles through.  Control-Option-Comma and Control-Option-Period step them.\n"
"#ribbonwidths 33 50 67 100\n"
"\n"
"# Border drawn around a window.  macOS does not let one program draw on\n"
"# another program's window, so on a Mac this is read and not used.\n"
"#borderwidth 1\n"
"\n"
"# Your own key bindings go here.  \"digitwm -k\" prints the table as it\n"
"# currently stands, with the command each combination runs.\n"
"#bind-key CM-h ribbon-focus-left\n";

int
confpath_seed(char *buf, size_t len)
{
	const char	*home;
	char		 dir[1024], legacy[1024];
	size_t		 want;
	ssize_t		 wrote;
	int		 fd;

	if (len == 0)
		return 0;
	buf[0] = '\0';

	/* Rule 3: somebody named a file; that is the file. */
	if (getenv(CONFPATH_ENVVAR) != NULL &&
	    *getenv(CONFPATH_ENVVAR) != '\0')
		return 0;

	home = confpath_home();

	if (confpath_join(buf, len, home,
	    CONFPATH_FAMILY "/" CONFPATH_TOOL "/" CONFPATH_NAME) == -1) {
		buf[0] = '\0';
		return 0;
	}

	/* Rule 1: never rewrite. */
	if (confpath_readable(buf))
		return 0;

	/*
	 * Rule 4.  Written out rather than hinted at, because the failure it
	 * prevents is silent: our file wins the search, so a seed in front of
	 * a real ~/.cwmrc replaces somebody's configuration with the defaults
	 * and says nothing.
	 */
	if (confpath_join(legacy, sizeof(legacy), home, CONFPATH_CWM) == 0 &&
	    confpath_readable(legacy)) {
		(void)fprintf(stderr, "digitwm: %s is yours and is being read; "
		    "no %s was written,\n         because it would win the "
		    "search and hide it.  To move over:\n"
		    "         mkdir -p %s/" CONFPATH_FAMILY "/" CONFPATH_TOOL
		    " && cp %s %s\n", legacy, buf, home, legacy, buf);
		return 0;
	}

	if (confpath_join(dir, sizeof(dir), home, CONFPATH_FAMILY) == -1)
		return 0;
	if (mkdir(dir, 0755) == -1 && errno != EEXIST)
		return -1;
	if (confpath_join(dir, sizeof(dir), home,
	    CONFPATH_FAMILY "/" CONFPATH_TOOL) == -1)
		return 0;
	if (mkdir(dir, 0755) == -1 && errno != EEXIST)
		return -1;

	/*
	 * O_EXCL rather than O_TRUNC, and it is not paranoia: two digitwm
	 * started at once on one account would otherwise race, and the loser
	 * would truncate the winner's file.  Rule 1 says never rewrite, and
	 * this is rule 1 held even against ourselves.
	 */
	fd = open(buf, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd == -1)
		return (errno == EEXIST) ? 0 : -1;

	want = sizeof(confpath_template) - 1;
	wrote = write(fd, confpath_template, want);
	if (close(fd) == -1 || wrote != (ssize_t)want) {
		(void)unlink(buf);
		return -1;
	}

	(void)fprintf(stderr, "digitwm: wrote %s - every setting there is, "
	    "commented out.\n", buf);
	return 1;
}
