# Portability: what here is X11, and what is arithmetic

**Русская версия: [portability.ru.md](portability.ru.md).**

"Will digitwm run on macOS" sounds like one question and is two, with different
answers. The first — "will the binary build and start" — is yes, under XQuartz,
and it will then manage X11 windows and nothing else. The second — "how much
code has to be rewritten for the ribbon to drive real macOS windows" — is a
number, and it can be measured without owning a Mac.

This document answers the second. Everything about digitwm here is **measured on
this machine**; everything about macOS is **read out of open-source sources and
SDK headers** and carries a citation. We have no Mac, and no claim about macOS
here has been checked by running it.

## Measured: the ribbon's arithmetic does not know about X11

```sh
sh tools/no-x-build.sh
```

The script puts a 17-line stub where the X11 headers go and builds the whole of
`ribbon.c` against it. The result:

| What | How much |
|---|---|
| lines in `ribbon.c` | 1183 |
| X11 names the resulting `ribbon.o` requires | **three**: `XSync`, `XCheckMaskEvent`, `X_Dpy` |
| where those three are | all in `ribbon_settle()`, 7 lines |
| the nine policies as their own translation unit | build with `-Wall -Wextra -Werror` and **not one X11 header** |
| undefined symbols of that unit | one: `Conf` |

Agreement is not only a matter of compiling. The nine policies, extracted into a
separate binary with no X11, and `layout-probe` out of a `cwm` linked against
Xlib, Xft and Xrandr answer identically on **171 conformance vectors** from
`fts/vectors/` and on a further **2700 random vectors** inside the models' domain.
Not one mismatch.

`tools/no-x-build.sh` is also a guard. It fails and names the line if a call to
Xlib reaches the arithmetic: checked by planting `XFlush(X_Dpy)` inside
`ribbon_policy_offset()`.

## Three layers and their sizes

Lines are function bodies in `ribbon.c`, without comments or the file header.

| Layer | Lines | What it is | Cost to port |
|---|---|---|---|
| policy | **142** | the nine `ribbon_policy_*`: numbers in, a number out | 0 |
| ribbon model | **256** | columns, stacks, `ribbon_measure`, `ribbon_place`, `ribbon_scroll` | 0, except the two places below |
| mechanics inside `ribbon.c` | **359** | everything that calls `client_*` and X | rewrite |
| mechanics outside `ribbon.c` | **4880** | `client.c`, `xutil.c`, `xevents.c`, `screen.c`, `kbfunc.c`, `menu.c`, `group.c`, `calmwm.c` | rewrite or drop |

The contract between the ribbon and the mechanics is narrow and wholly visible
in `nm -u ribbon.o`: ten functions — `client_current`, `client_hide`,
`client_show`, `client_resize`, `client_raise`, `client_set_active`,
`client_ptr_save`, `client_ptr_warp`, `region_find`, `xu_ptr_get` — plus `Conf`
and libc. That is the layer a macOS build writes anew, and its size is the
measure of the port.

Separately: **the conformance harness ports for free.** `probe.c` (1076 lines)
builds against the same stub and requires no X11 name at all — only `ribbon_*`,
`Conf`, `xcalloc`, `xstrdup` and libc. So `ribbon.o + probe.o + xmalloc.o` is
`layout-probe` on any system, with no window server. The FTS models, the vectors
and all four harnesses prove the layout on macOS exactly as they do here.

## Where Xlib did leak into the arithmetic

Three places, and only the first costs anything.

**1. Border width inside the geometry.** `ribbon_place()`, `ribbon.c:623-624`:

```c
cc->geom.w = MAX(1, col->w - (cc->bwidth * 2));
cc->geom.h = MAX(1, h - (cc->bwidth * 2));
```

`bwidth` is the X11 window border the manager draws itself
(`XSetWindowBorderWidth`, `XSetWindowBorder` in `client_draw_border()`). On
macOS you cannot draw a border on someone else's window: there is no such
attribute among the writable Accessibility ones (`AXPosition`, `AXSize`,
`AXMinimized`, `AXMain`). Marking the active window becomes a window of our own
on top of everything — which is no longer an "input/output layer" but a new
thing. In the arithmetic `bwidth` goes to zero and both lines become an
assignment.

**2. `ribbon_settle()`, `ribbon.c:731-739.`** `XSync` plus draining
`EnterWindowMask`: the ribbon swallowing the `EnterNotify` events it caused
itself. Seven lines, pure mechanics that ended up next door to the model. The
macOS counterpart is suppressing our own pointer events — same job, different
code.

**3. `sc->rootwin` in `ribbon_current()`, `ribbon.c:397`** — the only Xlib type
(`Window`) the ribbon sees at all, and only for
`xu_ptr_get(sc->rootwin, &x, &y)`. On macOS that is `NSEvent.mouseLocation`.

And two leaks the other way — arithmetic that ended up outside the policy:

**4. The clamp on a facing pair of panels** — `screen.c:323-335`. Two panels
opposite each other cannot take more than there is; the decision is about the
pair, while each half of the pair is decided by its own policy. There is no
model for this rule, and a port has to derive it again.

**5. `ribbon_policy_width()` reads `Conf.ribbonwidth[preset]`** — so it is not a
function of its arguments alone. The model `fts/column-width.fts` keeps
33/50/67/100 as constants and says so in its own header: conformance covers the
default set, not one overridden in `cwmrc`.

## What macOS gives without disabling system protection

None of the following **requires SIP to be off**. One permission is needed:
`Accessibility` in the privacy settings (`AXIsProcessTrustedWithOptions`).

| What the ribbon needs | What macOS has | Source |
|---|---|---|
| move and resize another app's window | `kAXPositionAttribute` = `"AXPosition"`, `kAXSizeAttribute` = `"AXSize"`, both writable for windows | `AXAttributeConstants.h:608-641`; yabai does this, `src/window_manager.c:415-433` |
| check that a window will accept it | `AXUIElementIsAttributeSettable` | `AXUIElement.h:204` |
| learn of a new window, a close, a move, a focus change | `AXObserverCreate` plus `kAXWindowCreatedNotification`, `kAXUIElementDestroyedNotification`, `kAXWindowMovedNotification`, `kAXWindowResizedNotification`, `kAXFocusedWindowChangedNotification` | `AXNotificationConstants.h:57,113,123,133,194` |
| learn of an app starting and quitting | Carbon `kEventAppLaunched`/`kEventAppTerminated`, `NSWorkspace` | yabai, `src/process_manager.c:161-251`, `src/workspace.m:157-180` |
| focus a window and raise it | `_SLPSSetFrontProcessWithOptions` plus `AXUIElementPerformAction(kAXRaiseAction)` | yabai, `src/window_manager.c:1324-1335` |
| the workable area instead of struts | `NSScreen.visibleFrame` — already without the Dock and the menu bar; `safeAreaInsets` for the notch | Apple documentation |
| hotkeys **that consume the key** | `RegisterEventHotKey` (Carbon): needs no permission, **swallows** the key — the app never sees it | AeroSpace (`Package.swift:28`) and AltTab do this |
| focus following the pointer | `CGEventTap` with `.defaultTap` (Accessibility permission) | yabai, `src/mouse_handler.c` |

What **does** require SIP off, per the yabai wiki page "Disabling System
Integrity Protection" and `src/sa.h`: create, destroy, reorder or switch a
Space; window opacity; shadows; layers and ordering; sticky windows;
picture-in-picture; smooth animations. The ribbon needs none of it.

Moving a window to another Space and switching Spaces **work with SIP on**, but
through private SkyLight calls (`SLSMoveWindowsToManagedSpace`,
`src/space_manager.c:686-701`) and a synthesized gesture
(`space_manager.c:927-983`, whose comment says outright: *"MacOS does not have
an API that allows for space activation"*).

## What the ribbon loses, by name

**1. Insertion without a jump is gone.** This is the one promise the port
certainly cannot keep, and it costs more than everything else together.

Here a window is given its place **inside the handling of `MapRequest`, before
it is shown** — `client.c:123-135`, under `XGrabServer`. That is what
`SubstructureRedirect` is: the manager receives the request instead of the
server acting on it.

macOS has nothing of the kind. `AXObserverCallback` is declared `void`
(`AXUIElement.h:446`) — a notification with no return channel, nothing to refuse
with and nothing to substitute geometry with; `kAXWindowCreatedNotification` is
documented as "a window was created", past tense. Every open-source macOS window
manager works after the fact — AeroSpace's hook is literally called
`on-window-detected`. So a new window will first appear where its application
put it, and only then jump into its column, one IPC round-trip later.

*(The claim "there is no such API" rests on an exhaustive read of the
Accessibility headers and on the behaviour of every open manager, not on a
statement from Apple: Apple does not document the absence of a feature.)*

The second promise — "an insertion changes the geometry of no window already
open" — is pure arithmetic plus one move of a neighbour, and it carries over
whole.

**2. `ribbonhide` will stop hiding.** Here it is `XUnmapWindow`. macOS will not
let you unmap someone else's window; the nearest thing is minimising to the
Dock, which is an action the user sees rather than "take it off the canvas".
That leaves parking it past the edge of the screen — and AeroSpace, which does
exactly that, documents the remainder: *"macOS doesn't allow to place windows
outside the visible area entirely. You will still be able to see a 1 pixel
vertical line"* (`docs/guide.adoc:438-455`). The ribbon's ordinary state —
columns half past the edge — survives: a window may be partly off-screen.

**3. Groups as they are will not survive.** cwm's nine groups map onto Spaces,
and Spaces have no public API at all — stated both in AeroSpace's documentation
and in yabai's sources. With SIP on you can move a window into an existing Space
and switch to it; create, destroy and reorder you cannot. On top of that the
system reorders Spaces by itself unless "Automatically rearrange Spaces" is
turned off — which Amethyst's README asks for.

**4. Full-screen windows will not join the ribbon.** A window blown up with the
green button lives in its own Space, and yabai refuses to manage it
(`src/window_manager.c:122`). The good news: the insertion policy already
allowed for this — `RIBBON_PLACE_FULL` in `ribbon_policy_insert()`. The
arithmetic needs no change.

**5. Native macOS tabs will break.** Terminal.app and Finder with tabs — the
caveat comes from yabai's own README. The ribbon will see one window where the
user sees several tabs.

**6. Struts will not be needed.** macOS has no `_NET_WM_STRUT_PARTIAL`, and does
not need one: `NSScreen.visibleFrame` already returns the area without the Dock
and the menu bar. Two of the nine policies — `ribbon_policy_span` and
`ribbon_policy_reserve`, 26 lines — are simply never called on macOS. They stay
in the tree for X11.

## Latency: the real work of the port, and it is in our code

Measured here (`doc/baseline.md`): the manager's own share of an insertion is
1–10 ms typically, median 4–5 ms, worst under load 887 ms.

On macOS every geometry write is a synchronous IPC round-trip into another
application's event loop. Swindler's README puts it plainly: *"you ask an
application for a window's position, wait for it to respond… it works at the
mercy of the remote application's event loop, which can lead to long,
multi-second delays"*. Silica additionally sets the size twice — before and
after the position — with the comment *"the accessibility APIs are really
finicky with setting size… this still occasionally silently fails"*
(`SIAccessibilityElement.m:195-226`).

And our code is currently arranged to make that hurt. `ribbon_sync_one()`
(`ribbon.c:685-721`) calls `client_resize()` for **every** window of **every**
column on **every** call, without asking whether the geometry changed at all.
And `client_init()` calls `ribbon_sync()` twice — `client.c:148` and
`client.c:202`. With eight windows open, opening one more comes to
2 × 8 × 3 = 48 synchronous IPC round-trips where X11 gets away with four
buffered asynchronous requests per window and a single `XSync`.

**This is fixed in the ribbon, not in macOS**: compare `cc->geom` with what was
last pushed and push only what moved. On an insertion that is the new window
plus the columns to the right of it, not the whole ribbon. On X11 such a check
buys nothing measurable — which is why it was never written; on macOS it is the
difference between usable and not.

## Permissions, delivery, licences

**Permissions.** One grant: Accessibility. SIP stays on, root is not needed. Our
own program, however, **cannot be sandboxed** — Apple DTS states it directly:
*"It's not possible to use the Accessibility APIs from a sandboxed app."* So the
Mac App Store is closed; brew is open.

**Delivery.** The existence proof is yabai: a plain Mach-O with no bundle, its
`Info.plist` welded into a `__TEXT,__info_plist` section with `-sectcreate`
(`makefile:4`), signed ad-hoc — `codesign -fs -` in the Homebrew formula.
Notarisation is not needed for a command-line program from brew. One trap: the
TCC grant is keyed to the signature, so rebuilding from source without a stable
signing identity asks for permission again every time.

Worth noting that everything around the manager already ports: `session/install.sh`
lays itself out on macOS, and by `session/README.md` brew installs 24 tools out
of 24 — more than apt or dnf manage. The one thing that does not install is the
window manager; `bootstrap.sh` currently refuses on `Darwin`, with a reason.

**Licences.** yabai, Amethyst, Silica, AeroSpace, soffes/HotKey — all MIT,
compatible with our ISC as long as their notices are kept. Copying an idea —
which call, in which order — creates no obligation at all.

Two traps:

- **AltTab is GPL-3.0.** It is the best open documentation of
  `CGSSetSymbolicHotKeyEnabled` and of what swallows an event. Reading it for
  facts is fine, taking its code is not: `tools/check-licensing.py` would fail
  the build, and this is exactly the case that gate was written for (see
  `NOTICE` on papersway and sdorfehs).
- **`CGSInternal`** — private-API headers with no LICENCE file in the
  repository, MIT only in the comments of individual files. Do not bring them
  into the tree; declare the symbols we need ourselves, as AeroSpace does.

## The answer

**This is a port, not a new product — and here is what proves it.** What a macOS
build would share is not "the idea of a ribbon" but 1183 lines of `ribbon.c` and
1076 lines of `probe.c`, which build without a single X11 header and answer with
the same numbers on 2871 vectors. Zero lines of arithmetic need rewriting.

The price:

| Item | Estimate | What it rests on |
|---|---|---|
| the arithmetic | **0 lines** | measured |
| edits inside `ribbon.c` | ~10 lines | the three leaks above |
| a new input/output layer | **2500–4000 lines** | estimated from the neighbours: Silica — the whole AX wrapper — is 2434 lines; AeroSpace's helper layer 928; yabai's process and event attachment about 2400 |
| dropped | 4880 lines of X11 mechanics, of which `xutil.c` (579, all EWMH) has no replacement at all: macOS has no EWMH | measured |
| a new obligation | a "what changed" check in `ribbon_sync_one()` | see above |
| impossible | insertion without a jump | `AXObserverCallback` returns `void` |

The 2500–4000 is an estimate, not a measurement: it is taken from the sizes of
other projects doing the same job, and it cannot be checked until there is a
Mac.

What is **not** in that price, and would have cost the most if it were:
disabling system protection. The ribbon needs it for nothing.
