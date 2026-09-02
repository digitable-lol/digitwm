# digitwm on macOS: putting it on, starting it, and what will go wrong

**Русская версия: [macos-install.ru.md](macos-install.ru.md).**

[macos.md](macos.md) is the plan of the port — the stages, the losses, the
numbers at which it is closed. This is the other half: what a person with a Mac
types, what he sees, which permission he is asked for, and — the part most
documents leave out — what is most likely to fail on the first run and how to
find out which Apple call did it.

**Nobody on this project has a Mac** (`portability.md:13`). Everything below
that describes a Mac is a description of what the code asks the Mac to do, not
a report of it happening. The line "What is checked here, and what is checked
nowhere" says exactly where the two part company, in numbers.

## What you type

One of three, whichever suits.

**From a clone, with the rest of the environment:**

```sh
git clone https://github.com/digitable-lol/digitwm
cd digitwm
sh bootstrap.sh
```

On a Mac, `bootstrap.sh` builds the macOS target rather than the X11 one and
installs `digitwm` into `~/.local/bin`. It needs Xcode Command Line Tools and
nothing else — no Homebrew packages, no yacc, no pkg-config, no X11 headers.
If the tools are missing it says so and stops: `xcode-select --install`.

**Only the window manager, from a clone:**

```sh
make -C macos
sudo make -C macos install          # /usr/local/bin/digitwm
```

**From brew:**

```sh
brew tap digitable-lol/digitwm https://github.com/digitable-lol/digitwm
brew install --HEAD digitable-lol/digitwm/digitwm
```

`--HEAD` because there is no released version carrying the macOS target yet.
The formula is `macos/digitwm.rb` and it has never been run either.

## What you see the first time

```
$ digitwm
```

1. macOS raises its own dialogue: *"digitwm would like to control this
   computer using accessibility features."* Nothing has happened to any window
   yet. Apple's own note on this call is that the prompt "occurs
   asynchronously and does not affect the return value" — which is why the
   next thing you see is:
2. digitwm says it has not been granted anything, tells you where the switch
   is, and exits. This is not a failure to recover from; the grant is read
   once, at start-up.
3. You turn digitwm on in **System Settings → Privacy & Security →
   Accessibility** and start it again.
4. `digitwm: 31 window(s) on the ribbon.` — and every window that was open
   moves into a column. Then the process sits there and does nothing visible
   until you press a key or open a window.

**Nothing appears in the Dock and there is no menu bar item.** digitwm has no
window of its own: it arranges other people's. To stop it, press
Control-Option-Q, or Control-C in the terminal it was started from.

**Start it from a terminal in a logged-in graphical session.** Not over ssh
and not from a launchd daemon: a process with no connection to the window
server cannot take a key combination, and digitwm says so in as many words
rather than starting deaf.

## Two permissions, and why you are only asked for one

Apple splits keyboard-and-window privileges in two, and the split is not the
obvious one. From the WWDC session that introduced it: *"where a listen-only
event requires authorization for input monitoring, a modifying event app
requires authorization for accessibility features."*

| What a program does | What macOS asks the user for |
|---|---|
| read and write other applications' windows through the Accessibility API | **Accessibility** |
| watch every key with a `CGEventTap`, even without changing anything | **Input Monitoring** — a different switch, a different dialogue |
| take one key combination with `RegisterEventHotKey` | nothing at all |

digitwm needs the first because that is what moving somebody else's window
*is*. It avoids the second by taking its keys with `RegisterEventHotKey`
rather than with an event tap — see the head of `macos/wsi_key.m` for the
argument in full. So: **one permission, asked once.**

The price of that choice is written down rather than hidden:

- `RegisterEventHotKey` belongs to Carbon, and Apple's own engineers say they
  cannot honestly recommend it — "intimately tied to the legacy Carbon
  toolbox". There is no modern replacement that swallows a key without a
  second privacy grant.
- **macOS Sequoia refuses a hot key whose only modifiers are Shift and
  Option.** The default table below uses Control-Option throughout and is
  unaffected; a `digitwmrc` line like `bind-key M-h ...` is not, and digitwm warns
  at that line rather than letting the key be refused in silence.

There is a third permission digitwm deliberately never asks for: **Screen
Recording**. It does not read window contents, only rectangles.

## The keys

```
$ digitwm -k
```

prints the live table. The defaults:

| Keys | What happens |
|---|---|
| ⌃⌥ H / L | focus the column to the left / right |
| ⌃⌥ K / J | focus the window above / below in the column |
| ⌃⌥⇧ H / L | carry this window to the neighbouring column |
| ⌃⌥⇧ K / J | carry this window up / down its own column |
| ⌃⌥⌘ H / L | exchange this whole column with its neighbour |
| ⌃⌥ R | cycle the column's width through the four presets |
| ⌃⌥ = / − | widen / narrow the column |
| ⌃⌥ C | centre the ribbon on the focused column |
| ⌃⌥ F | float this window off the ribbon, and back |
| ⌃⌥ Q | stop digitwm |

**These are not the X11 defaults, and that is deliberate.** `conf.c:280` binds
the ribbon to Mod4 "because upstream cwm leaves it entirely free". On a Mac
Mod4 is Command, which is the opposite of free — and a hot key registration
takes the combination away from *every* application at once. A port that kept
the X11 table would take Command-H, Command-J, Command-K, Command-L,
Command-C, Command-F, Command-R, Command-minus and Command-equal away from the
whole desk on its first run. Control-Option is what macOS itself leaves alone.

You can have the X11 keys back by writing them in
`~/.digitable/digitwm/digitwmrc`, where the modifier letters are the ones
`cwmrc(5)` already uses:

| Letter | X11 | macOS |
|---|---|---|
| `S` | Shift | ⇧ Shift |
| `C` | Control | ⌃ Control |
| `M` | Mod1 (Alt) | ⌥ Option |
| `4` | Mod4 | ⌘ Command |
| `5` | Mod5 | — nothing; a Mac has four |

A binding on Command alone gets one line of warning saying what it will do to
the rest of the machine. It is still obeyed.

## Which file is read, and what of it

digitwm reads **the same file the X11 build reads**, found by the same search,
so one file describes both machines. The order, strongest first:

| | file | |
|---|---|---|
| 1 | the file named with `-c` | nothing overrides it |
| 2 | `$DIGITWMRC` | when set and not empty |
| 3 | `~/.digitable/digitwm/digitwmrc` | our own name, in the directory this family of tools keeps its settings in |
| 4 | `~/.cwmrc` | cwm's name, read so that a configuration written for cwm keeps working — and said out loud, once, on stderr, when it is the file being read |

It is one piece of code, `confpath.c`, compiled into both binaries rather than
written twice, and `tools/check-config-order.sh` asks both of them the same
seven questions on every push — a Mac that quietly read a different file than
the workstation would make "one file describes both machines" a sentence and
nothing more.

What it cannot do is read all of that file. `cwmrc(5)` has **34 directives**;
digitwm acts on **9** and names the other **25** out loud, with the reason, at
the line they stand on:

```
$ digitwm -n
/Users/you/.digitable/digitwm/digitwmrc: 12 directive(s): 6 taken, 5 X11's alone, 1 not understood
```

**Taken (9):** `ribbon`, `ribbonhide`, `ribbonwarp`, `ribbongap`,
`ribbonminwidth`, `ribbonminheight`, `ribbonwidths`, `bind-key`, `unbind-key`.

**X11's alone (25),** grouped by the reason, all four of which
[macos.md](macos.md) settled before any of this was written:

- **there is no border to draw on another application's window** —
  `borderwidth`, `activeborder`, `inactiveborder`, `urgencyborder`,
  `groupborder`, `ungroupborder`, `color`;
- **there is no menu** — `font`, `fontname`, `selfont`, `menubg`, `menufg`;
- **there are no groups**, because Spaces have no public API — `sticky`,
  `autogroup`;
- **there is no pointer grab and no EWMH** — `bind-mouse`, `unbind-mouse`,
  `snapdist`, `moveamount`, `htile`, `vtile`, `gap` (`NSScreen.visibleFrame`
  has the Dock and the menu bar off already, so there are no struts to
  subtract), `command`, `wm`;
- **the port does not read a window's class**, so it cannot match one —
  `ignore`, `ribbonrule`.

That last pair is the only entry in the list that is a gap rather than a loss.
`ribbonrule` puts a named application into a column of your choosing; on macOS
it would need one more call across the boundary — asking a window for its
title or its application — and `macos/wsi_platform.h` is deliberately eleven
calls wide. It is named here rather than made to look like it works.

`ribbonwarp yes` is taken and then does nothing, and says so: this port cannot
move the pointer at all. `wsi.h` allows exactly that, on condition that focus
does not follow the pointer either, and this port takes the pair.

Of the **124 commands** `bind-key` can name on X11 (`conf.c`, `name_to_func`),
**16** exist here: the fifteen ribbon commands and `quit`. The rest ask for a
border, a group, a menu, a pointer warp or an EWMH property. `digitwm -k`
prints the sixteen.

## When it does not work, find out which name failed

```
$ digitwm -N
```

goes down every Apple call the port makes, in the order it reaches them at run
time, and prints which answered:

```
  [1] AXIsProcessTrustedWithOptions               answered  Accessibility granted
  [2] +[NSScreen screens], -frame, -visibleFrame  answered  2 display(s), first "Color LCD" 1512x982, workable 1512x944
  [2] NSWorkspace, NSRunningApplication           answered  31 window(s) taken onto the ribbon
  [2] +[NSEvent mouseLocation]                    answered  pointer at 812,443
  [1] AXUIElementCopyAttributeValue               answered  first window at 0,38 1512x944
  [2] AXUIElementIsAttributeSettable, Set...      answered  geometry accepted
  [2] AXUIElementPerformAction, kAXRaiseAction    answered  raise accepted
  [2] kAXMainAttribute, kAXFrontmostAttribute     FAILED    activation was refused
  [2] RegisterEventHotKey                         answered  CM-h taken
```

A `FAILED` line names the call, not the symptom. That is the thing to report,
and the rest of this section says what each of them costs.

**What `-N` cannot catch, and what catches it instead.** A misspelt symbol or
a wrong argument list never gets as far as running: it fails when the file
first meets Apple's headers, which is a compiler error with a line number,
before anything happens to anybody's windows. That is the cheapest failure
there is, and it is the one this port is built to fail with — there is not a
single numeric Apple constant anywhere in `macos/wsi_ax.m` or
`macos/wsi_key.m`, only symbols, so the quiet kind of mistake is not available
to them.

### The three things most likely to go wrong first

**1. `macos/wsi_key.m` does not compile.** Apple has retired the Carbon Event
Manager reference: the documentation page is gone, and the archived
programming guide says nothing about hot keys. The one Apple page that still
lists these symbols prints them in their *Swift* projection, so the parameter
types and their order are Apple's and **the C declarations are not published
anywhere Apple still hosts**. The stub in `macos/stub-build.sh` is written to
those types; if the real header disagrees, the compiler says so at the line.

**2. The hot keys register and never arrive.** digitwm is a plain executable
with no application bundle. **Apple documents nothing** about whether a
bundle-less command-line tool receives Carbon hot key events. What Apple does
say, about the neighbouring AppKit case, is discouraging — an engineer writes
that AppKit's event handling "may not work because it may rely on the AppKit
machinery to be running", and that a bundle-less tool wanting AppKit events
needs app-like packaging and `NSApplicationMain`. That is AppKit and this is
Carbon, and the port refuses to extrapolate silently, so it is named here.

*The symptom:* `digitwm -N` says `RegisterEventHotKey answered`, the windows
line up correctly, and no key does anything. *The remedy, in order of cost:*
call `-[NSApplication run]` instead of turning the run loop by hand; failing
that, ship digitwm inside a `.app` bundle with `LSUIElement` and start AppKit
properly. Both are changes to `macos/wsi_key.m` and `macos/wsi_run.c` and
neither touches the ribbon.

**3. Activation is refused.** `kAXFrontmostAttribute` and `kAXMainAttribute`
exist and Apple says what they mean; Apple does *not* say what value type they
take, and this port writes `kCFBooleanTrue` on the strength of AppKit's
parallel constants being documented as `NSNumber`. Different symbols, so it is
a guess. *The symptom:* windows move into their columns but the keyboard stays
where it was. *The remedy is already named in [macos.md](macos.md)*: the
private `_SLPSSetFrontProcessWithOptions`, which this tree refuses to declare
out of thin air and will declare with a citation the day it is needed.

### What each failure costs

| The call that failed | What stops working | What still works |
|---|---|---|
| `AXIsProcessTrustedWithOptions` | everything | nothing — this is the permission |
| `NSWorkspace` / `NSRunningApplication` | no windows are found at all | nothing |
| `NSScreen screens` / `frame` | no ribbon: there is nowhere to lay one out | nothing |
| `NSScreen localizedName` | two identical monitors become one name and one ribbon | one monitor |
| `AXUIElementCopyAttributeValue` | the port cannot read a window's rectangle | nothing |
| `AXUIElementIsAttributeSettable` / `SetAttributeValue` | windows are never moved | the layout is computed and never applied |
| `AXUIElementPerformAction` (raise) | overlapping windows stay in the wrong order | columns, which do not overlap |
| `kAXMainAttribute` / `kAXFrontmostAttribute` | the keyboard does not follow the focus | the layout, entirely |
| `RegisterEventHotKey` | no commands | the layout of windows as they open and close |
| `AXObserverRemoveNotification` | a window costs a little work after it leaves | everything |
| `CFRunLoopRemoveSource` | a quit application's observer leaks | everything |

## What is checked here, and what is checked nowhere

Three commands, on a machine with no macOS on it, and their numbers:

```
$ sh macos/check.sh 400
```

- the ribbon over the macOS port against the X11 binary: **400 cases, 2680
  windows, 0 divergences**; of those, **1213 windows** compared a second time
  against what the window system actually holds, **0 divergences**;
- the reverse mechanism, one scenario run twice: with tagging **40 writes, 40
  notices, 40 recognised as ours, 0 taken as foreign, 5 rounds to quiet**;
  without it **9968 writes, 9967 taken as foreign, 1200 rounds and never
  quiet**;
- parking with `ribbonhide`: **2 windows inside the viewport, 18 past the
  edge, 1 pixel of each still visible**;
- the start-up sequence, the configuration and the key dispatch, over the
  window system of memory: a sample configuration of **12 directives → 6 taken, 5
  X11's alone, 1 not understood**; **6 windows into 6 columns, 0 adrift**;
  **16 bindings, 1 refused and named**; a key moves the focus, a key widens a
  column, a key carries a window; a foreign move comes back; a second monitor
  appears and a second ribbon appears with it;
- and the whole binary is **linked**, with the real `main()` and every object
  file `macos/Makefile` builds, with exactly two replaced: `wsi_ax.m` and
  `wsi_key.m`, for which there is a window system of memory and a fake
  keyboard. That is the most of "does it build on a Mac" that can be answered
  without one: the object graph is complete and closes.

```
$ sh macos/stub-build.sh
```

compiles `wsi_ax.m` and `wsi_key.m` against stubs of ApplicationServices,
AppKit and Carbon with `-Wall -Wextra -Werror`. The check is falsifiable and
was falsified on purpose: `kVK_ANSI_H` misspelt as `kVK_ANSI_HH` fails at
`macos/wsi_key.m:122`.

**Never checked here, and no amount of care changes it:** that the stubs are
right; that either `.m` file compiles against Apple's own headers; that
`macos/Makefile` runs; that `-sectcreate __TEXT __info_plist` is the flag
Apple's linker takes (Apple documents the *section* and the Xcode build
setting that makes it, and no longer hosts a manual page for the flag); that a
hot key ever arrives; the flicker, in milliseconds; the cost of one round trip
into a real application; whether a geometry write silently fails.

## Delivery, and the trap in it

**The Mac App Store is closed to this program, by Apple's own rule.** Apple
lists "Use of accessibility APIs in assistive apps" among the activities
forbidden inside the App Sandbox, and the store requires the sandbox. So
digitwm is a plain executable with its `Info.plist` welded into a
`__TEXT,__info_plist` section — the shape Apple documents for single-file
tools — and brew is the way it travels.

**The trap: the permission is remembered against the signature.** `make -C
macos` signs ad-hoc (`codesign --sign -`), and an ad-hoc signature is a hash
of the binary — so every rebuild is a different program as far as macOS is
concerned, and the Accessibility dialogue comes back. Rebuilding twice a day
means answering it twice a day.

The way out is a stable signing identity of your own. Create a self-signed
code-signing certificate in Keychain Access (Certificate Assistant → Create a
Certificate, type *Code Signing*), then:

```sh
make -C macos SIGNID="Your Certificate Name"
```

and the grant survives rebuilds. This is standard practice for exactly this
problem and it is not something this project has ever run.

## What you will not have

Six of these are argued in [macos.md](macos.md), by measurement or by a cited
absence, and none of them is a temporary state of the work:

1. **a window will jump.** It appears where its application put it and moves
   into its column afterwards — macOS has no equivalent of X11's `MapRequest`,
   and `AXObserverCallback` returns `void`;
2. **`ribbonhide` leaves a 1-pixel line.** macOS will not put a window
   entirely off the screen;
3. **no groups.** Spaces have no public API;
4. **a full-screen window is not on the ribbon.** It lives in its own Space,
   and the insertion policy already allowed for that;
5. **native tabs break.** Terminal.app with tabs is one window to the
   Accessibility API and several to you;
6. **no border on the active window.** You cannot draw on another
   application's frame. Marking the focused window some other way is the
   owner's separate decision and not part of this port.

And two that this document adds:

7. **the pointer is never carried to the focus** — no call for it exists in
   the public API, and no focus follows the pointer either. One decision, both
   halves;
8. **a window of an application that was not running a second ago joins the
   ribbon up to one second late.** The port asks who is running on a clock
   rather than subscribing, and the head of `macos/wsi_ax.m` says why, what it
   costs, and which two documented notification names replace it.
