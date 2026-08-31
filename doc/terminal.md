# digitwm in a terminal: route A

**Русская версия: [terminal.ru.md](terminal.ru.md).**

The owner stated the task like this (translated from the Russian):

> port it so that it works in a console over ssh, tmux-style, with rendering …
> for applications like a browser and other desktop things, but otherwise so
> that it lets you work in a terminal too

This is the specification of the route that was chosen, not a report on work
done: **not one line of multiplexer code exists**, and the document exists so
that what is being built, out of what, what will not be in it and how it is
checked are written down before that first line.

**What is measured here and what is read elsewhere.** Everything about digitwm
below was measured on this machine, with the command printed next to the number.
Everything about tmux, dvtm, mtm, zellij, tuios, kitty and terminal protocols
comes from the research that preceded the choice of route and was **not
re-checked here** — this run has no network and none of those trees are on the
machine. Where a source is someone else's, the line says so.

## Route A, and the two others

| Route | What it is | Why not it |
|---|---|---|
| **A** | our own multiplexer, with the layout driven by this repository's `ribbon.c` — the same file the X11 build uses | chosen |
| B | the ribbon inside tmux | tmux's `layout_check()` rejects a layout with gaps, overlap or mismatched sums, and `layout_parse` resizes the window to fit the layout. A ribbon **wider than the viewport** cannot be spelled in that grammar at all — and that is what a ribbon is (from the research) |
| C | take someone else's multiplexer that already has a ribbon (tuios) | answered in its own section, [«Why write our own when tuios exists»](#why-write-our-own-when-tuios-exists) — which also says under which answer we should not write it |

Route A rests on one claim, and the claim is measured rather than asserted:
**the ribbon's arithmetic knows nothing about X11 or about terminals, and it
already counts in units a terminal can use.**

## The ribbon already counts in cells

This is the key command of the specification. It opens no display, draws
nothing, and prints finished pane rectangles:

```sh
./cwm -C "layout-probe layout viewport=200x50 columns=1,4,1,2 gap=1 min-width=20 min-height=3 border=0"
```

```
ok layout
stage initial
viewport 0 0 200 50
gap 1
border 0
ribbon length 399 offset 199 columns 4 focus 3 canvas 50 voffset 0
column 0 ribbon-x 0 width 99 preset 1 windows 1 height 50
window 0 0 ribbon 0 0 99 50 screen -199 0 99 50
column 1 ribbon-x 100 width 99 preset 1 windows 4 height 50
window 1 0 ribbon 100 0 99 11 screen -99 0 99 11
window 1 1 ribbon 100 12 99 11 screen -99 12 99 11
window 1 2 ribbon 100 24 99 11 screen -99 24 99 11
window 1 3 ribbon 100 36 99 14 screen -99 36 99 14
column 2 ribbon-x 200 width 99 preset 1 windows 1 height 50
window 2 0 ribbon 200 0 99 50 screen 1 0 99 50
column 3 ribbon-x 300 width 99 preset 1 windows 2 height 50
window 3 0 ribbon 300 0 99 24 screen 101 0 99 24
window 3 1 ribbon 300 25 99 25 screen 101 25 99 25
end
```

Read it as a 200×50 terminal: four columns of 99 cells, a one-cell gap, a ribbon
399 long inside a 200 viewport, offset 199, and a stack of four panes splitting
50 rows into 11, 11, 11 and 14. **Not one fractional number**:
`ribbon_policy_width()` computes `((vw - gap) * pct) / 100` in integers
(`ribbon.c`), and the same is true of all ten policies. To this arithmetic
"pixel" and "cell" are the same word: unit.

Column width in cells on typical terminals, computed by the same utility
(`./cwm -C "layout-probe column-width viewport-width=N preset=P gap=1 min-width=20"`):

| Viewport | 33 % | 50 % | 67 % | 100 % |
|---|---|---|---|---|
| 80 | 26 | 39 | 52 | 80 |
| 120 | 39 | 59 | 79 | 120 |
| 200 | 65 | 99 | 133 | 200 |

From which a limit of applicability follows, and it should be said out loud:
**on an 80-column terminal there is no ribbon.** Two columns at 50 % are 39
cells each, and an eighty-column source file does not fit into 39. Two full
eighty-column panes side by side need a viewport of **161 cells**
(`column-width viewport-width=161 preset=1` → `80`). The ribbon in a terminal is
about a wide window and a whole screen, not about the default console.

The vertical axis behaves just as honestly. Seven panes stacked on a 24-row
terminal:

```sh
./cwm -C "layout-probe layout viewport=80x24 columns=7 gap=1 min-width=20 min-height=3 border=0 focus=0"
```

answers `canvas 30 voffset 6`: the canvas is six rows taller than the viewport,
the top panes have gone off the top edge (`screen 0 -6`), and they are reached
by scrolling — exactly the behaviour [ribbon.md](ribbon.md) describes for
pixels.

## What ports

```sh
LC_ALL=C sh tools/no-x-build.sh
```

```
ribbon.c без Xlib: собралась
  имён X11 в ribbon.o: ноль
  операций оконной системы: 11, все объявлены в wsi.h
десять политик отдельной единицей: собрались (-Wall -Wextra -Werror, ни одного заголовка X11)
  неопределённых имён: Conf - и больше ничего
```

*(`LC_ALL=C` is not decoration here: the guard compares two sorted lists of
names, and a Russian locale sorts the underscore differently, so the lists stop
matching and the check fails on a tree where nothing changed. CI runs in the C
locale, which is why this is invisible there.)*

| Layer | Lines | Cost of the move into a terminal |
|---|---|---|
| policy — ten `ribbon_policy_*` | 142 | **0**: numbers in, a number out, not one X11 header |
| ribbon model — columns, stacks, `ribbon_measure`, `ribbon_place`, `ribbon_scroll` | 256 | **0**, except the two places below |
| mechanics inside `ribbon.c` — everything that calls `client_*` | 359 | rewrite |
| mechanics outside `ribbon.c` | 4,880 | throw away whole: `xutil.c`, `xevents.c`, `screen.c` are about X11 and EWMH, which a terminal does not have |

The line counts come from [portability.md](portability.md), where they were
measured; `ribbon.c` as a whole is 1,221 lines (`wc -l ribbon.c`).

The ribbon's contract with the outside world is visible in full in
`nm -u ribbon.o`: **11 window-system operations**, every one declared in
`wsi.h`, plus `Conf`, `conf_ribbonrule_match`, `xcalloc`, `xstrdup` and libc.

| Contract function | Calls in `ribbon.c` | What it becomes in a terminal |
|---|---|---|
| `client_resize` | 1 | `ioctl(TIOCSWINSZ)` plus `SIGWINCH` to the pane. **The only place where the ribbon touches the world** |
| `client_hide` | 1 | leave the pane out of the frame; do not change its size |
| `client_show` | 2 | put it back in the frame; the application need not redraw |
| `client_set_active` | 1 | where keyboard input goes |
| `client_current` | 2 | the active pane |
| `client_raise` | 2 | output order; ribbon panes never overlap, so nothing |
| `client_ptr_save` | 1 | **stub** |
| `client_ptr_warp` | 2 | **stub** |
| `client_geom_current` | 1 | the pane's current frame in cells |
| `region_pointer` | 1 | **stub**: a terminal has no pointer |
| `wsi_settle` | 3 | **stub**: there are no self-caused pointer events to swallow |

(Call counts: `grep -c` on `ribbon.c` for each name.)

The one place where X11 still leaks into the model is the one the macOS port
named: `bwidth` in `ribbon_place()` — in a terminal a border is zero or one
cell, and both lines become plain assignments. The other two leaks are already
gone: `ribbon_settle()` is now `wsi_settle()` behind the seam, and `ribbon.o`
requires **zero** X11 names as it stands.

And a free gift, verified here against the same stub the ribbon is built
against:

```
probe.c без Xlib: собралась
неопределённые имена probe.o: Conf ribbon_col_add … ribbon_scroll xcalloc xstrdup strtonum … libc
```

`probe.c` — 1,084 lines (`wc -l probe.c`) — needs not one X11 name. So `ribbon.o + probe.o +
xmalloc.o` is a `layout-probe` inside the terminal build, and **the whole
conformance harness moves across for zero lines** (see "How it is checked").

## What ports at a cost

| What | Cost |
|---|---|
| **Mod4** | the legacy key encoding physically cannot carry Super; a full modifier is delivered only by the kitty keyboard protocol (`CSI = flags ; mode u`), which tmux also has as extended-keys, off by default (from the research). The key map is rebuilt — see [«Keyboard»](#keyboard) |
| **Floating windows** | `ribbon-float-toggle` and the 25 cwm commands "kept in place with a changed meaning" ([commands.md](commands.md)) lose their footing: a terminal has no free geometry, and in tmux 3.7 floating panes move by mouse only, with no scriptable geometry until 3.8 (from the research) |
| **The pointer** | three contract functions become stubs, and the second branch of `ribbon_current()` (`ribbon.c:397`) — "the ribbon under the pointer" — goes with them. There is one ribbon in a terminal; the branch is not needed |
| **More than one monitor** | a terminal is one viewport. `region_find` degenerates and so does `ribbon_screen_relayout`; [monitors.md](monitors.md) is about nothing in the terminal build. A monitor leaving and coming back becomes `SIGWINCH` — and it is **the same arithmetic**: `ribbon_policy_output` pulls the offset back inside the new ribbon length |
| **The panel** | there will be no external panel as in [panel.md](panel.md): the status line is ours, one row. The good news: "take a strip away from the viewport" is already the policy `ribbon_policy_reserve`, in the same unit as everything else |
| **Groups, menus, window search** | 24 group commands, 7 menu commands and the window search map onto a terminal without contradiction, but each needs drawing of its own. The decision is in the open questions |

## What does not port at all

**X11 and Wayland applications.** A multiplexer drives processes that speak to a
pseudo-terminal. The browser and the "other desktop things" of the task do not
appear in a terminal because a multiplexer appeared: that needs a separate
process that renders a frame and hands it over as an image. That is stage two
and a separate decision — see [«Graphics»](#graphics-if-there-is-any).

## What gets better

**`ribbonhide` stops being a compromise.** On X11, hiding a window past the edge
of the screen costs a [measured](offscreen.md) median of 108 ms against 40 and a
worst case of 661 ms against 99 under a heavy redraw — because the content of a
hidden window is held by the client, and coming back means a full redraw. In a
terminal the pane's content is held by **the multiplexer**, in its own cell
buffer: `Expose` does not exist, a column sliding in is a rectangle copy rather
than a request to an application to draw itself again. The setting either
disappears or survives as a memory saving, but it stops being a choice between
"expensive now" and "expensive later".

**"Zero neighbour redraws" becomes a tautology.** On X11 that is a
[measured](baseline.md) result, and it cost a moved `ribbon_sync()` call. In a
terminal a neighbour cannot redraw: the only signal that touches it is
`SIGWINCH`, and only a change of pane size produces one.

**Scrolling stops being work.** Verified by two probe runs: with `focus=1` and
`focus=2` on the same 80×24 viewport, the ribbon coordinates and widths of every
column match to the number, and only `screen x` changes (−40 → −80). On X11
every `ribbon_sync_one()` still called `client_resize()` for every window of
every column (`ribbon.c:685-721`); in a terminal that call would be a `SIGWINCH`,
and there must be **none at all**: nothing was resized. This turns into a check
rather than a hope (see below).

**The cost of the channel is known and small.** A full 200×50 text redraw is
3.5 KB after gzip, that is 354 frames per second on 10 Mbit (from the research,
not re-checked here). Over ssh the bottleneck is RTT and encoding CPU, not
bandwidth.

## How it is built

Three layers and a hard boundary between them.

```
ribbon policy       ribbon_policy_*        142 lines, taken verbatim
ribbon model        columns, stacks,       256 lines, taken verbatim
                    measure/place/scroll
window model        pane = pty + buffer    written from scratch
rendering           frame, diff, input     written from scratch
```

**The boundary rule:** the policy and the ribbon model may not learn a single
control sequence, the name `libvterm`, or a file descriptor. What guards this is
not a promise but a twin of `tools/no-x-build.sh`: the same build of `ribbon.c`
against a stub and the same list of permitted external names. Today there are
eleven of them, all declared in `wsi.h`, and not one is X11's; in the terminal
build all eleven must remain and not one foreign name.

**One `ribbon.c` across both surfaces is what route A *is*.** If the terminal
version grows a copy of the arithmetic, the product becomes a second product,
and within half a year two answers to one question will differ. What the X11 and
terminal versions share is not "the idea of a ribbon" but 1,221 lines of
`ribbon.c` and 1,084 lines of `probe.c` (`wc -l ribbon.c probe.c`), which already
build without X11 and already answer in cells.

**What to build the window model on.** Three options, each with its price:

| Base | What it gives | What it costs |
|---|---|---|
| **our own leaf on libvterm** | terminal stream parsing comes ready-made, everything else is ours, nothing extra to tear out | an external dependency where digitwm had three and all three were X11's; NetBSD and a minimal build become a question rather than a given |
| **an mtm-like leaf** (~1000 lines of C, the author declared it "finished") | the cleanest slate: small, finished, not moving — so it cannot drift away either | "not moving" is also the risk: maintenance becomes ours |
| **dvtm** (dead; last commit on master 2021-03-09, issue #119 "Call for Project Maintenance" unanswered) | the model is right and is exactly ours: a table of pointers, `static Layout layouts[] = {{"[]=", tile}, …}`, ~4000 lines, into which the ribbon slots as one line | a dead upstream: a fork with no hope of sending patches back |

All three come from the research; not one tree was opened here. **The first
question to any of them is the licence**, and it is not rhetorical:
`tools/check-licensing.py` fails the build on GPL text under version control,
because digitwm is permissive and stays permissive — BSD-2-Clause for our
files, ISC for the inherited ones ([NOTICE](../NOTICE)). Whoever writes the
first line must check the licence of the base; it is not checked here, and this
document will not invent it.

**What the mechanics do.** Four duties, no more:

1. **pane** — `forkpty`, stream parsing, a cell buffer, a title;
2. **layout** — hand `ribbon_place()` the viewport in cells, take the
   rectangles, and send `TIOCSWINSZ` only to the panes whose rectangle changed;
3. **frame** — cut the visible part out of the canvas, assemble the pane
   rectangles, emit a diff against the previous frame;
4. **input** — a key either becomes a ribbon command or goes as bytes into the
   active pty.

Duty 2 is exactly what `ribbon_sync_one()` does today, minus its main flaw: it
calls `client_resize()` on every window on every call without asking whether the
geometry changed. On X11 that costs [nothing measurable](portability.md); in a
terminal every superfluous call is a `SIGWINCH` to someone else's application
and a full redraw inside it. **The "what changed" check is mandatory here, not
nice to have.**

**Units change meaning, and defaults with them.** `ribbongap 8`,
`ribbonminwidth 120`, `ribbonminheight 60` are pixels; in cells they correspond
to roughly 1, 20 and 3 — the very numbers used in every probe above. The
percentages `ribbonwidths 33 50 67 100` carry over unchanged. How to spell this
in the configuration is a separate question for the owner.

## Keyboard

This is the main usability risk, and it is muscular rather than technical.

**What physically is not there.** The legacy key encoding does not carry Super:
`Mod4+h` cannot get down the channel at all. A full modifier is delivered only
by the kitty keyboard protocol (`CSI = flags ; mode u`), understood by kitty,
Ghostty, foot, WezTerm and Contour (from the research). tmux has the same thing
as extended-keys, off by default.

**What that frees up.** The three-layer rule of
[`session/docs/KEYS.md`](../session/docs/KEYS.md) —

| Layer | Modifier | Who handles it |
|---|---|---|
| Desktop | Mod4 | the window manager |
| Editor | Ctrl, Alt, Shift, F7–F12, `\` | vim |
| Multiplexer | Ctrl+b | tmux |

— does not break in a terminal session, it **shifts**: there is no desktop layer
there at all, and the multiplexer layer is ours, because digitwm-in-a-terminal
replaces tmux rather than living inside it. The middle layer stays untouched,
literally: `Ctrl+hjkl`, `Alt+hjkl` and the function keys belong to vim and may
not be taken — for exactly the reason the window manager does not take them.

**The proposed answer is two modes; both must exist, one is the default.**

1. **A `Ctrl+b` prefix with a sticky ribbon mode.** After the prefix, `h l j k`
   **repeat** without a new prefix until `Esc` or a timeout. This is not a
   flourish: ribbon navigation repeats by nature ("three columns left"), and
   `Ctrl+b h` three times is what makes people stop using the ribbon. Everything
   else is one shot after the prefix, as in tmux.
   **The cost:** the gesture does not match the X11 gesture (`Mod4+h` against
   `Ctrl+b h`), and muscle memory splits in two. That cost is already paid by
   everyone who has both a window manager and tmux; it will not be new, but it
   cannot vanish either.
2. **The kitty keyboard protocol as an option.** Turn it on and `Mod4+h` works
   literally, so the gestures in X11 and in the terminal match key for key.
   **The cost:** the list of terminals narrows to five names, and over ssh this
   is a property of the **local** terminal rather than of the remote machine,
   so one and the same session behaves differently depending on where you
   connected from. That cannot be the default: a key map that works in some
   places is worse than a less convenient key map that works everywhere.

**What stays put.** The names of the eleven ribbon commands
(`ribbon-focus-left`, `ribbon-move-right`, `ribbon-width-cycle`, …), because the
documentation, the configuration and the way we talk about the product must not
split along with the key map. The binding syntax stays the one in `cwmrc(5)`;
only the class of modifiers it can spell changes.

`Ctrl+a` as an alternative prefix was considered and rejected: readline uses it
for "beginning of line", and taking it away breaks the shell in every pane.

## Graphics, if there is any

This is **stage two**, and it is not in the first version. The decision about
its basis is recorded now so that stage one cannot make it impossible.

**Build on kitty placeholders (`U+10EEEE`), not on sixel.** From the research:
tmux has a 1 MiB DCS limit, and sixel frames larger than it are **silently
dropped**; `MAX_IMAGE_COUNT = 20`; images are re-encoded on every full redraw;
and in copy-mode they are not visible at all. Over ssh, of kitty's four transfer
methods only `d` works — base64 in 4096-byte chunks.

What follows for stage one: **a pane's buffer must be able to hold more than a
character in a cell**, or there is nowhere to put the placeholders and stage two
runs into a rewrite of the window model. Nothing else is required of stage one.

## What the first version will not have

As a plain list, so nobody goes looking:

- floating panes;
- any graphics — no kitty, no sixel, no pictures in the status line;
- the mouse: no focus-follows-pointer, no dragging of borders, no selection;
- multiple viewports and everything [monitors.md](monitors.md) describes;
- X11 and Wayland applications;
- nesting: a multiplexer inside a multiplexer is unsupported and will not be
  fixed;
- a status-line construction kit: one row, a fixed set of fields;
- cwm groups, menus and window search — unless the owner decides otherwise (see
  the open questions);
- **detach/attach and scrollback, unless the owner says yes** — and this is not
  a detail: without them the product does not replace tmux, which means it will
  be launched inside tmux and the prefixes will collide the same day.

## How it is checked

The checks fall into two halves: the ones the repository already performs and
gets for free, and the ones that have to be written.

### Free: the same conformance, the same arithmetic

`probe.c` builds without X11 — verified here. So the terminal build gets its own
`layout-probe`, and **the very vectors that run today** run through it: 201
scalar vectors over ten models in `fts/vectors/*.json` plus 13 layout scenarios
in `layout.json` — 214 in all, counted in the tree with
`jq -s 'map(length)|add' fts/vectors/*.json`. Run against both language surfaces
they make the 448 checks that `conformance.mjs` prints. The requirement is hard:

```
the terminal build must answer all 214 vectors with the same numbers as the
X11 build.  One mismatch = the arithmetic has forked = route A is broken.
```

This is not new work: `fts/harness/conformance.mjs` is already built this way
and only needs a path to the utility.

### To be written: five checks that do not exist yet

| Check | What it proves | How |
|---|---|---|
| **frame = layout** | what is drawn is exactly what the ribbon computed | a text dump of the multiplexer's frame is compared against `layout-probe layout …` in the same cells, character by character on the pane boundaries. This is the terminal twin of `invariants.mjs` |
| **`SIGWINCH` counter** | scrolling is not work | moving focus between columns and along a stack: **zero** `SIGWINCH`. Changing a column's width: exactly as many as there are panes in the column, and not one more |
| **bytes per scroll** | the channel cost is not invented | `wc -c` on the pty output for a single keypress. Count both raw and after `gzip`: 3.5 KB for a full 200×50 frame is the number from the research, and our frame must be **no larger** |
| **`ribbonhide` became free** | the promise this document makes | the median time for a column to slide in must not differ between `yes` and `no` by more than noise. On X11 it differed by 2.7× at the median and 6.7× at the worst case ([offscreen.md](offscreen.md)) |
| **`SIGWINCH` scenarios** | the viewport changes, the ribbon survives | the same seven scenarios `hotplug.mjs` runs: no column is lost, no pane migrates, and returning to the previous size restores the ribbon to the cell |

Plus a boundary guard: a build of `ribbon.c` against a terminal stub with the
list of permitted external names — a twin of `tools/no-x-build.sh` that fails
and names the line if a drawing call gets into the arithmetic.

### Kill thresholds

Three criteria under which the route is closed rather than polished — **the
share of words read correctly**, **mouse hit accuracy** and **latency**. All
three come from the research and belong to the graphics stage.

**The threshold numbers are not carried into this document, because they are not
here.** The run that wrote this specification had no access to that measurement,
and inventing a threshold is the worst thing one can do to a threshold. The
owner must write three numbers before the first line of stage two; below is the
shape in which they are checkable:

| Criterion | What to measure | Threshold |
|---|---|---|
| words read correctly | text rendered as an image inside cells is read aloud or recognised; the share of correct words out of the total | *(from the research)* |
| mouse hit accuracy | the share of clicks that landed on the element aimed at, when a cell coordinate is mapped back to image coordinates | *(from the research)* |
| latency | from the byte of the keypress to the last byte of the frame, `CLOCK_REALTIME`, the way `tools/redraw-probe.c` does it | *(from the research)* |

For stage one — the text-only one — a threshold of our own is **proposed**, and
it is a proposal, not a measurement: digitwm's own share of an insertion on X11
is measured at 1–10 ms with a median of 4–5 ([baseline.md](baseline.md)), and a
terminal version with neither an X server nor an IPC round trip has no right to
be worse. The proposed wording: **a median "key → frame" of no more than 10 ms
on a local machine**, with exactly the RTT and nothing else added over ssh.

## Why write our own when tuios exists

The honest section, for the sake of which the specification was rewritten.

**tuios** (github.com/Gaurav-Gosain/tuios, Go, MIT) is a live multiplexer that
calls itself a window manager: a niri-style ribbon of columns
(`scroll_focus_left/right`, `scroll_move_left/right`, `scroll_cycle_width`
33/50/55/67/90 %, `scroll_consume`, `scroll_expel`) and the full kitty graphics
protocol. That description comes from the research; **tuios was neither run nor
read here.**

So someone else's solution already does what route A promises to do — and one
thing (graphics) it can do while we are only planning it. Hence three possible
answers, and the choice is the owner's:

**(a) Write our own — justified by exactly one argument.** In digitwm the layout
numbers are not buried in code: ten policies are described by FTS models on two
surfaces, and CI proves the code agrees with the models over 214 vectors — the
448 checks of `conformance.mjs`. A
multiplexer built on `ribbon.c` inherits all of that for free — one arithmetic,
one conformance suite, two kinds of surface. tuios has a policy of its own
(visible already in the presets: 33/50/55/67/90 against our 33/50/67/100), and
our models do not check it in any way. **If the ribbon must be one arithmetic
across all surfaces, write our own; if "behaves similarly" is enough, do not.**

**(b) Do not write it; recommend it.** Then digitwm stays an X11 window manager,
and the answer to "what about a terminal" is a link to tuios. That is cheaper by
the whole window model and the whole renderer, and more honest than a third
multiplexer in the world.

**(c) Bring our policy into tuios.** Our ten functions are 142 lines of integer
arithmetic and are not hard to restate in Go. But conformance needs a utility of
`layout-probe`'s level inside someone else's tree and the agreement of someone
else's maintainer to keep it green. That is a negotiation, not a piece of work,
and its outcome cannot be predicted.

What this document **cannot** claim: that tuios is bad in any way. Not one
command here measured it.

## Open questions, and the owner's decisions

None of these may be left unanswered when the first line of code is written.

1. **The licence of the base.** mtm, dvtm, libvterm — under what exactly?
   `check-licensing.py` fails the build on GPL in the tree. Not checked here.
2. **Build or recommend** — answer (a), (b) or (c) from the tuios section.
   Everything below matters only under (a).
3. **detach/attach and scrollback in the first version.** "No" means the product
   is launched inside tmux and the prefixes collide. "Yes" means the first
   version is bigger than a minimal multiplexer.
4. **The default key map:** a `Ctrl+b` prefix with sticky mode, or the kitty
   keyboard protocol. The other one becomes an option.
5. **Units in the configuration:** change the defaults of
   `ribbongap`/`ribbonminwidth`/`ribbonminheight` silently in the terminal
   build, or introduce an explicit unit and require it in both builds.
6. **The second binary:** its name, its place in the tree, and whether it shares
   `ribbon.o` with `cwm` in one `Makefile`. The answer decides whether the
   arithmetic stays one physically rather than only in words.
7. **Groups, menus and window search** — port them into the terminal, or declare
   them unnecessary.
8. **The kill thresholds** — three numbers from the research.
9. **Graphics: stage two or never.** If "stage two", stage one must store more
   than a character in a cell.
10. **Who the target is.** "Over ssh" means the panes are processes on the
    remote machine and the local terminal only draws; everything that depends on
    the local terminal (the kitty protocol, images) is a property of the user's
    workstation rather than of the product, and must not be written into the
    requirements.
