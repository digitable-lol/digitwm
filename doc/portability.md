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

The script puts a 13-line stub where the X11 headers go and builds the whole of
`ribbon.c` against it. The result:

| What | How much | Measured by |
|---|---|---|
| lines in `ribbon.c` | 1640 | `wc -l ribbon.c` |
| X11 names the resulting `ribbon.o` requires | **zero** | `sh tools/no-x-build.sh` |
| what the ribbon does ask of the window system | **11 operations**, every one declared in `wsi.h` | `sh tools/no-x-build.sh` |
| the ten policies as their own translation unit | build with `-Wall -Wextra -Werror` and **not one X11 header** | `sh tools/no-x-build.sh` |
| undefined symbols of that unit | `Conf`, the library's ten entry points and two lines of glue — checked name by name | `sh tools/no-x-build.sh` |
| the arithmetic itself (`ribbon-flang/out-c/`) | builds with `-Wall -Wextra -Werror -pedantic` and asks for **nothing** outside its own runtime — no `Conf`, no `wsi.h` | `sh tools/no-x-build.sh` |

Every number in this document that the tree can produce is checked against the
tree by `sh tools/check-doc-numbers.sh`: it runs the commands named in the
"Measured by" column and fails, naming both values, when a number here has
drifted. The numbers it deliberately does not check — and why — are listed at
the foot of that script.

Agreement is not only a matter of compiling, and two runs measure two different
things. Neither of them is "2871 vectors": that number was a conformance count
taken before `fts/flang/strut-pair.flang` existed, plus a random run no command in this
tree produces. What the tree prints today is below, and each number names the
command that prints it.

**The specs against the live window manager.** `flang io fts/flang/conformance.flang`
prints `векторов 201` and not one mismatch; `flang io fts/flang/layout.flang`
prints `сценариев 14` and not one differing byte. That is **215** comparisons,
one per vector, and the vectors are counted in the tree rather than asserted:
`grep -c '^    вариант «' fts/flang/conformance.flang` gives **201** scalar
vectors and `grep -c '^    запись «Сценарий»' fts/flang/layout.flang` gives
**14** whole-layout scenarios. The count fell from the 450 the Node harness
printed, and nothing was lost with it: 450 counted every vector twice, once per
language surface, plus 10 checks that the model's fields matched a table in the
harness. There is one surface now, and no table.

**A build with no X11 at all, against the X11-linked one.** The same `ribbon.c`
compiled to WebAssembly and `layout-probe` out of a `cwm` linked against Xlib,
Xft and Xrandr answer identically on **500 random cases — 4386 windows** —
inside the models' domain. That is the run CI performs:

```sh
sh tools/wasm-layout/build.sh
node tools/wasm-layout/check.mjs --wm ./cwm --cases 500
# случаев:  500, окон в них: 4386
# двоичный файл и WebAssembly ответили одинаково везде
```

`tools/no-x-build.sh` is also a guard. It fails and names the line if a call to
Xlib reaches the arithmetic: checked by planting `XFlush(X_Dpy)` inside
`ribbon_policy_offset()`.

## Three layers and their sizes

Lines are function bodies in `ribbon.c`, without comments or the file header.

| Layer | Lines | What it is | Cost to port |
|---|---|---|---|
| policy | **142** | the ten `ribbon_policy_*`: numbers in, a number out | 0 |
| ribbon model | **256** | columns, stacks, `ribbon_measure`, `ribbon_place`, `ribbon_scroll` | 0, except the two places below |
| mechanics inside `ribbon.c` | **359** | everything that calls `client_*` and X | rewrite |
| mechanics outside `ribbon.c` | **5129** | `client.c`, `xutil.c`, `xevents.c`, `screen.c`, `kbfunc.c`, `menu.c`, `group.c`, `calmwm.c` | rewrite or drop |

The contract between the ribbon and the mechanics is narrow and wholly visible
in `nm -u ribbon.o`: **11 window-system operations** — `client_current`,
`client_geom_current`, `client_hide`, `client_ptr_save`, `client_ptr_warp`,
`client_raise`, `client_resize`, `client_set_active`, `client_show`,
`region_pointer`, `wsi_settle` — plus `Conf`, `conf_ribbonrule_match`, `xcalloc`,
`xstrdup` and libc. Every one of the eleven is declared in `wsi.h`, and
`tools/no-x-build.sh` fails if the ribbon calls a twelfth. That is the layer a
macOS build writes anew, and its size is the measure of the port.

Separately: **the conformance harness ports for free.** `probe.c` (1172 lines,
`wc -l probe.c`) builds against the same stub and requires no X11 name at all —
only `ribbon_*`, `Conf`, `xcalloc`, `xstrdup` and libc. So
`ribbon.o + probe.o + xmalloc.o` is `layout-probe` on any system, with no window
server. The specs, the vectors that live inside them and both remaining
harnesses — `invariants.mjs` and `hotplug.mjs` — prove the layout on macOS
exactly as they do here.

## Where Xlib did leak into the arithmetic

One place is still open, and it is the one that costs something. Two more were
closed by the platform seam: `ribbon.o` now requires **zero** X11 names
(`sh tools/no-x-build.sh`).

**1. Border width inside the geometry.** `ribbon_place()`, `ribbon.c:846-847`:

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

**2. `ribbon_settle()` — CLOSED.** `XSync` plus draining `EnterWindowMask` — the
ribbon swallowing the `EnterNotify` events it caused itself — is now
`wsi_settle()` in `xutil.c`, on the far side of the seam, and the ribbon calls it
as an operation of `wsi.h`. The macOS counterpart is suppressing our own pointer
events: same job, different code, and no edit to the ribbon.

**3. `sc->rootwin` and `xu_ptr_get()` — CLOSED.** The only Xlib type (`Window`)
the ribbon ever saw is gone: the ribbon now asks `region_pointer()`, another
operation of `wsi.h`. On macOS that is `NSEvent.mouseLocation`, written once
behind the seam.

And one leak the other way — arithmetic that ended up outside the policy:

**4. The clamp on a facing pair of panels — CLOSED.** It was `screen.c:323-335`:
two panels opposite each other cannot take more than there is; the decision is
about the pair, while each half of the pair is decided by its own policy, and
the rule had no model. It is now `ribbon_policy_pair()` and the spec
`fts/flang/strut-pair.flang`: 30 examples, which are its 30 vectors, and a
mutation in `tools/check-flang-mutants.sh`. Ten policies, ten specs. A port has
nothing left to derive.

**5. `ribbon_policy_width()` reads `Conf.ribbonwidth[preset]`** — so it is not a
function of its arguments alone. The spec `fts/flang/column-width.flang` keeps
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
and the menu bar. Three of the ten policies — `ribbon_policy_span`,
`ribbon_policy_reserve` and `ribbon_policy_pair` — are simply never called on
macOS. They stay in the tree for X11.

## The flicker, in milliseconds

"A new window appears in the wrong place and then jumps" is not a feeling, it
is a number, and the decision about the port turns on it. **We have no Mac, so
we did not measure it, and nothing below pretends we did.** What is here is
three things: what the number is made of, what parts of it we could measure
here, and a harness that produces the rest in one command on a machine that has
a Mac — `tools/macos-flicker/`, `sh run.sh`.

The flicker is a sum of four terms:

| Term | What it is | Where its number comes from |
|---|---|---|
| 1. the window is drawn → we learn of it | the compositor has already shown it | **not ours, not measured by us** |
| 2. the layout decision | the ten policies | **0.06 ms** per insertion, measured |
| 3. the geometry writes | AX round trips into the app | **count measured: 9 writes / 18 round trips** per insertion |
| 4. one screen refresh | the compositor shows the move | 8.3 ms at 120 Hz, 16.7 at 60 |

Term 2 is measured on the same code compiled to WebAssembly (`node
tools/wasm-layout/check.mjs`: eight insertions in 0.51 ms), and it is
negligible. Term 3 is where our own work went: the count halved, above.

Term 1 is the one that cannot be removed, and the published evidence about it
is worth quoting rather than paraphrasing.

- **It fires after the paint, by design.** yabai's author, on the issue about
  exactly this ([yabai#1437](https://github.com/asmvik/yabai/issues/1437)):
  *"Yabai doesn't know about the window before it is rendered on the screen, as
  we cannot intercept the compositor."* And on why it varies: *"The amount of
  time it takes for us to get notified about a window that we can manage varies
  heavily between applications."*
- **Even hiding it as fast as technically possible leaves 20–30 ms visible.**
  Same thread, same author, testing a branch that hides the window the instant
  it is detected: *"probably visible for like ~30ms, but I do notice it"*, and
  of another case *"~20ms. It was very noticeable for me as a user"*. That
  branch needs SIP off — see below — so 20–30 ms is a **lower bound**, not our
  number.
- **The earliest a manager can learn, without SIP, has been measured by
  someone else.** alt-tab-macos subscribes to SkyLight's per-window
  notifications (event 811 "created", 815 "ordered in") instead of the
  Accessibility one, and its sources carry the measurement, taken on macOS 26.5
  with the WindowServer driven to ~99 % CPU: the first per-window event **never
  arrived earlier than 7.1 ms**, p50 7.2 ms, p99 12.7 ms
  ([alt-tab-macos](https://github.com/lwouis/alt-tab-macos),
  `src/switcher/state/Applications.swift`). Event 815 is, by that project's own
  comment, *"the true 'pixels on screen' moment"*. This path needs no SIP off —
  only Accessibility — but it is still a notification: you learn earlier, you
  do not intercept.

So without disabling system protection the flicker is bounded below by roughly
**7 ms of learning plus one round trip plus a frame**, and the only thing that
actually hides it — making the window transparent until it is placed — lives in
yabai's Dock.app payload and therefore behind SIP, which Tahoe broke twice and
the macOS 27 betas break again.

The cost of a single round trip has **no published per-application number**;
what is published are the extremes, and they are wide: the default AX messaging
timeout is 6 s (yabai and alt-tab both cut it to 1 s), and one Accessibility
read during a window animation is recorded at ~500 ms against 11 ms for the
same fact asked of the WindowServer in a batch. That spread is exactly why
term 3 is a count worth halving, and why the harness measures the unit cost per
application rather than assuming one.

**Nothing about any of this changed in macOS 15, 26 or the 27 betas as of
August 2026**: no public placement or tiling API for third parties, no new
entitlement, no pre-display hook. Apple's own tiling exposes no hooks.

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

And our code was arranged to make that hurt. That is now **measured and
fixed**, and the number is not an estimate: `tools/measure-syncs.sh` counts the
wire rather than the code — `tools/count-geom.c` goes under `LD_PRELOAD` and
records every call that reached libX11.

Opening the ninth window with eight already open:

| | geometry writes | of them repeating what was already there | AX round trips on macOS |
|---|---|---|---|
| before | 19 | 10 | 38 |
| after | **9** | **0** | **18** |

Over the whole nine-window session: 44 writes instead of 99, and 56 % of the
old ones were restating a geometry the window already had. The estimate this
document used to carry — "2 × 8 × 3 = 48" — was arithmetic on paper; the wire
says 38, and this is the case where measuring was worth it.

Fixed in the ribbon, not in macOS: `client_geom_current()` compares `cc->geom`
against what the window was last given, and `ribbon_sync_one()` says nothing
when nothing changed. The cache is safe because a window on the ribbon has one
owner: a `ConfigureRequest` from it is denied (`xev_handle_configurerequest`),
and the single path that changes geometry behind that record's back drops it
itself.

On X11 the check buys nothing measurable — an insertion is still 2–4 ms from
`XMapWindow` and still redraws no neighbour (`tools/measure-insert.sh`, nine
windows). Its point is elsewhere: on macOS each of those writes is a round trip
into another process's event loop, and half of them are no longer made.

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
compatible with our BSD-2-Clause, and with the ISC of the inherited files, as
long as their notices are kept. Copying an idea —
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
build would share is not "the idea of a ribbon" but 1640 lines of `ribbon.c` and
1172 lines of `probe.c` (`wc -l ribbon.c probe.c`), which build without a single
X11 header and answer with the same numbers on the **215 vectors** of
`fts/flang/` — 215 checks, `conformance.flang` and `layout.flang` — and on
**500 random cases**
besides (`tools/wasm-layout/check.mjs --cases 500`). Zero lines of arithmetic
need rewriting.

The price:

| Item | Estimate | What it rests on |
|---|---|---|
| the arithmetic | **0 lines** | measured |
| edits inside `ribbon.c` | ~10 lines | the one leak still open above |
| a new input/output layer | **2500–4000 lines** | estimated from the neighbours: Silica — the whole AX wrapper — is 2434 lines; AeroSpace's helper layer 928; yabai's process and event attachment about 2400 |
| dropped | 5129 lines of X11 mechanics, of which `xutil.c` (605, all EWMH) has no replacement at all: macOS has no EWMH | measured |
| a new obligation | a "what changed" check in `ribbon_sync_one()` | **done**: 19 writes per insertion became 9 |
| impossible | insertion without a jump | `AXObserverCallback` returns `void`; the jump is bounded below by ~7 ms + one round trip + a frame |

The 2500–4000 is an estimate, not a measurement: it is taken from the sizes of
other projects doing the same job, and it cannot be checked until there is a
Mac.

What is **not** in that price, and would have cost the most if it were:
disabling system protection. The ribbon needs it for nothing — except one thing:
without it the flicker on insertion cannot be hidden, so the flicker stays.

**Is a port like that good enough.** Yes, in everything but one promise, and the
promise is named: insertion without a jump. The jump is neither a trifle nor
fatal — it is ~7 ms of learning (a third party's measurement, macOS 26.5) plus
one IPC round trip plus a frame, which is visible to the eye and not removable
with SIP on. The exact number for a given machine and given applications is what
`tools/macos-flicker/` produces in one command; our own half of it is already
halved.
