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
