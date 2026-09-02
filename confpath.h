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
 * One order, one file, both builds.
 *
 * The X11 build finds its configuration in conf.c and the macOS build in
 * macos/wsi_conf.c, and until this file existed each of them spelled the
 * path out for itself - the same literal "~/.cwmrc" written twice.  Two
 * copies of a search order diverge the first time one of them is changed,
 * and the claim doc/macos.md rests on ("one file describes both machines")
 * would stop being true without anybody noticing.  So the order lives here,
 * in plain C with no X11 and no Objective-C in sight, and both builds link
 * the same object file.
 *
 * The order, from strongest to weakest:
 *
 *   1. the file named with -c.  Somebody meant that file; nothing overrides
 *      it, and a missing one is an error rather than a reason to look on.
 *   2. $DIGITWMRC, when it is set and not empty.  Set-but-missing behaves
 *      like -c and for the same reason: it was named on purpose.
 *   3. ~/.digitable/digitwm/digitwmrc - our own name in the family's own
 *      directory.  ~/.digitable/ is not this program's invention: digitdisk
 *      keeps ~/.digitable/digitdisk/settings.conf there (digitdisk(1),
 *      "FILES"), and its own source says the directory is the family's and
 *      the tools beside it keep their settings in it.  Hence <family>/<tool>
 *      /<file> rather than a bare file dropped in the family directory.
 *   4. ~/.cwmrc - the file cwm reads.  Whoever arrived from cwm keeps
 *      working, and confpath_say() says out loud, once, that this is the
 *      file being read, so that nobody lives on the old path unawares.
 *
 * When neither 3 nor 4 exists the answer is 3: a person who has written no
 * configuration at all should be told about our name, not cwm's, if anything
 * downstream ever names the file it did not find.
 */

#ifndef _CONFPATH_H_
#define _CONFPATH_H_

#include <stddef.h>

enum confpath_src {
	CONFPATH_ARG,		/* named with -c */
	CONFPATH_ENV,		/* $DIGITWMRC */
	CONFPATH_OWN,		/* ~/.digitable/digitwm/digitwmrc */
	CONFPATH_LEGACY,	/* ~/.cwmrc, the name cwm uses */
	CONFPATH_NONE		/* none of them exists; buf holds CONFPATH_OWN */
};

/*
 * Write the configuration file to read into buf and say which of the four it
 * turned out to be.  Steps 2 to 4 only: -c is the caller's business, because
 * only the caller has seen the command line.
 */
enum confpath_src	 confpath_find(char *buf, size_t len);

/*
 * One line on stderr when the file being read is ~/.cwmrc, and nothing at all
 * otherwise.  Says it once per process however often it is called: the two
 * builds call it from different places and a person needs to hear it once.
 */
void			 confpath_say(enum confpath_src, const char *path);

/*
 * The home directory: $HOME, then the passwd entry, then "." - the same
 * three steps both builds used to take separately.
 */
const char		*confpath_home(void);

/*
 * WRITE THE FILE THE PERSON IS ABOUT TO LOOK FOR - once, and only when there
 * is nothing to lose by writing it.
 *
 * The owner went to edit ~/.digitable/digitwm/digitwmrc on the first real Mac
 * and found neither the file nor the directory.  digitdisk, the tool next
 * door, writes its own on first run (host/internal/settings/settings.go: the
 * directory 0755, the file 0644), and a family in which one tool seeds its
 * settings and the other waits to be guessed at is not a family.
 *
 * FOUR RULES, and the fourth is the one worth arguing about:
 *
 *   1. Only when our file does not exist.  An existing file is never
 *      rewritten - not on upgrade, not on rebuild, not ever.
 *   2. The contents are the defaults, commented out, one line of prose each.
 *      A person who opens it sees what there is to turn without going to the
 *      manual, and a file of comments parses to nothing, so seeding it
 *      changes no behaviour at all.
 *   3. Not when -c or $DIGITWMRC named a file: somebody said which file, and
 *      writing a different one behind their back answers a question nobody
 *      asked.
 *   4. NOT WHEN ~/.cwmrc EXISTS.  Our file wins over cwm's in the search
 *      order, so seeding ours in front of a real ~/.cwmrc would take a person
 *      who has been configuring cwm for years and silently give them the
 *      defaults.  A file of comments is not harmless when it shadows a file
 *      that is not.  In that case nothing is written and the reason is said
 *      out loud, with the command to convert.
 *
 * Returns 1 when a file was written, 0 when there was a good reason not to,
 * -1 when writing was tried and failed.  Nothing here is fatal: a window
 * manager that will not start because it could not write a comment would be
 * worse than one with no seed file.
 */
int			 confpath_seed(char *buf, size_t len);

#endif /* _CONFPATH_H_ */
