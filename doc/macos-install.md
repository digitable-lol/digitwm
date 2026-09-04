# digitwm on macOS: putting it on, starting it, and what will go wrong

**Русская версия: [macos-install.ru.md](macos-install.ru.md).**

[macos.md](macos.md) is the plan of the port — the stages, the losses, the
numbers at which it is closed. This is the other half: what a person with a Mac
types, what he sees, which permission he is asked for, and — the part most
documents leave out — what is most likely to fail on the first run and how to
find out which Apple call did it.

**On 2 September 2026 digitwm was built and run on a real Mac for the first
time.** Until that day this paragraph said "nobody on this project has a Mac",
and that had been true for half a year. What is known now, and from where -
because the sources are different and worth different amounts:

| Where | What passed there |
|---|---|
| **The owner's Mac**, 2 September 2026: Apple Silicon, `/opt/homebrew`, **3 displays**, build `HEAD-bfd5d85` via `brew install --HEAD` | Build, start, the Accessibility grant, and `digitwm -N`: **all 12 Apple calls answered, 0 refused** - writing geometry, raising a window and taking `CM-h` included. **10 windows** taken onto the ribbon. |
| **GitHub's macOS runners**, both architectures, on every push | The build from `macos/Makefile`: **0 compiler errors, 0 warnings** on both. Signing, `-k`, `-n`, `-N`, the `.app` bundle, the reproducible release build. |
| **Linux**, on every push | Everything that needs no Mac: `macos/check.sh` (the ribbon over the port against the ribbon over X11) and `macos/stub-build.sh` (the two Objective-C files against our idea of the API). |

**What nobody has checked, and it should be held in mind while reading the
rest.** `digitwm -N` writes a window back exactly where it already was, so a
real layout under load - ten windows spreading into columns, a person pressing
keys, applications opening and closing windows - has **never once run** on a
Mac. And one live failure is known: **hot keys register and do not act** - `-N`
says "`RegisterEventHotKey answered CM-h taken`" and Control-Option-H does
nothing. The `-v` flag is what takes that apart; see "When it does not work".

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

This route - `brew install --HEAD` - is how the macOS target was first built on
a real Mac, on 2 September 2026: `digitwm/HEAD-bfd5d85` went into the Cellar and
a symbolic link `/opt/homebrew/bin/digitwm` onto `PATH`. The difference between
the link and the real file is not cosmetic: the Accessibility grant is
remembered against the real file, and the first hour on that Mac went on
exactly this. See "When it does not work".

### The flags, and the page that describes them

`man digitwm` — the page is `digitwm.1` in the tree, and brew installs it
beside `cwmrc(5)`, so both `man digitwm` and `man 5 cwmrc` answer on a machine
that took digitwm from brew. `make -C macos install` installs the binary alone;
from a clone the same page is `man ./digitwm.1`.

| Flag | What it does |
|---|---|
| `-c file` | read this file instead of the one the search would find. A file named here that does not exist is an error, not a reason to look elsewhere: somebody meant that file |
| `-h` | print the usage and exit |
| `-k` | print the key table and the command each combination runs, then exit. Needs no permission |
| `-N` | go down the Apple calls this port makes, one at a time, and say which answered. Before the first call it prints three facts about the process itself — which file is running, who started it, what its signature is — and those need no permission either. Exits non-zero when anything did not answer |
| `-n` | read the configuration, say what was made of it, and exit. Needs no permission |
| `-v` | say out loud, on standard error, what happens: every key press that arrives, which binding it matched, which command ran, which window held the keyboard before and after, and the whole state of the ribbon. Every fiftieth turn of the loop it prints a pulse — how many turns and how many notices — so that the silence of "the press never arrived" reads differently from the silence of "the process is wedged". `DIGITWM_TRACE` set to anything does the same for a digitwm started by `launchd` or from the Finder, where there is nowhere to put a flag |

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

**On the first run that finds neither of the last two, digitwm writes
`~/.digitable/digitwm/digitwmrc` itself** — the directory 0755, the file 0644 —
holding every setting there is, commented out, with a line of prose above each.
A file of comments changes no behaviour; it is there so that a person who opens
it sees what there is to turn. It is written once and never rewritten: what is
put in it survives every upgrade. When `~/.cwmrc` exists and ours does not,
nothing is written and the reason is printed — ours would win the search and
hide a file that is not empty, which is exactly the silent replacement a
settings file must never make. The message names the two commands that move the
old file over.

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
time, and prints which answered. **What follows is not an example but the real
output**, from the owner's Mac, 2 September 2026, Apple Silicon, three
displays:

```
  [1] AXIsProcessTrustedWithOptions                  answered Accessibility granted
  [2] +[NSScreen screens], -frame, -visibleFrame     answered 3 display(s), first "P27FBA-RAGL" 1920x1080, workable 1920x1049
  [2] -[NSScreen localizedName]                      answered P27FBA-RAGL
  [2] NSWorkspace, NSRunningApplication              answered 10 window(s) taken onto the ribbon
  [2] +[NSEvent mouseLocation]                       answered pointer at 374,661
  [1] AXUIElementCopyAttributeValue (position, size) answered first window at -1996,31 956x1049
  [2] AXUIElementIsAttributeSettable, SetAttributeValue answered geometry accepted
  [2] AXUIElementPerformAction, kAXRaiseAction       answered raise accepted
  [2] kAXMainAttribute, kAXFrontmostAttribute        answered activation accepted
  [-] wsip_pointer_warp                              answered refuses, as designed: no call for it exists
  [2] InstallEventHandler, GetApplicationEventTarget answered handler installed
  [2] RegisterEventHotKey                            answered CM-h taken

0 call(s) did not answer.
```

Twelve of twelve. Note the `-1996`: that is a window on the monitor to the
**left** of the main one. Negative coordinates are ordinary for this port and
are handled in signed arithmetic. One scenario does test one — «монитор слева
от главного» in `fts/flang/layout.flang`, whose viewport starts at `-1996` —
and everything else in the tree (`macos/runcheck.c`, the two harnesses) still
stands at a non-negative origin.

For comparison, the same `-N` on a GitHub macOS runner, where there is neither
a person nor a real window: the grant is given there, but **two calls of the
twelve refuse** — `SetAttributeValue` ("the write was refused") and
`kAXRaiseAction` ("raising was refused"). The window it does find there can be
read and not written. That is not the port breaking; that is how a runner's
virtual display differs from a desk, and it is worth knowing while reading any
report off a runner.

A `FAILED` line names the call, not the symptom. That is the thing to report,
and the rest of this section says what each of them costs.

### The grant is given and it is still not there

The first hour on the real Mac went on exactly this: the grant is given to
digitwm, and digitwm says it has none. There are three causes, and `-N` now
names them **itself, before the first call** — these lines need no permission:

```
This process, before any of that:
  running   /opt/homebrew/Cellar/digitwm/HEAD-bfd5d85/bin/digitwm
  launched  /opt/homebrew/bin/digitwm
            NOT THE SAME FILE. ...
  started by /Applications/Ghostty.app/Contents/MacOS/ghostty (pid 4711)
            If that is a terminal, the permission macOS looks at is ITS ...
  signature ad-hoc (a hash of this very file)
            cdhash 0ab83fd244a251e850d1c29ea6ae2d363ebe8478
```

**1. Started from a terminal.** macOS attributes the grant not to the program
but to what Apple calls *responsible code*. An Apple DTS engineer lists the
cases outright: "Run by the user from Terminal — **The tool's responsible code
is Terminal**"; "Run by `launchd` as a daemon or agent — If the daemon or agent
was installed by `SMAppService`, that makes the app the responsible code.
Otherwise, the daemon or agent should include `AssociatedBundleIdentifiers` in
its `launchd` property list"
([developer.apple.com/forums/thread/756510](https://developer.apple.com/forums/thread/756510)).
In the same place Apple qualifies it: "The exact algorithm it uses is not
documented, has changed in the past, and may well change in the future". Which
means: **give the grant to the terminal** (Terminal, iTerm, Ghostty) and quit
the terminal whole — Command-Q, not the tab. Apple never writes the sentence
"grant it to the terminal"; it follows from the rule quoted above, and other
tools of the same kind repeat it.

**2. brew puts a link on `PATH`.** `/opt/homebrew/bin/digitwm` is a symbolic
link to a file in the Cellar. The grant is remembered against the real file.
`-N` prints both paths and says out loud when they have parted.

**3. Rebuilt after the grant was given.** The system remembers the grant
against the signature. Apple, TN3127: "**Ad hoc signed code**, called Sign to
Run Locally by Xcode, **has a DR but it's tied to that specific version of the
code** ... If you tweak the code and run it again, macOS repeats that prompt"
([TN3127](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements)).
The cure is a stable identity of your own: `make -C macos SIGNID="your
certificate name"`. The `.app` bundle does **not** cure this one: in the same
note Apple explains that the bundle identifier is only one term inside the
requirement, and the requirement for ad-hoc code is `cdhash H"…"`.

**What the bundle fixes, and what it does not.** It fixes the first: an
application started from `/Applications` is its own responsible code. Apple:
"If you nest your tool, or your background-only app, within a standard app then
the system will **likely** consider that app to be responsible for your
process" ([thread/680491](https://developer.apple.com/forums/thread/680491);
"likely" is Apple's word, not ours). The third it does not fix at all.

**About restarting.** Apple's `AXUIElement.h` header says exactly one thing
about the prompt: "Prompting occurs asynchronously and does not affect the
return value". That the state of the permission is cached, and that the program
has to be restarted once the grant is given, **Apple writes nowhere**; it is
the converging experience of other managers of this kind — yabai: "The
application must be restarted after access has been granted", skhd: the same
thing word for word. We repeat it as experience, not as documented behaviour.

**And one trap that belongs to the system version.** The same Apple engineer,
January 2026: "The inability to add a standalone executable to System Settings >
Privacy & Security is a known regression in macOS 26.1. It's fixed in our 26.3
beta seeds"
([thread/813989](https://developer.apple.com/forums/thread/813989)). On 26.1
and 26.2 it may be impossible to add a bare executable to the list at all —
which is one more argument for giving the grant to the terminal, or for
installing the bundle.

### The key registers and does not act

The live failure found on the owner's Mac: `-N` says `RegisterEventHotKey
answered CM-h taken` and `InstallEventHandler ... answered handler installed`,
and Control-Option-H does nothing.

Between the press and the ribbon moving there are four places where a press
disappears, and from outside all four look the same. The flag that takes them
apart:

```
$ digitwm -v 2>/tmp/digitwm.log
```

What to look for in the output, in order:

1. `carbon: handler entered` — **there or not there**. Not there: the event
   never reached the process, and the ribbon has nothing to do with it; what to
   read then is `macos/wsi_key.m`, and the fact that a process with no `.app`
   bundle receives no Carbon events. The neighbouring lines print what this
   process turned out to be to the system: its activation policy and its bundle
   identifier.
2. `key: id N = C-M-h -> ribbon-focus-left ARRIVED in this process` — the event
   was recognised and turned into a command.
3. `key: before` and `key: after`, with the number of the window that holds the
   keyboard — if the number did not change, the command ran for nothing.
4. The state dump: every display with its origin and size, every ribbon with
   its column count and its `attached`/`DETACHED` mark, every window with its
   coordinates and the ribbon it landed on. This is where it shows if, with
   three displays, the windows attached to the wrong screen — the focus then
   travels along a ribbon nobody is looking at.

`DIGITWM_TRACE=1` does the same for a digitwm brought up by `launchd` or from
the Finder, where there is nowhere to put a flag.

**What `-N` cannot catch, and what catches it instead.** A misspelt symbol or a
wrong argument list never lives to be run: they fail at the file's first
meeting with Apple's own headers — a compiler error with a line number, before
anything happens to anybody's windows. That first meeting took place on
2 September 2026 on GitHub's macOS runners and gave **0 errors and 0 warnings**
on both architectures.

### Three predictions, and what came of them

While there was no Mac, three things stood here as the ones that would fail
first. Two of the three did not fail; one did, and not the one that was backed.
The predictions are kept beside their verdicts: a document that erases its own
misses is a document there is nothing to check.

| Predicted | What came of it | Where it was measured |
|---|---|---|
| **`macos/wsi_key.m` will not compile** — Apple has retired the Carbon Event Manager reference, publishes no C declarations, and the stub was written to the Swift projection | **It compiled.** Both Objective-C files, `wsi_ax.m` and `wsi_key.m` — **0 errors, 0 warnings** against Apple's own headers, first time, on both architectures | GitHub's macOS runners, 2 September 2026 |
| **The keys register and never arrive** — a program with no `.app` bundle, and Apple writes nothing about Carbon event delivery to such a program | **This is exactly what failed.** The registration goes through (`RegisterEventHotKey answered CM-h taken`), the press does nothing. The cause has no number to it yet — the `-v` flag was made for that | The owner's Mac, 2 September 2026 |
| **Activation is refused** — Apple does not document the value type `kAXMainAttribute`/`kAXFrontmostAttribute` take, and `kCFBooleanTrue` was a guess | **Not refused.** `activation accepted`. The guess was right | The owner's Mac, 2 September 2026 |

An Apple quote about the second has turned up since, and it is not in our
favour: a DTS engineer on `RegisterEventHotKey` — "However, it's **intimately
tied to the legacy Carbon toolbox** and thus I can't honestly recommend it",
advising `CGEventTap` with `CGPreflightListenEventAccess` instead
([thread/735223](https://developer.apple.com/forums/thread/735223)). In the same
place he describes the shape an application with hot keys wants — a nested app
with `LSUIElement`. The price of moving to `CGEventTap` is named earlier in this
document: a second permission, Input Monitoring, a second dialogue and a second
row in System Settings. The decision is the owner's, and it is to be taken
after `-v` has said where the press is lost, not instead of that.

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

### The same, on a Mac — and now the numbers are real

Five entries of the old "checked nowhere" list are closed, three are still
open, and one has been added. In order, with the date and the machine.

**Closed on GitHub's macOS runners** (both architectures, on every push, from
2 September 2026):

- **both Objective-C files compile against Apple's own headers** — `0` errors,
  `0` warnings, on `macos-latest` (Apple Silicon, SDK 26.5, clang 21) and on
  `macos-15-intel` (x86-64, SDK 15.5, clang 17);
- **`macos/Makefile` runs** — the target goes through whole, signing included;
- **`-sectcreate __TEXT __info_plist` is the flag** — the section is there, and
  `codesign` reads `Info.plist entries=6` out of it. That was the one line of
  the file not written from Apple's documentation;
- **the release build is reproducible** — the target is built twice, into
  different directories, and the digests compared: they matched on both
  architectures;
- **`macos/check.sh` and `macos/stub-build.sh` pass on a Mac too**, not only on
  Linux. Both broke the first time, and both breakages were real: `check.sh`
  did not link `reallocarray.o` (on Linux glibc gave the function, on a Mac
  nothing does), and `stub-build.sh` took a libc level other than the real
  build's.

**Closed on the owner's Mac** (Apple Silicon, three displays, `HEAD-bfd5d85`,
2 September 2026): `digitwm -N` — **12 calls of 12 answered, 0 refused**, the
geometry write (`geometry accepted`) and the raise (`raise accepted`) among
them. **3 displays** and **10 windows** found; the first window at `-1996,31`.

**Still checked nowhere:**

- **a real layout under load.** `-N` puts a window back exactly where it was;
  ten windows spreading into columns on a live desk have been watched by
  nobody;
- **the flicker, in milliseconds, and the cost of one round trip** into a live
  application — the meter `tools/macos-flicker/` has never been run on a Mac;
- **negative coordinates in the checks.** A window at `-1996` works, and one
  layout scenario does start there, but no MONITOR in the tree does:
  `layout-probe outputs` parses only `WxH+X+Y` (`probe.c:816`, two mandatory
  `+`), so every display in `macos/runcheck.c`, `macos/wsicheck.c` and in the
  hotplug harness stands to the right and below. That is a hole in the checks,
  not in the code.

**And one thing that is now known as a failure rather than as an unknown:** hot
keys register and do not act. The `-v` flag takes it apart; see above.

## Delivery, and the trap in it

**The Mac App Store is closed to this program, by Apple's own rule.** Apple
lists "Use of accessibility APIs in assistive apps" among the activities
forbidden inside the App Sandbox, and the store requires the sandbox. So
digitwm is a plain executable with its `Info.plist` welded into a
`__TEXT,__info_plist` section — the shape Apple documents for single-file
tools — and brew is the way it travels.

**The trap: the grant is remembered against the signature.** `make -C macos`
signs ad-hoc (`codesign --sign -`), and an ad-hoc signature is a hash of the
binary — so every rebuild for macOS is already a different program, and the
dialogue comes back. Rebuilding twice a day means answering it twice a day.
Apple says so outright (TN3127, quoted above). A released file has one
signature for the whole version, and `brew reinstall` of that same version does
not lose the grant; moving to the next version does, and there is no other way
for it to be.

**The second trap, found by the build itself:** `codesign`, meeting an
`Info.plist` with `CFBundleExecutable` in the directory, signs the
**directory** and not the file — the very first build on a runner gave
`Format=app bundle` and a seal over 42 files of `macos/`. A file carried out of
such a directory — and the formula does carry it out — is left holding a
signature with nothing to rest on. So `macos/Makefile` links and signs in
`${OBJDIR}`, where there is no `Info.plist`, and fails if the signature came
out a bundle's after all.

**And a third, about the bundle:** `digitwm` and
`digitwm.app/Contents/MacOS/digitwm` are one and the same program with
**different `cdhash`es**: signing the bundle, `codesign` re-signs the file
lying inside it. So the grant given to the file and the grant given to the
bundle are two different grants, and neither counts as the other.

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
