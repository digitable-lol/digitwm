/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - macos/wsi_platform.h over the Accessibility API
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
 * THIS FILE HAS NEVER BEEN COMPILED AGAINST APPLE'S HEADERS.
 *
 * Nobody on this project has a Mac (doc/portability.md:13), and this machine
 * has no macOS SDK: there is no ApplicationServices, no AppKit, no framework
 * of Apple's anywhere on it.  So "it builds" is not a claim this file is
 * allowed to make, and it does not make it.
 *
 * What is checked here, and what "sh macos/stub-build.sh" does, is the same
 * thing tools/macos-flicker/stub-build.sh does for axcost.c and for the same
 * reason: the file is compiled - syntax, types, argument lists, the shape of
 * every callback - against a stub written out of Apple's headers by hand.
 * A misspelled name, a swapped argument, a callback with the wrong
 * signature, a CFTypeRef where an AXUIElementRef belongs: all of that fails
 * here, on Linux, today.  What CANNOT fail here is the stub itself being
 * wrong, and the stub is our belief about somebody else's API.
 *
 * So every Apple symbol below carries a source, and there are three grades
 * of source.  The grade is written at the call site whenever it is not the
 * first one.
 *
 *   [1] TRANSCRIBED IN THIS TREE ALREADY.  The stub in
 *       tools/macos-flicker/stub-build.sh was written out of
 *       CoreFoundation.h, AXUIElement.h, AXValue.h, AXAttributeConstants.h
 *       and AXNotificationConstants.h, and axcost.c has been compiling
 *       against it since it was written.  macos/stub-build.sh reuses it
 *       verbatim rather than making a second copy, so the two files cannot
 *       drift into disagreeing about the same API.
 *
 *   [2] CITED IN doc/portability.md, name and place, signature not.  The
 *       table there gives a header and a line ("AXUIElement.h:204") or an
 *       open-source manager and a line ("yabai, src/window_manager.c:1324-
 *       1335").  That confirms the call exists and does this job.  It does
 *       not confirm the argument list, and where this file guesses one, it
 *       says so at the call.
 *
 *   [3] NOT CONFIRMED BY ANYTHING IN THIS TREE.  Written from general
 *       knowledge of the API, which is exactly the thing that is allowed to
 *       be wrong.  Marked at the call, every time, and listed again in
 *       doc/macos.md so the owner can go down the list on the first Mac.
 *
 * The whole inventory, so that nobody has to grep for it:
 *
 *   [1] - 32 names, and the signature of AXObserverCallback with them:
 *       CFRelease CFRetain CFGetTypeID CFArrayGetTypeID CFArrayGetCount
 *       CFArrayGetValueAtIndex CFDictionaryCreate
 *       kCFTypeDictionaryKeyCallBacks kCFTypeDictionaryValueCallBacks
 *       kCFBooleanTrue CFRunLoopGetCurrent CFRunLoopAddSource
 *       CFRunLoopRunInMode kCFRunLoopDefaultMode
 *       AXUIElementCreateApplication AXUIElementCopyAttributeValue
 *       AXUIElementSetAttributeValue AXUIElementSetMessagingTimeout
 *       AXValueCreate AXValueGetValue AXObserverCreate
 *       AXObserverAddNotification AXObserverGetRunLoopSource
 *       AXIsProcessTrustedWithOptions kAXTrustedCheckOptionPrompt
 *       kAXPositionAttribute kAXSizeAttribute kAXWindowsAttribute
 *       kAXWindowCreatedNotification kAXValueTypeCGPoint kAXValueTypeCGSize
 *       kAXErrorSuccess
 *
 *   [2] - 9 names:
 *       AXUIElementIsAttributeSettable (AXUIElement.h:204)
 *       kAXUIElementDestroyedNotification kAXWindowMovedNotification
 *       kAXWindowResizedNotification kAXFocusedWindowChangedNotification
 *       (AXNotificationConstants.h:113,123,133,194)
 *       AXUIElementPerformAction, kAXRaiseAction
 *       (yabai src/window_manager.c:1324-1335)
 *       +[NSEvent mouseLocation] (doc/portability.md:95, doc/macos.md:76)
 *       -[NSScreen visibleFrame] (doc/portability.md, "Apple documentation")
 *
 *   [3] - 4 names.  CFRunLoopRemoveSource (the exact mirror of the [1]
 *       CFRunLoopAddSource), -[NSString UTF8String], -[NSArray count],
 *       -[NSArray objectAtIndex:].
 *
 * ELEVEN NAMES LEFT THAT LIST, and it is worth saying how, because the way is
 * repeatable and the rest of the list can leave the same way.  They were
 * looked up in Apple's own documentation - not in a neighbour's source, not
 * from memory - and each of the pages below states the declaration this file
 * uses.  That is a stronger citation than [2] asks for (header and line), so
 * they are [2] here with the page named instead of the header:
 *
 *   AXUIElementIsAttributeSettable, AXUIElementPerformAction, kAXRaiseAction
 *     applicationservices/1459972-axuielementisattributesettable
 *     applicationservices/1462091-axuielementperformaction
 *     applicationservices/kaxraiseaction
 *   AXObserverRemoveNotification
 *     applicationservices/1462066-axobserverremovenotification
 *   CFEqual                     corefoundation/cfequal(_:_:)
 *   kAXMainAttribute            applicationservices/kaxmainattribute
 *   kAXFrontmostAttribute       applicationservices/kaxfrontmostattribute
 *   +[NSScreen screens], -frame, -visibleFrame, -localizedName
 *     appkit/nsscreen (screens, frame, visibleframe, localizedname)
 *   +[NSWorkspace sharedWorkspace], -runningApplications
 *     appkit/nsworkspace/shared, appkit/nsworkspace/runningapplications
 *   -[NSRunningApplication processIdentifier], -activationPolicy,
 *   NSApplicationActivationPolicyRegular
 *     appkit/nsrunningapplication/processidentifier, /activationpolicy
 *
 * WHAT THE PAGES DO NOT SAY, and therefore what is still a guess:
 *
 *   - the VALUE TYPE of kAXMainAttribute and kAXFrontmostAttribute.  Apple
 *     says what they mean and not what to write into them; this file writes
 *     kCFBooleanTrue, on the strength of AppKit's parallel constants being
 *     documented as NSNumber.  Different symbols, so it stays a guess;
 *   - the string value behind kAXRaiseAction and the notification constants.
 *     Nothing here needs one: they are compared and passed as symbols;
 *   - -[NSRunningApplication processIdentifier] is documented to answer -1
 *     for an application that has no pid.  This file does not check for it -
 *     AXUIElementCreateApplication(-1) simply finds nothing - and that is a
 *     small hole rather than a wrong answer.
 *
 * AXUIElementGetPid turns out to be documented too
 * (applicationservices/1460337-axuielementgetpid), which removes the reason
 * this file avoided it.  It is still not called: the observer's refcon
 * answers the same question for nothing, and a call that is not made cannot
 * be wrong.
 *
 * One call this file deliberately does NOT make.  doc/portability.md's route
 * to activation is "_SLPSSetFrontProcessWithOptions plus
 * AXUIElementPerformAction(kAXRaiseAction)".  The first of those is a
 * private SkyLight entry point: no header declares it anywhere, public or
 * in this tree, and a signature written for it would be invention rather
 * than a guess.  So activation here goes through kAXFrontmostAttribute,
 * which is public and grade [2] - Apple documents the constant, though not
 * what to write into it - and if the first Mac says that is not
 * enough, the private call goes in with yabai's declaration copied
 * literally and cited.  Naming the substitution is the point; doing it
 * silently would be the thing not to do.
 *
 * Build on a Mac:   cc -Wall -Wextra -fobjc-arc -c macos/wsi_ax.m \
 *                       -framework ApplicationServices -framework AppKit
 * Check here:       sh macos/stub-build.sh
 */

#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "wsi_platform.h"

#define AX_MAXWIN	512
#define AX_MAXAPP	128
#define AX_NAMELEN	64

/*
 * How long to wait for one application to answer before giving up on it.
 *
 * Not politeness: a window manager that blocks on a hung application stops
 * being a window manager.  The number is a guess and is marked as one - the
 * command that replaces it with a measurement is "sh
 * tools/macos-flicker/run.sh", whose "выдача" column is the cost of exactly
 * one of these calls.
 */
#define AX_TIMEOUT	0.2f

/*
 * How often to look for applications that have started or quit since the last
 * look.
 *
 * A window manager that only ever attached to the applications running when it
 * started would manage nothing a person opens afterwards, and would keep a
 * column for every application he has since quit.
 *
 * THE BETTER INSTRUMENT EXISTS AND IS DOCUMENTED, and this is not it.  Apple
 * publishes NSWorkspaceDidLaunchApplicationNotification and
 * NSWorkspaceDidTerminateApplicationNotification, with the warning that they
 * arrive only through -[NSWorkspace notificationCenter] and not through the
 * default one (appkit/nsworkspace/didlaunchapplicationnotification).  They
 * would answer immediately where this answers within a second.
 *
 * The poll is what is written because it asks a question this file already
 * asks, needs no observer object, no block and no second notification centre,
 * and answers both halves - who is new and who is gone - with one walk.  The
 * price is written down rather than hidden: a window of an application that
 * was not running a second ago joins the ribbon up to this many milliseconds
 * late.  Windows of an application already attached are not affected; those
 * arrive by notification, at the delay doc/macos.md measures.
 *
 * If that second is felt on the first Mac, the two notification names above
 * are the replacement, and they are cited rather than guessed.
 */
#define AX_SCAN_MS	1000.0

struct ax_win {
	wsip_window		 id;
	AXUIElementRef		 el;
	pid_t			 pid;
	int			 inuse;
	int			 watched;
};

struct ax_app {
	pid_t			 pid;
	AXUIElementRef		 el;
	AXObserverRef		 obs;
	int			 inuse;
};

static int		 ax_track(AXUIElementRef);
static int		 ax_attach(pid_t);
static void		 ax_forget(struct ax_app *);
static void		 ax_scan(void);

static struct ax_win	 ax_win[AX_MAXWIN];
static struct ax_app	 ax_app[AX_MAXAPP];
static wsip_window	 ax_nextid = 1;
static CGFloat		 ax_screen_top;	/* AppKit y of the top of the desk */

/*
 * Coordinates.
 *
 * AX puts the origin at the top left of the primary display and counts y
 * downwards; AppKit puts it at the bottom left and counts upwards.  X11 does
 * what AX does, so everything above macos/wsi_platform.h - the ribbon
 * included - stays in the coordinates it was written for, and the flip lives
 * here, in the four functions that touch AppKit.  That is worth one variable
 * and no arithmetic anywhere else.
 *
 * ax_screen_top is the AppKit y of the top edge of the primary display,
 * which is the line both systems measure from.
 */
static int
ax_flip(CGFloat appkit_y, CGFloat height)
{
	return (int)(ax_screen_top - (appkit_y + height));
}

static struct ax_win *
ax_find(wsip_window id)
{
	int	 i;

	for (i = 0; i < AX_MAXWIN; i++) {
		if (ax_win[i].inuse && ax_win[i].id == id)
			return &ax_win[i];
	}
	return NULL;
}

static struct ax_win *
ax_find_el(AXUIElementRef el)
{
	int	 i;

	/*
	 * Compared as pointers, and that is not obviously right: AX hands out
	 * a fresh AXUIElementRef for the same window every time it is asked,
	 * and two refs to one window are equal by CFEqual and unequal by
	 * pointer.  It works here because every ref this table holds is one
	 * we retained ourselves and hand back to the observer as its
	 * refcon - so the pointer that comes back is the pointer that went
	 * out.  If that ever stops being true the fix is CFEqual, whose
	 * signature is now known (see this file's header) - it was not when
	 * this was written, and the comparison stays a pointer one because it
	 * is correct here, not because CFEqual is unavailable.
	 */
	for (i = 0; i < AX_MAXWIN; i++) {
		if (ax_win[i].inuse && ax_win[i].el == el)
			return &ax_win[i];
	}
	return NULL;
}

static struct ax_app *
ax_app_find(pid_t pid)
{
	int	 i;

	for (i = 0; i < AX_MAXAPP; i++) {
		if (ax_app[i].inuse && ax_app[i].pid == pid)
			return &ax_app[i];
	}
	return NULL;
}

/*
 * Read one rectangle out of a window.
 *
 * Two attributes, two round trips, because AX has no "frame": position and
 * size are separate and are separately settable.  [1] for all four calls -
 * axcost.c does exactly this and compiles against the same stub.
 */
static int
ax_rect(AXUIElementRef el, struct wsip_rect *r)
{
	CFTypeRef	 pos = NULL, size = NULL;
	CGPoint		 p;
	CGSize		 s;

	if (AXUIElementCopyAttributeValue(el, kAXPositionAttribute,
	    &pos) != kAXErrorSuccess)
		return -1;
	if (AXUIElementCopyAttributeValue(el, kAXSizeAttribute,
	    &size) != kAXErrorSuccess) {
		if (pos != NULL)
			CFRelease(pos);
		return -1;
	}
	if (!AXValueGetValue((AXValueRef)pos, kAXValueTypeCGPoint, &p) ||
	    !AXValueGetValue((AXValueRef)size, kAXValueTypeCGSize, &s)) {
		CFRelease(pos);
		CFRelease(size);
		return -1;
	}
	CFRelease(pos);
	CFRelease(size);

	/* Already AX coordinates: top-left origin, y downwards. */
	r->x = (int)p.x;
	r->y = (int)p.y;
	r->w = (int)s.width;
	r->h = (int)s.height;
	return 0;
}

/*
 * The observer's callback.
 *
 * Declared void, and that single fact is why the whole port is shaped the
 * way it is: there is no return channel, so nothing here can refuse a
 * change, substitute a geometry, or make the sender wait.  wsi.h spends a
 * paragraph on it under wsi_settle() and doc/portability.md cites the
 * declaration at AXUIElement.h:446.  All this function may do is report
 * upwards, which it does.
 *
 * The names are compared with CFEqual, which is grade [2]: Apple's own page
 * gives the declaration this file uses (corefoundation/cfequal), and the line
 * macos/stub-build.sh declares for it agrees with it.  The alternative was to
 * turn the name into a
 * C string with the grade [1] CFStringGetCString and compare it against
 * "AXWindowMoved" and its four siblings, which trades one unconfirmed
 * signature for five unconfirmed string VALUES: doc/portability.md cites the
 * constants by header and line, never their contents.  One guess beats five,
 * and this one is the API's own way of asking the question.
 */
static void
ax_note(AXObserverRef obs, AXUIElementRef el, CFStringRef name, void *refcon)
{
	struct ax_app		*a = refcon;
	struct ax_win		*w;
	struct wsip_rect	 r;

	(void)obs;

	/*
	 * The five notifications.  kAXWindowCreatedNotification is grade [1];
	 * the other four are [2] - doc/portability.md cites them by header
	 * and line (AXNotificationConstants.h:57,113,123,133,194), so
	 * macos/stub-build.sh declares them alongside the [1] ones and this
	 * comment is the marker.
	 */
	if (CFEqual(name, kAXWindowCreatedNotification)) {
		if (ax_track(el) != 0 || (w = ax_find_el(el)) == NULL)
			return;
		/*
		 * Which application this window belongs to, and the only place
		 * the answer is available for free.  AXUIElementGetPid answers
		 * it too and is documented (see this file's header), so the
		 * choice is not about sources any more: the refcon is grade
		 * [1] - AXObserverAddNotification takes it and
		 * AXObserverCallback hands it back - and it costs no call at
		 * all, where AXUIElementGetPid is a round trip into another
		 * process on a path that already makes two.
		 *
		 * It is not decoration.  wsip_watch() and wsip_activate() both
		 * start by finding the application, so a window whose pid
		 * stayed zero would never be watched for moving, resizing or
		 * closing, and could never be given the keyboard.  Every
		 * window opened after digitwm started is one of those.
		 */
		if (a != NULL)
			w->pid = a->pid;
		if (ax_rect(el, &r) == 0)
			wsi_note_open(w->id, &r);
		return;
	}
	if ((w = ax_find_el(el)) == NULL)
		return;

	if (CFEqual(name, kAXUIElementDestroyedNotification)) {
		wsi_note_close(w->id);
		w->inuse = 0;
		CFRelease(w->el);
		return;
	}
	if (CFEqual(name, kAXWindowMovedNotification) ||
	    CFEqual(name, kAXWindowResizedNotification)) {
		if (ax_rect(el, &r) == 0)
			wsi_note_frame(w->id, &r);
		return;
	}
	if (CFEqual(name, kAXFocusedWindowChangedNotification))
		wsi_note_focus(w->id);
}

/* Give a window an identifier of ours, if it has not got one. */
static int
ax_track(AXUIElementRef el)
{
	int	 i;

	if (ax_find_el(el) != NULL)
		return 0;

	for (i = 0; i < AX_MAXWIN; i++) {
		if (ax_win[i].inuse)
			continue;
		(void)memset(&ax_win[i], 0, sizeof(ax_win[i]));
		ax_win[i].inuse = 1;
		ax_win[i].id = ax_nextid++;
		ax_win[i].el = (AXUIElementRef)CFRetain(el);
		return 0;
	}
	return -1;
}

/*
 * Attach to one application: an element for talking to it, an observer for
 * being talked to, and the five notifications.
 *
 * AXUIElementSetMessagingTimeout is [1] and is the difference between a
 * window manager and a hung one.
 */
static int
ax_attach(pid_t pid)
{
	AXUIElementRef	 app;
	AXObserverRef	 obs = NULL;
	CFTypeRef	 wins = NULL;
	int		 i, slot = -1;

	if (ax_app_find(pid) != NULL)
		return 0;

	for (i = 0; i < AX_MAXAPP; i++) {
		if (!ax_app[i].inuse) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return -1;

	if ((app = AXUIElementCreateApplication(pid)) == NULL)
		return -1;
	AXUIElementSetMessagingTimeout(app, AX_TIMEOUT);

	if (AXObserverCreate(pid, ax_note, &obs) != kAXErrorSuccess) {
		CFRelease(app);
		return -1;
	}

	/*
	 * The record is filled before the first notification is asked for,
	 * because it is what goes into the refcon: ax_note() needs to know
	 * which application a window it has never seen belongs to, and the
	 * refcon is the only channel AXObserverCallback has for saying so.
	 */
	ax_app[slot].inuse = 1;
	ax_app[slot].pid = pid;
	ax_app[slot].el = app;
	ax_app[slot].obs = obs;

	AXObserverAddNotification(obs, app, kAXWindowCreatedNotification,
	    &ax_app[slot]);
	AXObserverAddNotification(obs, app, kAXFocusedWindowChangedNotification,
	    &ax_app[slot]);

	CFRunLoopAddSource(CFRunLoopGetCurrent(),
	    AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);

	/*
	 * The windows the application already had.  A manager that only ever
	 * heard kAXWindowCreated would manage nothing that existed before it
	 * started, which is every window on a desk it was just launched onto.
	 */
	if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute,
	    &wins) == kAXErrorSuccess && wins != NULL) {
		if (CFGetTypeID(wins) == CFArrayGetTypeID()) {
			CFIndex	 n = CFArrayGetCount((CFArrayRef)wins), k;
			struct wsip_rect	 r;
			struct ax_win		*w;

			for (k = 0; k < n; k++) {
				AXUIElementRef	 el = (AXUIElementRef)
				    CFArrayGetValueAtIndex((CFArrayRef)wins, k);

				if (ax_track(el) != 0)
					break;
				if ((w = ax_find_el(el)) == NULL)
					continue;
				w->pid = pid;
				if (ax_rect(el, &r) == 0)
					wsi_note_open(w->id, &r);
			}
		}
		CFRelease(wins);
	}
	return 0;
}

/*
 * Let one application go: its windows leave the ribbon, its observer leaves
 * the run loop, and its slot is free again.
 *
 * The windows are reported closed rather than quietly dropped, because that is
 * the only thing that takes them off the ribbon: nothing else in this port
 * removes a column, and an application that quit while its windows still stood
 * in one would leave a hole the user cannot scroll past.
 *
 * CFRunLoopRemoveSource is grade [3] and is the exact mirror of the grade [1]
 * CFRunLoopAddSource three functions up - same three arguments, opposite verb.
 */
static void
ax_forget(struct ax_app *a)
{
	int	 i;

	for (i = 0; i < AX_MAXWIN; i++) {
		if (!ax_win[i].inuse || ax_win[i].pid != a->pid)
			continue;
		wsi_note_close(ax_win[i].id);
		if (ax_win[i].inuse) {
			CFRelease(ax_win[i].el);
			ax_win[i].inuse = 0;
		}
	}

	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
	    AXObserverGetRunLoopSource(a->obs), kCFRunLoopDefaultMode);
	CFRelease(a->obs);
	CFRelease(a->el);
	a->inuse = 0;
}

/*
 * Who is running now: attach to what is new, let go of what is gone.
 *
 * Grade [2] from the first line of the pool to the last: every selector here
 * is on an Apple page named in this file's header.  It was grade [3] when it
 * was written, and it is the function the whole port stands on - a manager
 * that finds no applications finds no windows - which is why it was the first
 * thing looked up.
 *
 * Applications with no dock icon are skipped: an agent has no windows a person
 * arranges, and attaching an observer to each of them costs a port into that
 * process for nothing.
 */
static void
ax_scan(void)
{
	pid_t	 pid, live[AX_MAXAPP];
	int	 nlive = 0, truncated = 0, i, j, found;

	@autoreleasepool {
		NSArray		*apps;
		NSUInteger	 k;

		apps = [[NSWorkspace sharedWorkspace] runningApplications];
		for (k = 0; k < [apps count]; k++) {
			NSRunningApplication	*a = [apps objectAtIndex:k];

			if ([a activationPolicy] !=
			    NSApplicationActivationPolicyRegular)
				continue;
			if (nlive >= AX_MAXAPP) {
				truncated = 1;
				break;
			}
			pid = [a processIdentifier];
			live[nlive++] = pid;
			(void)ax_attach(pid);
		}
	}

	/*
	 * More applications than there are slots is not a reason to declare the
	 * ones we could not see dead: the list is incomplete, and reaping from
	 * an incomplete list would close windows that are still open.
	 */
	if (truncated)
		return;

	for (i = 0; i < AX_MAXAPP; i++) {
		if (!ax_app[i].inuse)
			continue;
		for (j = 0, found = 0; j < nlive; j++) {
			if (ax_app[i].pid == live[j]) {
				found = 1;
				break;
			}
		}
		if (!found)
			ax_forget(&ax_app[i]);
	}
}

/*
 * macos/wsi_platform.h, on the macOS side of the line.
 */

int
wsip_open(void)
{
	const void	*keys[1];
	const void	*vals[1];
	CFDictionaryRef	 opts;
	Boolean		 trusted;

	/*
	 * The one permission this port needs, and the prompt that asks for
	 * it.  [1] for both the call and the option key.  Nothing here
	 * requires SIP to be off - doc/portability.md says which things
	 * would, and the ribbon needs none of them.
	 */
	keys[0] = kAXTrustedCheckOptionPrompt;
	vals[0] = kCFBooleanTrue;
	opts = CFDictionaryCreate(NULL, keys, vals, 1,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	trusted = AXIsProcessTrustedWithOptions(opts);
	if (opts != NULL)
		CFRelease(opts);
	if (!trusted)
		return -1;

	/*
	 * DISPLAYS FIRST, APPLICATIONS SECOND, and the order is load-bearing
	 * twice over rather than tidy.
	 *
	 * wsi_note_displays() is what fills the region list the ribbon binds
	 * itself to by name, and what fills ax_screen_top, the one number every
	 * coordinate in this file is flipped around.  Attaching to the
	 * applications reports every window they already have - each of those
	 * goes straight into wsi_note_open(), which asks the ribbon which
	 * column it belongs in.  Done in the other order, that question is
	 * asked of a screen with no outputs, and answered by a flip around
	 * zero: every window on the desk lands in nothing, at a negative y.
	 */
	wsi_note_displays();
	ax_scan();
	return 0;
}

/*
 * A monotonic clock shared with everything else that measures on this
 * machine - the same one tools/macos-flicker/axcost.c uses, and for the same
 * reason: numbers from the two must add up without a correction.
 */
double
wsip_now(void)
{
	struct timespec	 ts;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0);
}

int
wsip_frame_get(wsip_window id, struct wsip_rect *r)
{
	struct ax_win	*w = ax_find(id);

	return (w == NULL) ? -1 : ax_rect(w->el, r);
}

/*
 * Put the window here.
 *
 * Position first, then size, and the order is not arbitrary: a window moved
 * before it is resized never occupies a rectangle wider than either the old
 * or the new one, so a column does not flash over its neighbour on the way.
 * This is the one call the whole port is measured by - it is a synchronous
 * round trip into another process, and doc/portability.md's estimate of the
 * flicker is a count of these times their cost.
 *
 * AXUIElementIsAttributeSettable is grade [2]: doc/portability.md:139 gives
 * the name and AXUIElement.h:204, and nothing in this tree gives the
 * argument list.  The three arguments below are this file's guess and are
 * marked as such; if they are wrong, the first Mac says so at compile time,
 * which is the cheapest place to be told.
 */
int
wsip_frame_set(wsip_window id, const struct wsip_rect *r)
{
	struct ax_win	*w = ax_find(id);
	AXValueRef	 pos, size;
	CGPoint		 p;
	CGSize		 s;
	Boolean		 settable = 0;
	int		 rc = 0;

	if (w == NULL)
		return -1;

	if (AXUIElementIsAttributeSettable(w->el, kAXPositionAttribute,
	    &settable) != kAXErrorSuccess || !settable)
		return -1;

	p.x = (CGFloat)r->x;
	p.y = (CGFloat)r->y;
	s.width = (CGFloat)r->w;
	s.height = (CGFloat)r->h;

	if ((pos = AXValueCreate(kAXValueTypeCGPoint, &p)) != NULL) {
		if (AXUIElementSetAttributeValue(w->el, kAXPositionAttribute,
		    pos) != kAXErrorSuccess)
			rc = -1;
		CFRelease(pos);
	}
	if ((size = AXValueCreate(kAXValueTypeCGSize, &s)) != NULL) {
		if (AXUIElementSetAttributeValue(w->el, kAXSizeAttribute,
		    size) != kAXErrorSuccess)
			rc = -1;
		CFRelease(size);
	}
	return rc;
}

/*
 * Above its siblings.  [2]: doc/portability.md cites
 * AXUIElementPerformAction with kAXRaiseAction through yabai
 * src/window_manager.c:1324-1335; the two-argument signature below is this
 * file's guess.
 */
int
wsip_raise(wsip_window id)
{
	struct ax_win	*w = ax_find(id);

	if (w == NULL)
		return -1;
	return (AXUIElementPerformAction(w->el, kAXRaiseAction) ==
	    kAXErrorSuccess) ? 0 : -1;
}

/*
 * The keyboard goes here.
 *
 * Three steps, because on macOS focus is a pair - a front process, and a
 * window of it - and there is no single call that sets both.  kAXMainAttribute
 * and kAXFrontmostAttribute are grade [2] by name and meaning and a guess by
 * value type, which is the split this file's header spells out; the
 * substitution for
 * _SLPSSetFrontProcessWithOptions is argued in this file's header rather
 * than made quietly.
 */
int
wsip_activate(wsip_window id)
{
	struct ax_win	*w = ax_find(id);
	struct ax_app	*a;

	if (w == NULL || (a = ax_app_find(w->pid)) == NULL)
		return -1;

	(void)AXUIElementPerformAction(w->el, kAXRaiseAction);
	(void)AXUIElementSetAttributeValue(w->el, kAXMainAttribute,
	    kCFBooleanTrue);
	return (AXUIElementSetAttributeValue(a->el, kAXFrontmostAttribute,
	    kCFBooleanTrue) == kAXErrorSuccess) ? 0 : -1;
}

/*
 * Watch this window, or stop.
 *
 * The move and resize notifications are per window rather than per
 * application, which is why they are added here and not in ax_attach():
 * subscribing to every window of every application would deliver the moves
 * of windows the ribbon does not manage, and each one of those costs the
 * echo check a walk it need not make.
 *
 * AXObserverRemoveNotification is grade [2] - Apple's page gives the
 * declaration; it was [3] until this cell looked it up.
 */
int
wsip_watch(wsip_window id, int on)
{
	struct ax_win	*w = ax_find(id);
	struct ax_app	*a;

	if (w == NULL || (a = ax_app_find(w->pid)) == NULL)
		return -1;

	if (on) {
		AXObserverAddNotification(a->obs, w->el,
		    kAXWindowMovedNotification, a);
		AXObserverAddNotification(a->obs, w->el,
		    kAXWindowResizedNotification, a);
		AXObserverAddNotification(a->obs, w->el,
		    kAXUIElementDestroyedNotification, a);
	} else {
		AXObserverRemoveNotification(a->obs, w->el,
		    kAXWindowMovedNotification);
		AXObserverRemoveNotification(a->obs, w->el,
		    kAXWindowResizedNotification);
		AXObserverRemoveNotification(a->obs, w->el,
		    kAXUIElementDestroyedNotification);
	}
	w->watched = on;
	return 0;
}

/*
 * Where the pointer is.  [2] for the call (doc/portability.md:95), and the
 * flip is ours.
 */
int
wsip_pointer(int *x, int *y)
{
	@autoreleasepool {
		NSPoint	 p = [NSEvent mouseLocation];

		*x = (int)p.x;
		*y = ax_flip(p.y, 0.0);
	}
	return 0;
}

/*
 * And this one cannot be done.
 *
 * Not "is hard": doc/portability.md's table has a macOS counterpart on every
 * row but this one, and doc/macos.md:76 says outright that the plan refuses
 * to invent a call for it.  wsi.h allows exactly this - a port that cannot
 * move the pointer keeps the contract by doing nothing here, provided focus
 * does not follow the pointer on that platform - and this port takes the
 * pair: it installs no CGEventTap, so nothing follows the pointer, so
 * nothing needs to be dragged after the focus.
 *
 * If a call is ever found, it goes here and the CGEventTap goes in with it.
 * They are one decision.
 */
int
wsip_pointer_warp(int x, int y)
{
	(void)x;
	(void)y;
	return -1;
}

/*
 * Let the platform's own machinery run.
 *
 * CFRunLoopRunInMode with returnAfterSourceHandled false is [1]: it is what
 * tools/macos-flicker/axcost.c waits on, against the same stub.  This is
 * where every notification in this file is delivered from - the observer
 * sources were added to this run loop in ax_attach() - so a port that never
 * called it would hear nothing at all.
 */
int
wsip_pump(double ms)
{
	static double	 scanned;
	double		 now;

	(void)CFRunLoopRunInMode(kCFRunLoopDefaultMode,
	    (CFTimeInterval)(ms / 1000.0), 0);

	/*
	 * And the one thing no notification tells this port: that there is an
	 * application now which was not running a moment ago.  See AX_SCAN_MS
	 * for why the question is asked on a clock rather than subscribed to.
	 */
	now = wsip_now();
	if (now - scanned >= AX_SCAN_MS) {
		scanned = now;
		ax_scan();
	}
	return 0;
}

/*
 * The displays.
 *
 * visibleFrame is grade [2] and is the cheapest line of the whole port: it
 * is already without the menu bar and the Dock, so the 579 lines of EWMH
 * strut arithmetic in xutil.c have nothing to be ported into - they are
 * dropped, and this is what replaces them.
 *
 * screens, frame and localizedName are grade [2] as of this cell - Apple's own
 * pages give all three - and the problem with localizedName is not its grade.
 * It is a name for a person to read, and wsi.h wants a name that survives a
 * cable being pulled: two identical monitors give the same one, and the ribbon
 * binds itself by that name.  The documented way out is
 * -[NSScreen deviceDescription] under the key @"NSScreenNumber", which Apple
 * states holds the CGDirectDisplayID; ColorSync's
 * CGDisplayCreateUUIDFromDisplayID turns that into something stable across
 * reboots, and Apple publishes no word about what it is for.  Both are cited
 * rather than guessed, and this stands as the first thing to fix on the first
 * Mac with two identical monitors.
 *
 * Apple also warns that the array must not be cached because screens are
 * reconfigured at any time.  It is not cached: macos/wsi_run.c asks again
 * every second and rebuilds the region list when the answer changes.
 */
int
wsip_displays(struct wsip_display *out, int max)
{
	int	 n = 0;

	@autoreleasepool {
		NSArray		*screens = [NSScreen screens];
		NSUInteger	 i;

		if ([screens count] == 0)
			return 0;

		/*
		 * The primary display is the first, and its top edge is the
		 * line AX measures from.
		 */
		ax_screen_top = [[screens objectAtIndex:0] frame].origin.y +
		    [[screens objectAtIndex:0] frame].size.height;

		for (i = 0; i < [screens count] && n < max; i++) {
			NSScreen	*s = [screens objectAtIndex:i];
			NSRect		 f = [s frame];
			NSRect		 v = [s visibleFrame];

			(void)snprintf(out[n].name, sizeof(out[n].name), "%s",
			    [[s localizedName] UTF8String]);
			out[n].view.x = (int)f.origin.x;
			out[n].view.y = ax_flip(f.origin.y, f.size.height);
			out[n].view.w = (int)f.size.width;
			out[n].view.h = (int)f.size.height;
			out[n].work.x = (int)v.origin.x;
			out[n].work.y = ax_flip(v.origin.y, v.size.height);
			out[n].work.w = (int)v.size.width;
			out[n].work.h = (int)v.size.height;
			n++;
		}
	}
	return n;
}
