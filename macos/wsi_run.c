/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - starting, turning and stopping the ribbon on somebody else's desk
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
 * Everything in this file is ordinary C and is checked on a machine with no
 * macOS: macos/runcheck.c runs the whole of it - the start-up sequence, the
 * command dispatch, one turn of the loop - over the window system of memory in
 * macos/wsi_fake.c.  What it cannot check is that the platform underneath
 * answers truthfully, which is the same thing macos/check.sh cannot check and
 * for the same reason.
 *
 * The loop is three things and no more, and it is worth saying which, because
 * a window manager's loop is usually where the manager lives and here it is
 * not: the manager lives in ribbon.c, which decides, and in macos/wsi_core.c,
 * which remembers.  This turns the crank.
 *
 *   1. let the platform deliver what has come due (wsip_pump);
 *   2. if any of it was somebody else moving one of our windows, put the
 *      layout back (ribbon_sync) - that clause is what makes this a window
 *      manager rather than a one-off arrangement;
 *   3. once a second, ask whether the displays are still the displays.
 */

#include <sys/types.h>
#include "queue.h"

#include <stdio.h>
#include <string.h>

#include "calmwm.h"
#include "wsi_core.h"
#include "wsi_conf.h"
#include "wsi_key.h"
#include "wsi_run.h"

/*
 * The settings record the ribbon reads.  It is defined here because this is
 * the file that is in every digitwm and in nothing else; conf.c defines the
 * same symbol for cwm, and no binary contains both.
 */
struct conf		 Conf;

static struct screen_ctx	 sc;
static int			 running;

/*
 * How long one turn of the loop waits for the platform.
 *
 * Not a rate: CFRunLoopRunInMode returns as soon as a source fires, so this is
 * how long the process sleeps when nothing at all is happening.  It bounds one
 * thing only - how late the display poll and the "quit" key can be - and 20 ms
 * is under one frame at 60 Hz, which is the smallest unit anything here is
 * measured in.  doc/macos.md's flicker is not this number: that one is paid
 * before the notification even reaches us.
 */
#define WSI_PUMP_MS	20.0

/* How often to ask whether a monitor has been plugged in or pulled out. */
#define WSI_DISPLAY_MS	1000.0

#define WSI_MAXDISPLAY	8

static struct wsip_display	 seen[WSI_MAXDISPLAY];
static int			 nseen;
static double			 checked;

static void	 run_displays(void);
static void	 doctor_line(const char *, const char *, int, const char *);

struct screen_ctx *
wsi_run_screen(void)
{
	return &sc;
}

int
wsi_run_init(void)
{
	(void)memset(&sc, 0, sizeof(sc));
	TAILQ_INIT(&sc.clientq);
	TAILQ_INIT(&sc.regionq);
	TAILQ_INIT(&sc.groupq);
	ribbon_screen_init(&sc);

	/*
	 * Before wsi_core_init() and not after, which is the one ordering
	 * trap in this sequence.  wsi_core_init() opens the platform, and
	 * opening the platform reports every window that is already on the
	 * desk - each of those is given cc->bwidth at the moment it is
	 * reported.  A border width set afterwards would apply to windows
	 * opened later and not to the ones the user is looking at.
	 */
	wsi_core_border(Conf.bwidth);

	if (wsi_core_init(&sc) != 0)
		return -1;

	ribbon_screen_relayout(&sc);
	ribbon_sync(&sc);

	nseen = wsip_displays(seen, WSI_MAXDISPLAY);
	checked = wsip_now();

	/*
	 * Running from here rather than from the first turn of the loop, so
	 * that "quit" means something before the loop is entered - and so that
	 * a harness which owns its own clock can press it and see the answer.
	 */
	running = 1;
	return 0;
}

int
wsi_run_keys(void)
{
	const struct wsiconf_bind	*b;
	char				 buf[64];
	int				 i, n, refused = 0;

	b = wsiconf_binds(&n);
	if (n == 0)
		return 0;

	wsi_trace("keys: %d binding(s) to take", n);

	if (wsik_open() != 0) {
		(void)fprintf(stderr, "digitwm: no keyboard: this process has "
		    "no connection to the window server.  digitwm must be "
		    "started from a logged-in graphical session, not over ssh "
		    "and not from a launchd daemon.\n");
		return -1;
	}

	wsi_trace("keys: wsik_open() ok - the process has a window server");

	for (i = 0; i < n; i++) {
		if (wsik_bind(i, b[i].mods, b[i].key) == 0) {
			wsi_trace("keys: id %d = %s -> %s, taken", i,
			    wsiconf_bindstr(&b[i], buf, sizeof(buf)),
			    wsiconf_cmdname(b[i].cmd));
			continue;
		}
		refused++;
		wsi_trace("keys: id %d = %s -> %s, REFUSED", i,
		    wsiconf_bindstr(&b[i], buf, sizeof(buf)),
		    wsiconf_cmdname(b[i].cmd));
		(void)fprintf(stderr, "digitwm: %s: refused - the key name is "
		    "unknown, or something on this Mac already holds the "
		    "combination (%s will do nothing)\n",
		    wsiconf_bindstr(&b[i], buf, sizeof(buf)),
		    wsiconf_cmdname(b[i].cmd));
	}
	return refused;
}

/*
 * A key came back up.  The identifier is the binding's place in the table,
 * because that table is what was handed down in the first place - nothing
 * below macos/wsi_key.h knows what a ribbon is, and this is where it stops
 * being an integer.
 *
 * The two contexts are conf.c's two contexts, and the fallback is xevents.c's:
 * a command about "the window" acts on the one that has the keyboard, and does
 * nothing at all when no window has it.  On X11 there is a first choice before
 * that - the window under the pointer - and this port has none, because it
 * neither moves the pointer nor lets focus follow it.
 */
/*
 * The state a person needs to see next to a key press, and no more of it.
 *
 * Three displays and a monitor to the LEFT of the main one - the owner's
 * machine on 2 September 2026 - mean windows with NEGATIVE x, and -1996 is
 * what his first window reported.  A ribbon that bound those windows to the
 * wrong output would look exactly like a key that did not arrive: the press
 * lands, the command runs, and it moves the focus on a ribbon nobody is
 * looking at.  So the dump names, for every ribbon, the output it belongs to,
 * whether that output is attached, its viewport, and how many columns it has -
 * and then every window with the ribbon it ended up on.
 */
static void
wsi_run_trace_state(void)
{
	struct region_ctx	*rc;
	struct ribbon		*rb;
	struct client_ctx	*cc;
	int			 n;

	TAILQ_FOREACH(rc, &sc.regionq, entry)
		wsi_trace("  output %d \"%s\" view %d,%d %dx%d work %d,%d %dx%d",
		    rc->num, rc->name == NULL ? "(none)" : rc->name,
		    rc->view.x, rc->view.y, rc->view.w, rc->view.h,
		    rc->work.x, rc->work.y, rc->work.w, rc->work.h);

	TAILQ_FOREACH(rb, &sc.ribbonq, entry) {
		n = ribbon_col_count(rb);
		wsi_trace("  ribbon \"%s\" %s, %d column(s), focus col %d, "
		    "viewport %d,%d %dx%d, offset %d/%d, len %d",
		    rb->output == NULL ? "(none)" : rb->output,
		    rb->active ? "attached" : "DETACHED", n,
		    rb->focus == NULL ? -1 : ribbon_col_index(rb, rb->focus),
		    rb->view.x, rb->view.y, rb->view.w, rb->view.h,
		    rb->offset, rb->voffset, rb->len);
	}

	TAILQ_FOREACH(cc, &sc.clientq, entry) {
		rb = cc->rbcol == NULL ? NULL : cc->rbcol->rb;
		wsi_trace("  window %lu at %d,%d %dx%d -> ribbon \"%s\" "
		    "column %d%s", (unsigned long)cc->win,
		    cc->geom.x, cc->geom.y, cc->geom.w, cc->geom.h,
		    rb == NULL || rb->output == NULL ? "(floating)" : rb->output,
		    cc->rbcol == NULL ? -1 : ribbon_col_index(rb, cc->rbcol),
		    cc == client_current(&sc) ? "  <- has the keyboard" : "");
	}
}

/* One line: the press arrived, and this is what it was taken to mean. */
static void
wsi_note_key_trace(int id, const struct wsiconf_bind *b, char *buf, size_t len)
{
	wsi_trace("key: id %d = %s -> %s ARRIVED in this process", id,
	    wsiconf_bindstr(&b[id], buf, len), wsiconf_cmdname(b[id].cmd));
}

void
wsi_note_key(int id)
{
	const struct wsiconf_bind	*b;
	struct client_ctx		*cc;
	char				 buf[64];
	int				 n;

	b = wsiconf_binds(&n);
	if (id < 0 || id >= n) {
		wsi_trace("key: id %d arrived and there is no such binding "
		    "(%d in the table)", id, n);
		return;
	}

	/*
	 * The line that separates two of the four ways a press dies.  If this
	 * appears, the Carbon handler ran and the identifier survived; if it
	 * does not, the press never reached this process at all, and the place
	 * to look is macos/wsi_key.m and the run loop, not the ribbon.
	 */
	if (wsi_trace_want) {
		wsi_note_key_trace(id, b, buf, sizeof(buf));
		cc = client_current(&sc);
		wsi_trace("key: before - focused window %lu",
		    cc == NULL ? 0UL : (unsigned long)cc->win);
	}

	switch (b[id].cmd) {
	case WSI_CMD_FOCUS_LEFT:
		ribbon_focus_col(&sc, CWM_LEFT);
		goto done;
	case WSI_CMD_FOCUS_RIGHT:
		ribbon_focus_col(&sc, CWM_RIGHT);
		goto done;
	case WSI_CMD_FOCUS_UP:
		ribbon_focus_win(&sc, CWM_UP);
		goto done;
	case WSI_CMD_FOCUS_DOWN:
		ribbon_focus_win(&sc, CWM_DOWN);
		goto done;
	case WSI_CMD_SWAP_LEFT:
		ribbon_swap_col(&sc, CWM_LEFT);
		goto done;
	case WSI_CMD_SWAP_RIGHT:
		ribbon_swap_col(&sc, CWM_RIGHT);
		goto done;
	case WSI_CMD_CENTER:
		ribbon_center(&sc);
		goto done;
	case WSI_CMD_QUIT:
		running = 0;
		goto done;
	default:
		break;
	}

	if ((cc = client_current(&sc)) == NULL) {
		wsi_trace("key: no window holds the keyboard - a command "
		    "about \"the window\" has nothing to act on");
		goto done;
	}

	switch (b[id].cmd) {
	case WSI_CMD_MOVE_LEFT:
		ribbon_move_client(cc, CWM_LEFT);
		break;
	case WSI_CMD_MOVE_RIGHT:
		ribbon_move_client(cc, CWM_RIGHT);
		break;
	case WSI_CMD_MOVE_UP:
		ribbon_move_win(cc, CWM_UP);
		break;
	case WSI_CMD_MOVE_DOWN:
		ribbon_move_win(cc, CWM_DOWN);
		break;
	case WSI_CMD_WIDTH_CYCLE:
		ribbon_width(cc, CWM_CENTER);
		break;
	case WSI_CMD_WIDTH_GROW:
		ribbon_width(cc, CWM_RIGHT);
		break;
	case WSI_CMD_WIDTH_SHRINK:
		ribbon_width(cc, CWM_LEFT);
		break;
	case WSI_CMD_FLOAT:
		ribbon_float_toggle(cc);
		break;
	default:
		break;
	}
 done:
	if (wsi_trace_want) {
		cc = client_current(&sc);
		wsi_trace("key: after  - focused window %lu",
		    cc == NULL ? 0UL : (unsigned long)cc->win);
		wsi_run_trace_state();
	}
}

void
wsi_run_step(double ms)
{
	const struct wsi_stats	*st = wsi_core_stats();
	unsigned long		 before = st->echo_foreign;

	(void)wsip_pump(ms);

	/*
	 * Somebody moved one of our windows and it was not us.  wsi_note_frame()
	 * has already recorded that the window is no longer where it was told
	 * to stand; this is the half that acts on it, and it is deliberately
	 * here and not there - one place decides where a window goes, and it
	 * is the ribbon.
	 */
	if (st->echo_foreign != before)
		ribbon_sync(&sc);

	run_displays();
}

void
wsi_run_loop(void)
{
	while (running)
		wsi_run_step(WSI_PUMP_MS);
}

void
wsi_run_stop(void)
{
	running = 0;
}

int
wsi_run_running(void)
{
	return running;
}

/*
 * Has a monitor come or gone?
 *
 * Asked on a clock rather than subscribed to, and the reason is the one that
 * runs through this whole port: the subscription would be
 * NSApplicationDidChangeScreenParametersNotification or
 * CGDisplayRegisterReconfigurationCallback, and this tree cites neither, so
 * either would be a new name of the lowest grade on a path that is otherwise
 * only as weak as its weakest existing name.  Comparing the answer to the
 * question the port already asks costs nothing new and is checked here.
 *
 * The comparison is by name and rectangle together: a display that keeps its
 * name and changes its resolution is a change the ribbon has to hear about,
 * and so is one that keeps its resolution and is now a different monitor.
 */
static void
run_displays(void)
{
	struct wsip_display	 now[WSI_MAXDISPLAY];
	double			 t = wsip_now();
	int			 n, i, same;

	if (t - checked < WSI_DISPLAY_MS)
		return;
	checked = t;

	n = wsip_displays(now, WSI_MAXDISPLAY);
	if (n < 0)
		n = 0;

	same = (n == nseen);
	for (i = 0; same && i < n; i++) {
		if (strncmp(now[i].name, seen[i].name, WSIP_NAMELEN) != 0 ||
		    memcmp(&now[i].view, &seen[i].view, sizeof(now[i].view)) != 0 ||
		    memcmp(&now[i].work, &seen[i].work, sizeof(now[i].work)) != 0)
			same = 0;
	}
	if (same)
		return;

	(void)memcpy(seen, now, sizeof(seen));
	nseen = n;
	wsi_note_displays();
}

/*
 * THE LIST THIS PORT GOES DOWN ON ITS FIRST MAC.
 *
 * doc/macos.md grades every Apple name this port uses by how well the tree
 * knows it, and says of the lowest grade that it is "what to go down first on
 * the first Mac".  This is that walk, made by the program itself, in the order
 * the names are reached at run time, so that a first run which does not work
 * ends in "kAXFrontmostAttribute was refused" rather than in silence.
 *
 * What it can and cannot catch is worth being exact about.  A misspelt symbol
 * or a wrong argument list never gets this far - it fails when the file is
 * compiled against Apple's own headers, which is the cheapest place to fail
 * and the reason macos/stub-build.sh exists.  What reaches here is the other
 * kind: a name that exists, compiles, links, and does not mean what we thought
 * it meant.  Those answer with an error code or with nothing at all, and this
 * is where that shows up with the name attached.
 */
static void
doctor_line(const char *grade, const char *names, int ok, const char *detail)
{
	(void)printf("  %-3s %-46s %-8s %s\n", grade, names,
	    ok ? "answered" : "FAILED", (detail != NULL) ? detail : "");
}

int
wsi_run_doctor(void)
{
	struct wsip_display	 disp[WSI_MAXDISPLAY];
	struct wsip_identity	 who;
	struct wsip_rect	 r;
	struct client_ctx	*cc;
	const struct wsiconf_bind	*b;
	char			 detail[256];
	int			 n, x, y, bad = 0, nb;

	(void)printf("digitwm: going down the Apple calls this tree cannot "
	    "check without a Mac.\nThis arranges your windows on the way, "
	    "because arranging them is what is being\ntested; the one window "
	    "it writes back is written back exactly where it already was.\n");
	(void)printf("The grade is doc/macos.md's: [1] already transcribed "
	    "here, [2] cited by\nheader, line or Apple's own documentation "
	    "page, [3] confirmed by nothing.\nA FAILED line names the call to "
	    "report; doc/macos-install.md says what each\none costs if it "
	    "stays broken.\n\n");

	/*
	 * THREE LINES BEFORE THE FIRST CALL, and they are here because of what
	 * happened on the first real Mac this port ever ran on, 2 September
	 * 2026: the owner granted Accessibility, and digitwm went on saying it
	 * had none.  Everything below needed the permission, so the report
	 * stopped at line one and told him what he had already read.
	 *
	 * These three do not need the permission and do not touch a window.
	 * They are the three reasons a granted permission does not arrive, and
	 * each of them is a fact about this process that only this process can
	 * see.
	 */
	if (wsip_identity(&who) == 0) {
		(void)printf("This process, before any of that:\n");
		(void)printf("  running   %s\n", who.path);
		if (strcmp(who.path, who.launched) != 0)
			(void)printf("  launched  %s\n"
			    "            NOT THE SAME FILE.  The permission is "
			    "remembered against the\n"
			    "            first path, not the second.  brew puts "
			    "the link on PATH and the\n"
			    "            real file in the Cellar; add the first "
			    "line to Accessibility.\n",
			    who.launched);
		if (who.parent[0] != '\0')
			(void)printf("  started by %s\n"
			    "            If that is a terminal, the permission "
			    "macOS looks at is ITS\n"
			    "            permission, not digitwm's - a program "
			    "started from a shell does\n"
			    "            not hold its own.  Grant it to the "
			    "terminal, quit the terminal\n"
			    "            completely (Command-Q, not the tab) "
			    "and start it again.\n",
			    who.parent);
		if (who.cdhash[0] != '\0')
			(void)printf("  signature %s\n            cdhash %s\n"
			    "            This number is what the permission is "
			    "keyed to.  Ad-hoc means it\n"
			    "            changes with the file, so a rebuild "
			    "asks again.\n",
			    who.signing, who.cdhash);
		(void)printf("\n");
	} else
		(void)printf("This process cannot say who it is: not a Mac.\n\n");

	if (wsi_run_init() != 0) {
		doctor_line("[1]", "AXIsProcessTrustedWithOptions", 0,
		    "Accessibility is not granted to this binary");
		(void)printf("\nNothing else can be tried until that is "
		    "granted: every call below needs it.  The\n"
		    "three lines above are where a granted permission usually "
		    "goes missing; go\n"
		    "down them before going to System Settings > Privacy & "
		    "Security > Accessibility.\n");
		return 1;
	}
	doctor_line("[1]", "AXIsProcessTrustedWithOptions", 1,
	    "Accessibility granted");

	n = wsip_displays(disp, WSI_MAXDISPLAY);
	if (n <= 0) {
		bad++;
		doctor_line("[2]", "+[NSScreen screens], -frame, -visibleFrame",
		    0, "no display answered");
	} else {
		(void)snprintf(detail, sizeof(detail),
		    "%d display(s), first \"%s\" %dx%d, workable %dx%d",
		    n, disp[0].name, disp[0].view.w, disp[0].view.h,
		    disp[0].work.w, disp[0].work.h);
		doctor_line("[2]", "+[NSScreen screens], -frame, -visibleFrame",
		    1, detail);
		if (disp[0].name[0] == '\0') {
			bad++;
			doctor_line("[2]", "-[NSScreen localizedName]", 0,
			    "a display with no name: the ribbon binds to an "
			    "output by name");
		} else
			doctor_line("[2]", "-[NSScreen localizedName]", 1,
			    disp[0].name);
	}

	cc = TAILQ_FIRST(&sc.clientq);
	if (cc == NULL) {
		bad++;
		doctor_line("[2]", "NSWorkspace, NSRunningApplication", 0,
		    "not one window found: either no application answered, "
		    "or the enumeration is wrong");
	} else {
		(void)snprintf(detail, sizeof(detail), "%lu window(s) taken "
		    "onto the ribbon", wsi_core_stats()->opened);
		doctor_line("[2]", "NSWorkspace, NSRunningApplication", 1,
		    detail);
	}

	if (wsip_pointer(&x, &y) != 0) {
		bad++;
		doctor_line("[2]", "+[NSEvent mouseLocation]", 0,
		    "the pointer has no position");
	} else {
		(void)snprintf(detail, sizeof(detail), "pointer at %d,%d", x, y);
		doctor_line("[2]", "+[NSEvent mouseLocation]", 1, detail);
	}

	if (cc != NULL) {
		if (wsip_frame_get(cc->win, &r) != 0) {
			bad++;
			doctor_line("[1]", "AXUIElementCopyAttributeValue "
			    "(position, size)", 0, "a window would not say "
			    "where it is");
		} else {
			(void)snprintf(detail, sizeof(detail),
			    "first window at %d,%d %dx%d", r.x, r.y, r.w, r.h);
			doctor_line("[1]", "AXUIElementCopyAttributeValue "
			    "(position, size)", 1, detail);

			/*
			 * The window is written back exactly where it already
			 * is.  A check that moved somebody's window to prove
			 * it can move windows would be a check nobody runs
			 * twice.
			 */
			if (wsip_frame_set(cc->win, &r) != 0) {
				bad++;
				doctor_line("[2]", "AXUIElementIsAttributeSettable,"
				    " SetAttributeValue", 0, "the write was "
				    "refused - the ribbon cannot place windows");
			} else
				doctor_line("[2]", "AXUIElementIsAttributeSettable,"
				    " SetAttributeValue", 1,
				    "geometry accepted");
		}

		if (wsip_raise(cc->win) != 0) {
			bad++;
			doctor_line("[2]", "AXUIElementPerformAction, "
			    "kAXRaiseAction", 0, "raising was refused");
		} else
			doctor_line("[2]", "AXUIElementPerformAction, "
			    "kAXRaiseAction", 1, "raise accepted");

		if (wsip_activate(cc->win) != 0) {
			bad++;
			doctor_line("[2]", "kAXMainAttribute, "
			    "kAXFrontmostAttribute", 0, "activation was "
			    "refused - this is the call doc/macos.md says to "
			    "replace with _SLPSSetFrontProcessWithOptions");
		} else
			doctor_line("[2]", "kAXMainAttribute, "
			    "kAXFrontmostAttribute", 1, "activation accepted");
	}

	if (wsip_pointer_warp(0, 0) == 0)
		doctor_line("[-]", "wsip_pointer_warp", 1,
		    "answered, and this port expects it not to");
	else
		doctor_line("[-]", "wsip_pointer_warp", 1,
		    "refuses, as designed: no call for it exists");

	b = wsiconf_binds(&nb);
	if (nb > 0) {
		if (wsik_open() != 0) {
			bad++;
			doctor_line("[2]", "InstallEventHandler, "
			    "GetApplicationEventTarget", 0,
			    "no connection to the window server");
		} else {
			doctor_line("[2]", "InstallEventHandler, "
			    "GetApplicationEventTarget", 1, "handler installed");
			if (wsik_bind(0, b[0].mods, b[0].key) != 0) {
				bad++;
				doctor_line("[2]", "RegisterEventHotKey", 0,
				    "the first binding was refused");
			} else {
				char	 bind[64];

				(void)snprintf(detail, sizeof(detail),
				    "%s taken", wsiconf_bindstr(&b[0], bind,
				    sizeof(bind)));
				doctor_line("[2]", "RegisterEventHotKey", 1,
				    detail);
			}
			wsik_close();
		}
	}

	(void)printf("\n%d call(s) did not answer.\n", bad);
	if (bad > 0)
		(void)printf("Report the FAILED line as it stands: the name in "
		    "it is the thing to fix, and\ndoc/macos-install.md says "
		    "what each of them costs if it stays broken.\n");
	return bad;
}
