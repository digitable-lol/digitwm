/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - the process
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
 * Argument handling and one call.  Everything the process actually does is in
 * macos/wsi_run.c, which is checked on a machine with no macOS; this file is
 * the part that could only be checked by starting it, so there is as little of
 * it as it is possible to have.
 */

#include <sys/types.h>
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"
#include "confpath.h"
#include "wsi_conf.h"
#include "wsi_core.h"
#include "wsi_key.h"
#include "wsi_run.h"

/*
 * usage() is not static: calmwm.h declares it, because cwm has one too, and a
 * static definition of a name the header has already declared extern is an
 * error rather than a shadowing.  One header, two programs.
 */
void		 usage(void);
static void	 show_keys(void);

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: digitwm [-hkNn] [-c file]\n"
	    "  -c file  read this instead of the file the search would find:\n"
	    "           $DIGITWMRC, then ~/.digitable/digitwm/digitwmrc, then\n"
	    "           cwm's own ~/.cwmrc\n"
	    "  -n       read the configuration, say what was made of it, stop\n"
	    "  -k       print the key table and the commands it can name\n"
	    "  -N       go down the Apple calls one at a time and say which "
	    "answered\n"
	    "  -h       this\n");
	exit(1);
}

static void
show_keys(void)
{
	const struct wsiconf_bind	*b;
	char				 buf[64];
	int				 i, n;

	b = wsiconf_binds(&n);
	(void)printf("The key table, %d binding(s).  S=Shift C=Control "
	    "M=Option 4=Command,\nthe letters cwmrc(5) uses, so that one file "
	    "describes both machines.\n\n", n);
	for (i = 0; i < n; i++)
		(void)printf("  bind-key %-12s %s\n",
		    wsiconf_bindstr(&b[i], buf, sizeof(buf)),
		    wsiconf_cmdname(b[i].cmd));
	(void)printf("\nThe commands above are the whole list this platform "
	    "has.  Everything else\ncwmrc(5) names asks for a border, a group, "
	    "a menu or a pointer grab, none of\nwhich macOS gives a window "
	    "manager - doc/macos-install.md says which and why.\n");
}

int
main(int argc, char **argv)
{
	struct wsiconf_report	 rep;
	enum confpath_src	 src = CONFPATH_ARG;
	char			 buf[1024];
	const char		*path = NULL;
	int			 ch, mode = 0, refused;

	wsiconf_default();

	while ((ch = getopt(argc, argv, "c:hkNn")) != -1) {
		switch (ch) {
		case 'c':
			path = optarg;
			break;
		case 'k':
			mode = 'k';
			break;
		case 'N':
			mode = 'N';
			break;
		case 'n':
			mode = 'n';
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	/*
	 * A configuration nobody has written yet is the ordinary case and not
	 * a failure; a missing file that was named on the command line is a
	 * failure, because somebody meant that file.
	 *
	 * Which file the search settles on, and the one line said out loud
	 * when it is cwm's old ~/.cwmrc, are confpath.c's - the same code the
	 * X11 build runs, so that neither machine can quietly start reading
	 * something the other does not.
	 */
	if (path == NULL) {
		path = wsiconf_file(buf, sizeof(buf), &src);
		confpath_say(src, path);
		if (wsiconf_load(path, &rep) != 0)
			(void)memset(&rep, 0, sizeof(rep));
	} else if (wsiconf_load(path, &rep) != 0) {
		(void)fprintf(stderr, "digitwm: %s: cannot read it\n", path);
		return 1;
	}

	if (mode == 'k') {
		show_keys();
		return 0;
	}

	if (mode == 'n') {
		(void)printf("%s: %d directive(s): %d taken, %d X11's alone, "
		    "%d not understood\n", path, rep.lines, rep.taken,
		    rep.skipped, rep.bad);
		(void)printf("ribbon %s, ribbonhide %s, ribbonwarp %s, "
		    "ribbongap %d, ribbonminwidth %d,\nribbonminheight %d, "
		    "ribbonwidths %d %d %d %d\n",
		    Conf.ribbon ? "yes" : "no",
		    Conf.ribbonhide ? "yes" : "no",
		    Conf.ribbonwarp ? "yes" : "no",
		    Conf.ribbongap, Conf.ribbonminw, Conf.ribbonminh,
		    Conf.ribbonwidth[0], Conf.ribbonwidth[1],
		    Conf.ribbonwidth[2], Conf.ribbonwidth[3]);
		return (rep.bad > 0) ? 1 : 0;
	}

	if (mode == 'N')
		return (wsi_run_doctor() > 0) ? 1 : 0;

	/*
	 * The one failure a person can do something about, and the only place
	 * this program says the word "Accessibility" out loud on the way in.
	 * The system dialogue has already been raised by then -
	 * kAXTrustedCheckOptionPrompt does that - and it does not come back to
	 * this process with an answer: the grant is read at start-up, so the
	 * program has to be started again once it is given.
	 */
	if (wsi_run_init() != 0) {
		(void)fprintf(stderr,
		    "digitwm: this binary has not been granted Accessibility, "
		    "and without it it\n"
		    "cannot see or move a single window.  macOS should have "
		    "just asked; if it did\n"
		    "not, open System Settings > Privacy & Security > "
		    "Accessibility and add\n"
		    "digitwm there.  Then start it again - the grant is read "
		    "once, at start-up.\n");
		return 1;
	}

	(void)printf("digitwm: %lu window(s) on the ribbon.\n",
	    wsi_core_stats()->opened);

	if ((refused = wsi_run_keys()) < 0)
		return 1;
	if (refused > 0)
		(void)fprintf(stderr, "digitwm: %d key combination(s) refused; "
		    "the rest work.  \"digitwm -k\"\nprints the table, "
		    "cwmrc(5) says how to change it.\n", refused);

	wsi_run_loop();
	wsik_close();
	return 0;
}
