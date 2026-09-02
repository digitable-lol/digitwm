/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - macos/wsi_key.h over the Carbon Event Manager
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
 * THIS FILE HAS NEVER BEEN COMPILED AGAINST APPLE'S HEADERS, for the same
 * reason macos/wsi_ax.m has not: nobody on this project has a Mac
 * (doc/portability.md:13).  "sh macos/stub-build.sh" compiles it against a
 * stub written out of Apple's documentation by hand, with -Wall -Wextra
 * -Werror, and that catches a misspelt name, a swapped argument and a callback
 * with the wrong signature.  It cannot catch the stub being wrong.
 *
 * The grading is doc/macos.md's and is used here unchanged:
 *
 *   [1] transcribed in this tree already;
 *   [2] cited in this tree by header, line or documentation page;
 *   [3] confirmed by nothing here, written from the documentation of the API.
 *
 * NOT ONE APPLE NAME IN THIS FILE APPEARS ANYWHERE ELSE IN THIS TREE, because
 * until now nothing in this tree took a key.  Every one of them was therefore
 * looked up before it was written, in Apple's documentation and nowhere else,
 * and the result is uneven enough to be worth setting out in full.
 *
 * Apple has RETIRED the Carbon Event Manager reference: the documentation URL
 * is gone and the archived programming guide says nothing about hot keys.  The
 * one Apple page that still enumerates these symbols is an API-diff page for
 * macOS 10.10, and it prints them in their SWIFT projection.  So for the whole
 * Carbon half: the symbols exist, their PARAMETER TYPES and their ORDER are
 * Apple's, and their C parameter names, their numeric values and their header
 * name are not published anywhere Apple still hosts.
 *
 *   [2] Carbon, from that page, quoted as Apple prints it:
 *       RegisterEventHotKey(UInt32, UInt32, EventHotKeyID, EventTarget!,
 *           OptionBits, ...EventHotKey ref) -> OSStatus
 *       UnregisterEventHotKey(EventHotKey!) -> OSStatus
 *       InstallEventHandler(EventTarget!, EventHandlerUPP, ItemCount,
 *           UnsafePointer<EventTypeSpec>, void *, ...EventHandler ref)
 *           -> OSStatus
 *       GetEventParameter(Event!, EventParamName, EventParamType,
 *           EventParamType *, ByteCount, ByteCount *, void *) -> OSStatus
 *       GetApplicationEventTarget() -> EventTarget
 *       NewEventHandlerUPP(EventHandlerProcPtr) -> EventHandlerUPP
 *       EventHotKeyID.signature, EventHotKeyID.id, EventHotKeyRef,
 *       EventTypeSpec.eventClass, EventTypeSpec.eventKind,
 *       kEventClassKeyboard, kEventHotKeyPressed, kEventParamDirectObject,
 *       typeEventHotKeyID, cmdKey, shiftKey, optionKey, controlKey.
 *       The stub in macos/stub-build.sh is written to those types, so a
 *       swapped argument fails on Linux; the numeric values in it are
 *       arbitrary and are marked so, because Apple publishes none.
 *
 *   [2] Virtual key codes: the same page lists all 118 kVK_ constants,
 *       including every one used below, and an Apple engineer states in the
 *       developer forums that they live in <HIToolbox/Events.h>, are tied to
 *       Apple's ADB keyboards and "can't change".  Their values are not
 *       published and this file uses none - only the names.
 *
 *   [2] AppKit: +[NSApplication sharedApplication], -setActivationPolicy:,
 *       NSApplicationActivationPolicyAccessory, all three on Apple's own
 *       pages, the last documented as "doesn't appear in the Dock and doesn't
 *       have a menu bar".
 *
 * THE ONE THING NOBODY DOCUMENTS, and it is the thing most likely to fail:
 * whether a command-line program with no application bundle receives Carbon
 * hot key events at all.  Apple says nothing about it either way.  What Apple
 * does say, about the neighbouring case, is discouraging: an engineer answering
 * about AppKit's event handling writes that it "may not work because it may
 * rely on the AppKit machinery to be running", and that a bundle-less tool
 * wanting AppKit events needs app-like packaging and NSApplicationMain.  That
 * is AppKit and this is Carbon, and extrapolating from one to the other is
 * exactly the kind of guess this file refuses to make quietly - so it is named
 * here instead.  If wsik_open() answers and no key ever arrives, that is this
 * paragraph coming true, and the remedy is packaging: doc/macos-install.md
 * says what to do.
 *
 * TWO MORE THINGS APPLE SAYS THAT CHANGE HOW THIS IS USED.
 *
 *   - macOS Sequoia refuses a hot key registration whose modifiers are only
 *     Shift and Option; at least one other modifier is required.  The default
 *     table in macos/wsi_conf.c uses Control-Option throughout and satisfies
 *     it; a cwmrc that binds "M-h" does not, and macos/wsi_conf.c says so at
 *     the line rather than leaving the key to be refused in silence.
 *   - the same engineers do not recommend this call at all - "intimately tied
 *     to the legacy Carbon toolbox" - and offer nothing in its place that
 *     swallows a key without a second privacy grant.  It is used with that
 *     said out loud.
 *
 * WHY CARBON, WHICH IS OLD.  Because of what it does not need.  Apple's own
 * statement, from the WWDC session that introduced the grant: monitoring all
 * keyboard events including those of other applications requires user
 * approval, and "where a listen-only event requires authorization for input
 * monitoring, a modifying event app requires authorization for accessibility
 * features".  A CGEventTap for keys would therefore make digitwm hold TWO
 * grants - Accessibility for the windows, Input Monitoring for the keys -
 * where RegisterEventHotKey needs none of its own.  One dialogue instead of
 * two, for a program whose whole delivery problem is that it asks for a
 * dialogue at all.
 *
 * A misspelt symbol or a wrong argument list fails when this file first meets
 * Apple's headers: a compiler error with a line number, before anything runs,
 * which is the cheapest failure there is.  There are no numeric constants in
 * this file at all, so the quiet kind of mistake - a key code written as a
 * number and binding the wrong key - is not available to it.
 *
 * Build on a Mac:   cc -Wall -Wextra -fobjc-arc -c macos/wsi_key.m \
 *                       -framework Carbon -framework AppKit
 * Check here:       sh macos/stub-build.sh
 */

#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>

#include <string.h>

#include "wsi_key.h"

#define KEY_MAX		64

/*
 * The four characters that say these hot keys are ours, as a number rather
 * than as 'dgtw': a four-character literal is a warning under -Wall on one
 * compiler and an error under -Werror on the next, and the field is an OSType,
 * which is a number.  0x64677477 is "dgtw".
 */
#define KEY_SIGNATURE	0x64677477

static EventHotKeyRef	 key_ref[KEY_MAX];
static int		 key_used[KEY_MAX];
static int		 key_open;

/*
 * cwmrc's key names on the left, Apple's constants on the right.  The names on
 * the left are X11 keysym names, because that is what a cwmrc says and this
 * port reads the same file; the right-hand side is the thing a Mac compiler
 * checks.
 *
 * What is not here is as deliberate as what is: function keys, the keypad, and
 * every key whose position moves between keyboard layouts.  A virtual key code
 * is a POSITION on the keyboard, not a letter, so "h" here means "the key an
 * ANSI keyboard prints h on" - on a layout where that key prints something
 * else, the binding follows the position.  That is how every Mac hot key
 * behaves, and saying so is better than pretending otherwise.
 */
static const struct {
	const char	*name;
	UInt32		 code;
} key_codes[] = {
	{ "a", kVK_ANSI_A }, { "b", kVK_ANSI_B }, { "c", kVK_ANSI_C },
	{ "d", kVK_ANSI_D }, { "e", kVK_ANSI_E }, { "f", kVK_ANSI_F },
	{ "g", kVK_ANSI_G }, { "h", kVK_ANSI_H }, { "i", kVK_ANSI_I },
	{ "j", kVK_ANSI_J }, { "k", kVK_ANSI_K }, { "l", kVK_ANSI_L },
	{ "m", kVK_ANSI_M }, { "n", kVK_ANSI_N }, { "o", kVK_ANSI_O },
	{ "p", kVK_ANSI_P }, { "q", kVK_ANSI_Q }, { "r", kVK_ANSI_R },
	{ "s", kVK_ANSI_S }, { "t", kVK_ANSI_T }, { "u", kVK_ANSI_U },
	{ "v", kVK_ANSI_V }, { "w", kVK_ANSI_W }, { "x", kVK_ANSI_X },
	{ "y", kVK_ANSI_Y }, { "z", kVK_ANSI_Z },

	{ "0", kVK_ANSI_0 }, { "1", kVK_ANSI_1 }, { "2", kVK_ANSI_2 },
	{ "3", kVK_ANSI_3 }, { "4", kVK_ANSI_4 }, { "5", kVK_ANSI_5 },
	{ "6", kVK_ANSI_6 }, { "7", kVK_ANSI_7 }, { "8", kVK_ANSI_8 },
	{ "9", kVK_ANSI_9 },

	{ "Return", kVK_Return },
	{ "space", kVK_Space },
	{ "Tab", kVK_Tab },
	{ "Escape", kVK_Escape },
	{ "Left", kVK_LeftArrow },
	{ "Right", kVK_RightArrow },
	{ "Up", kVK_UpArrow },
	{ "Down", kVK_DownArrow },
	{ "minus", kVK_ANSI_Minus },
	{ "equal", kVK_ANSI_Equal },
	{ "comma", kVK_ANSI_Comma },
	{ "period", kVK_ANSI_Period },
	{ "slash", kVK_ANSI_Slash },
	{ "semicolon", kVK_ANSI_Semicolon },
	{ "apostrophe", kVK_ANSI_Quote },
	{ "bracketleft", kVK_ANSI_LeftBracket },
	{ "bracketright", kVK_ANSI_RightBracket },
	{ "grave", kVK_ANSI_Grave },
	{ "backslash", kVK_ANSI_Backslash },
};

static int
key_lookup(const char *name, UInt32 *out)
{
	size_t	 i;

	for (i = 0; i < sizeof(key_codes) / sizeof(key_codes[0]); i++) {
		if (strcmp(key_codes[i].name, name) == 0) {
			*out = key_codes[i].code;
			return 0;
		}
	}
	return -1;
}

/*
 * The hot key arrived.
 *
 * Everything this callback is allowed to do is turn an EventHotKeyID back into
 * the integer the caller handed down and pass it up.  It runs on the main run
 * loop - the same one macos/wsi_ax.m's observers were added to and the same one
 * wsip_pump() turns - so a command dispatched from here reaches the ribbon
 * between two pumps and needs no queue of its own.
 *
 * noErr in both exits, including the one where the parameter could not be read.
 * The alternative, eventNotHandledErr, hands the combination on to whatever is
 * behind us, and there is nothing behind us that should get it: the key was
 * registered by this process and by nobody else.
 */
static OSStatus
key_handler(EventHandlerCallRef next, EventRef ev, void *ctx)
{
	EventHotKeyID	 id;

	(void)next;
	(void)ctx;

	if (GetEventParameter(ev, kEventParamDirectObject, typeEventHotKeyID,
	    NULL, sizeof(id), NULL, &id) != noErr)
		return noErr;
	if (id.signature != KEY_SIGNATURE)
		return noErr;

	wsi_note_key((int)id.id);
	return noErr;
}

int
wsik_open(void)
{
	EventTypeSpec	 spec;

	if (key_open)
		return 0;

	/*
	 * A command-line program has no application object until it asks for
	 * one, and without an application object there is no application event
	 * target for the handler to be installed on.  Accessory rather than
	 * regular: no Dock icon, no menu bar, no window of our own - digitwm
	 * arranges other people's windows and has none.
	 */
	@autoreleasepool {
		NSApplication	*app = [NSApplication sharedApplication];

		[app setActivationPolicy:NSApplicationActivationPolicyAccessory];
	}

	spec.eventClass = kEventClassKeyboard;
	spec.eventKind = kEventHotKeyPressed;

	if (InstallEventHandler(GetApplicationEventTarget(),
	    NewEventHandlerUPP(key_handler), 1, &spec, NULL, NULL) != noErr)
		return -1;

	key_open = 1;
	return 0;
}

int
wsik_bind(int id, unsigned int mods, const char *key)
{
	EventHotKeyID	 hkid;
	EventHotKeyRef	 ref = NULL;
	UInt32		 code, mac = 0;

	if (!key_open || id < 0 || id >= KEY_MAX || key_used[id])
		return -1;
	if (key_lookup(key, &code) != 0)
		return -1;

	if (mods & WSIK_SHIFT)
		mac |= shiftKey;
	if (mods & WSIK_CONTROL)
		mac |= controlKey;
	if (mods & WSIK_OPTION)
		mac |= optionKey;
	if (mods & WSIK_COMMAND)
		mac |= cmdKey;

	hkid.signature = KEY_SIGNATURE;
	hkid.id = (UInt32)id;

	/*
	 * A combination something else already holds is refused here, and that
	 * is the ordinary case rather than an error: Spotlight, an input
	 * source switcher or another manager got there first.  The caller
	 * prints which one was refused, because a key that silently does
	 * nothing is indistinguishable from a broken window manager.
	 */
	if (RegisterEventHotKey(code, mac, hkid, GetApplicationEventTarget(),
	    0, &ref) != noErr)
		return -1;

	key_ref[id] = ref;
	key_used[id] = 1;
	return 0;
}

void
wsik_close(void)
{
	int	 i;

	for (i = 0; i < KEY_MAX; i++) {
		if (!key_used[i])
			continue;
		(void)UnregisterEventHotKey(key_ref[i]);
		key_used[i] = 0;
		key_ref[i] = NULL;
	}
}
