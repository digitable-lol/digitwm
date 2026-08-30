# digitwm on macOS: the plan of the port

**Русская версия: [macos.ru.md](macos.ru.md).**

## The decision

digitwm is ported to macOS as a **real window manager that drives real macOS
windows through the Accessibility API**. The owner took that decision in August
2026, and it closes two answers that had been given to "what about the Mac"
before it.

- **A webview application on Tauri is not the answer.** It was a different
  product wearing the same name: a window of ours with our own contents, which
  never touches the windows the user already has. The ribbon is arithmetic about
  someone else's windows, and a webview has none.
- **A terminal multiplexer is not the answer to "the Mac" either.** Route A of
  [terminal.md](terminal.md) is **not cancelled and not folded into this plan**.
  It answers a different question — working over ssh, where the panes are
  processes on a remote machine — and it stays a task of its own, with its own
  open questions and its own three thresholds still to be filled in. A Mac user
  who wants the ribbon over his real windows is not served by it.

Two consequences land in the tree with this document: `bootstrap.sh` stops
sending a Mac to Tauri and names this plan instead, and this plan joins the
table of documents in both READMEs.

## What this document is, and where its numbers come from

[portability.md](portability.md) answers "what would the port cost". This one
answers **"what do we do, in what order, and by what sign do we stop"**.

It adds no measurement of its own. Every number below is taken from
`portability.md` and carries the same label it carries there: measured here,
published by someone else, or an estimate. Where a number is missing, this
document says which command produces it rather than guessing it.

The plan starts from a fact rather than from a wish: **not one line of the port
exists.** Nothing in the tree calls the Accessibility API — `client.c:568` and
`tools/count-geom.c:26` only mention it in comments. The whole of macOS here is
`tools/macos-flicker/`, a harness written so that a person with a Mac can get
the missing numbers with one command. And **nobody here has a Mac**
(`portability.md:13`), which is why the first stage below is not code.

## The contract: what the ribbon asks of the window system

The ribbon's arithmetic needs **0 lines** of rewriting — measured: `ribbon.c`
(1183 lines) builds whole against a 17-line stub in place of the X11 headers,
and the resulting `ribbon.o` requires exactly three X11 names. Everything the
ribbon does to the world it does through **ten functions**, and that list —
visible in `nm -u ribbon.o`, plus the settings record `Conf` and libc — is the
whole contract. It is what a macOS build implements anew.

A neighbouring worker is currently lifting that list into a header of its own.
The names it will carry are not settled, so the contract is written here by
meaning.

| What the ribbon asks for | What X11 does today | What macOS has for it |
|---|---|---|
| which window is the focused one right now | the manager's own record | our own record, kept current by `kAXFocusedWindowChangedNotification` |
| take a window off the canvas | `XUnmapWindow` | **nothing equivalent** — parking past the edge; see loss 2 |
| put it back on the canvas | `XMapWindow` | a position write back into the visible area |
| give a window a rectangle | `XMoveResizeWindow` | `kAXPositionAttribute` and `kAXSizeAttribute`, both writable, with `AXUIElementIsAttributeSettable` to ask first |
| raise a window above its neighbours | `XRaiseWindow` | `AXUIElementPerformAction(kAXRaiseAction)` |
| make a window the active one | focus plus the border the manager draws | `_SLPSSetFrontProcessWithOptions` plus `kAXRaiseAction`; the border has no counterpart at all — see "not a stage" at the end of the stages |
| remember where the pointer stood inside a window | `xu_ptr_get` on the window | reading is `NSEvent.mouseLocation` |
| put the pointer back where it stood | `XWarpPointer` | **no counterpart is cited in `portability.md`** — an open question, to be answered before stage 4 |
| which output a point belongs to, and its workable area | Xrandr plus `_NET_WM_STRUT_PARTIAL` | `NSScreen.visibleFrame`, already without the Dock and the menu bar; `safeAreaInsets` for the notch |
| where the pointer is now | `xu_ptr_get` on the root window | `NSEvent.mouseLocation` |

Nine of the ten have a counterpart that was read out of open sources or SDK
headers. One — putting the pointer back — does not, and this plan refuses to
invent a call for it: name it, with a source, before the layer is written.

**Inside `ribbon.c` the port costs about 10 lines**, in three places, all named
in `portability.md`:

1. the window border inside the geometry (`ribbon.c:623-624`) — on macOS you
   cannot draw a border on someone else's window, so `bwidth` becomes zero and
   both lines become plain assignments;
2. `ribbon_settle()` (`ribbon.c:731-739`, 7 lines) — `XSync` plus draining
   `EnterWindowMask` is the ribbon swallowing the pointer events it caused
   itself; the macOS counterpart does the same job with different code;
3. `sc->rootwin` in `ribbon_current()` (`ribbon.c:397`) — the only Xlib type the
   ribbon ever sees, and only to ask where the pointer is.

**What is thrown away: 4880 lines** of mechanics outside `ribbon.c`. Of those,
`xutil.c` — 579 lines, all EWMH — has no replacement at all, because macOS has
no EWMH.

**What is written anew: 2500–4000 lines**, and this is an **estimate, not a
measurement**. It is read off the neighbours doing the same job: Silica's whole
AX wrapper is 2434 lines, AeroSpace's helper layer 928, yabai's process and
event attachment about 2400. The estimate cannot be checked until there is a
Mac.

## The stages, and what each one lets you see

Stage 1 needs a Mac and no code; stage 2 needs no Mac at all; stage 3 needs a
Mac but no window server. **Stage 4 must not begin before stage 1 has produced
its numbers**, because stage 1 is what the stop thresholds are checked
against.

### Stage 1 — the measurement, and the owner performs it

```sh
sh tools/macos-flicker/run.sh          # eight windows, 1500 ms apart
sh tools/macos-flicker/stub-build.sh   # what can be checked here, on Linux
```

One permission is needed — Accessibility for whatever runs `axcost`. **SIP is
not disabled**, here or anywhere else in this plan. `run.sh` says so itself if
the permission is missing.

*Result you can see:* the three numbers of the "stop thresholds" section below,
printed by one command, on a machine that has a Mac. Until they exist, every
statement about whether this port is pleasant to use is a guess.

### Stage 2 — the seam, in the X11 build

The contract above becomes one header, with the X11 implementation behind it and
nothing else changing. This is a neighbouring worker's task, not this document's
claim; it is a stage of the port because the macOS layer has nowhere to attach
until it exists.

*Result you can see:* `sh tools/no-x-build.sh` still green — it fails and names
the line if an Xlib call reaches the arithmetic — and the CI set from
[build.md](build.md) unchanged: the ten policies against `layout-probe` on 171
conformance vectors and 2700 random ones, 2871 in all, not one mismatch.

### Stage 3 — the conformance harness, on the Mac, with no window server

`probe.c` (1076 lines) needs no X11 name at all, so `ribbon.o + probe.o +
xmalloc.o` is `layout-probe` on any system. Nothing is ported to get this; it is
built.

*Result you can see:* the same 2871 vectors answering identically when run on
macOS. The layout is then proven on macOS before a single window has been moved
there.

### Stage 4 — the input/output layer, geometry only

Permission, enumerating the windows of running applications, reading and writing
geometry, one screen, **no events yet**: the ribbon is told by hand to lay out
what is already open.

*Result you can see:* windows that were already open line up into their columns,
and `axcost cost <pid>` gives the price of one write and one read **per
application** — the number nobody has published for any application.

### Stage 5 — events

`AXObserverCreate` with the five notifications (window created, destroyed,
moved, resized, focus changed), plus Carbon `kEventAppLaunched` /
`kEventAppTerminated` and `NSWorkspace` for applications starting and quitting.

*Result you can see:* a new window lands in its column **after the fact** — and
the flicker becomes a measured number instead of a bound. This is the stage the
port is judged at.

### Stage 6 — input

`RegisterEventHotKey`, which needs no permission and **swallows** the key, so
the application never sees it; `CGEventTap` for focus following the pointer,
under the Accessibility grant already held.

*Result you can see:* the ribbon's commands work from the keyboard, with the
same key map a `cwmrc` describes.

### Stage 7 — delivery

The brew formula, the signing identity, the permission dialogue.

*Result you can see:* on a clean Mac, an install from brew and **one**
permission dialogue, once — not once per rebuild.

**Not a stage, and deliberately outside this plan:** marking the active window.
On X11 that is a border the manager draws on someone else's window; macOS has no
such writable Accessibility attribute, and the nearest replacement is a window
of our own on top of everything — which is not an input/output layer any more
but a new thing. It is the owner's separate decision, not a step of the port.

## What the ribbon will not have on macOS, by name

Six losses, and none of them is a temporary state of the work.

**1. Insertion without a jump is gone. Permanently.** This is the one promise
the port cannot keep. Here a window is given its place **inside the handling of
`MapRequest`, before it is shown** (`client.c:123-135`, under `XGrabServer`):
the manager receives the request instead of the server acting on it. macOS has
nothing of the kind — `AXObserverCallback` is declared `void`
(`AXUIElement.h:446`), a notification with no return channel: nothing to refuse
with, nothing to substitute geometry with, and `kAXWindowCreatedNotification` is
documented in the past tense. A new window will first appear where its
application put it and only then jump into its column. The floor, measured by
someone else and not by us: the first per-window event **never arrived earlier
than 7.1 ms** (p50 7.2, p99 12.7 — alt-tab-macos on macOS 26.5), and one screen
frame is 8.3 ms at 120 Hz, 16.7 at 60.

The ribbon's *second* promise — an insertion changes the geometry of no window
already open — is arithmetic plus one move of a neighbour, and it **carries over
whole**.

**2. `ribbonhide` will stop hiding.** Here it is `XUnmapWindow`. macOS will not
unmap someone else's window; minimising to the Dock is an action the user sees,
not "take it off the canvas". What is left is parking past the edge, and
AeroSpace, which does exactly that, documents the remainder: *"macOS doesn't
allow to place windows outside the visible area entirely. You will still be able
to see a 1 pixel vertical line"*. So: a **visible 1-pixel strip** for every
hidden window. The ribbon's ordinary state — columns half past the edge —
survives.

**3. The nine groups will not survive.** cwm's groups map onto Spaces, and
Spaces have no public API at all. With SIP on, a window can be moved into an
existing Space and switched to; create, destroy and reorder cannot. The system
also reorders Spaces by itself unless "Automatically rearrange Spaces" is turned
off.

**4. Full-screen windows will not join the ribbon.** A window blown up with the
green button lives in its own Space. The insertion policy already allowed for
this — `RIBBON_PLACE_FULL` in `ribbon_policy_insert()` — so **the arithmetic
needs no change**.

**5. Native macOS tabs will break.** Terminal.app and Finder with tabs: the
ribbon will see one window where the user sees several.

**6. Struts will not be needed** — which is a loss of code, not of behaviour.
`NSScreen.visibleFrame` already excludes the Dock and the menu bar, so three of
the ten policies — `ribbon_policy_span`, `ribbon_policy_reserve` and
`ribbon_policy_pair` — are **never called** on macOS. They stay in the tree for
X11.

## The stop thresholds: what to measure on the first Mac, and with what

[terminal.md](terminal.md) leaves its three thresholds empty on purpose: that
run had nothing to measure with, and inventing a threshold is the worst thing
one can do to a threshold. Here the situation is different in one respect — **the
instrument exists and is in the tree**. So this document does not fill in the
numbers either, but it names, for each of them, the command that produces it and
what it is to be weighed against.

| What to measure | The instrument | What is already known to weigh it against |
|---|---|---|
| **the flicker**: how long a window stands visible in the wrong place | the helper half of `sh tools/macos-flicker/run.sh` — `flicker` prints `open`/`paint`/`moved` per window, and the flicker of one window is `moved − paint` | nothing of ours: we have never measured it. The only published figure is 20–30 ms, and it belongs to a manager that hides the window the instant it is detected — which needs SIP off, so it is a **lower bound**, not a target |
| **the cost of one geometry write, and one read, per application** | `axcost cost <pid> [n]` — min, median, p95, max | multiply by the count we already measured: **9 writes / 18 AX round trips** per insertion (it was 19/38). The published extremes are wide: the default AX messaging timeout is 6 s (yabai and alt-tab cut it to 1 s), and one read during a window animation is recorded at ~500 ms |
| **the delay of the notification**: window created → we learn of it | `axcost watch <pid> <sec> <n>` — it prints the path "notice → read → write" per window | never earlier than **7.1 ms**, p50 7.2, p99 12.7 (alt-tab-macos, macOS 26.5, WindowServer at ~99 % CPU) |

**The threshold numbers are the owner's**, and they are written after stage 1,
not before. Two are proposed here, and they are proposals rather than
measurements:

- if the **median** cost of one geometry write, times the nine writes an
  insertion makes, exceeds one screen frame (8.3 ms at 120 Hz, 16.7 at 60), then
  the jump is not "one frame late" but several frames long, and stage 5 is not
  worth finishing in this shape;
- if the **p95** of one write approaches the ~500 ms recorded for a read during
  an animation, the ribbon is unusable whenever anything on screen animates, and
  the port is closed rather than polished.

Two things are worth stating so that no work is spent arguing with them. The
floor of the flicker — 7.1 ms of learning, plus one round trip, plus one frame —
**cannot be removed by any amount of our code**; the only thing that actually
hides the jump is making the window transparent until it is placed, and that
lives behind SIP. And for scale, the manager's own share of an insertion on X11,
measured here, is 1–10 ms typically, median 4–5, worst under load 887 ms
([baseline.md](baseline.md)).

The layout decision itself is not a candidate for any threshold: it is **0.06 ms
per insertion** (eight insertions in 0.51 ms, measured on the same code compiled
to WebAssembly), and it is negligible against everything above.

## Delivery

**One permission: Accessibility.** SIP stays on, root is not needed. What would
require SIP off — creating, destroying, reordering or switching a Space, window
opacity, shadows, layers, sticky windows, picture-in-picture, smooth animations
— **the ribbon needs for nothing.**

**The Mac App Store is closed to us.** Apple DTS states it directly: it is not
possible to use the Accessibility APIs from a sandboxed app, and the store
requires the sandbox. **brew is open**, and the existence proof is yabai: a plain
Mach-O with no bundle, its `Info.plist` welded into a `__TEXT,__info_plist`
section, signed ad-hoc. Notarisation is not needed for a command-line program
installed from brew.

**One trap, and it is a decision rather than a detail: the TCC grant is keyed to
the signature.** Rebuilding from source without a stable signing identity asks
the user for the permission again, every time. A stable identity therefore
belongs to stage 7, not to a later tidying-up.

Everything around the manager already ports: `session/install.sh` lays itself
out on macOS, and brew installs **24 tools out of 24** — more than apt or dnf
manage. The one thing that does not install is the window manager, which is what
this plan is about.

**Licences.** yabai, Amethyst, Silica, AeroSpace are all MIT and compatible with
our ISC as long as their notices are kept; copying an idea — which call, in
which order — creates no obligation. Two things must stay out of the tree:
**AltTab is GPL-3.0** (read it for facts, take no code — `tools/check-licensing.py`
would fail the build), and the `CGSInternal` headers have no licence file at
all, so the private symbols we need are declared ourselves, as AeroSpace does.

## What is checked without a Mac, and what never is

**Checked here, today, and it is not a small list:**

- the arithmetic contains no X11 — `sh tools/no-x-build.sh`, which fails and
  names the line if an Xlib call reaches the policies;
- the policies and `layout-probe` agree on **2871 vectors** (171 conformance,
  2700 random), not one mismatch — so the layout the Mac will get is the layout
  proven here;
- `layout-probe` builds with no window server anywhere, from `probe.c`'s 1076
  lines;
- the AX code agrees with **our idea of** the Accessibility API — `sh
  tools/macos-flicker/stub-build.sh` builds `axcost.c` with `-Wall -Wextra
  -Werror` against a stub rewritten from the SDK headers, and catches a
  misspelt name, a swapped argument, a callback with the wrong signature.

**Never checked without a Mac, and no amount of care changes that:**

- whether the stub is *right*. It is our idea of someone else's API, and the
  liar in that pair can be the stub rather than the code;
- that `flicker.m` compiles at all — it has **never been built**, because honest
  stubs for AppKit are not worth making;
- the flicker, in milliseconds, and therefore whether the port is pleasant;
- the cost of one AX round trip for any real application;
- whether a geometry write silently fails — Silica sets the size twice, before
  and after the position, with the comment that the accessibility APIs are
  finicky and that this *still occasionally silently fails*;
- anything about the permission dialogue, the TCC grant surviving a rebuild, or
  the brew formula on a clean machine.

## What is still the owner's to decide

1. **The three threshold numbers**, after stage 1 and before stage 5.
2. **Marking the active window** — a window of our own on top of everything, or
   nothing at all. The border cannot be drawn.
3. **Groups on macOS** — dropped outright, or the subset that works with SIP on
   (move into an existing Space, switch to it).
4. **Putting the pointer back** — which call, with a source; it is the one
   operation of the ten with no counterpart cited.

Nothing here claims that a Mac port has been started in code. The plan exists so
that the first line, when someone with a Mac writes it, is written against a
contract, in an order, and with a number that says when to stop.
