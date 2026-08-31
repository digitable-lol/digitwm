# The ribbon

**Русская версия: [ribbon.ru.md](ribbon.ru.md).**

Windows live on an endless horizontal row of **columns**. A column holds a
**stack** of one or more windows, split vertically. Row and stacks together are
the **canvas**; the screen is a **viewport** onto it and slides over both of its
axes. Moving focus moves the viewport, and the canvas keeps its shape.

That is the whole model. Everything below is what follows from it.

## Two promises

```
opening a window alters the geometry of no window already on the ribbon
the focused column always lies wholly inside the viewport horizontally,
and the focused window of it vertically
```

The first is the reason the ribbon exists. A tiling layout that splits the
screen has to take space from a neighbour to give it to a newcomer, and with
five windows open every one of them is a sliver. Here a new window arrives as a
new column: the columns to its right are **pushed along the ribbon**, not
squeezed, and the ones to its left do not move at all.

The border of that promise is drawn where it stops holding. Putting a window
into the stack of an existing column *does* recompute the heights inside that
one column — there is nowhere else for it to go. Nothing outside that column
moves.

The second promise is what makes the first bearable: the canvas is larger than
the screen, so the viewport is pulled along to keep what you are working in
fully visible, with a gap's width of the neighbour showing when there is room
for it.

The unit differs per axis, and not by carelessness. Across, the viewport follows
the **column**: a column is bounded by its own width. Down, it follows the
**focused window**, because a stack can be taller than any screen and a promise
about the whole of it would be a lie: at 1280x800, gap 8, minimum height 60,
eleven windows fit, twelve make a canvas 811 tall, twenty make one 1352 tall.
Whatever did not fit is reached by scrolling: `ribbon-focus-down` takes the
canvas to exactly 11 and 552 respectively. Before the second axis those pixels
were windows nothing could reach.

What is larger than the viewport shows its beginning: a column wider than the
screen its left edge, a window taller than the screen its top.

Both are checked against the running binary, not asserted in a comment:

```sh
node fts/harness/invariants.mjs --wm ./cwm
```

## The numbers are a specification

Seven decisions drive the layout, and none of them is buried in C:

| Decision | C function | FTS model |
|---|---|---|
| how far the viewport scrolls along the ribbon after a focus change | `ribbon_policy_offset` | [`fts/scroll-offset.fts`](../fts/scroll-offset.fts) |
| how far it scrolls down the canvas after a focus change | `ribbon_policy_voffset` | [`fts/stack-offset.fts`](../fts/stack-offset.fts) |
| how wide a column is | `ribbon_policy_width` | [`fts/column-width.fts`](../fts/column-width.fts) |
| how tall window *n* of *m* in a column is | `ribbon_policy_height` | [`fts/window-height.fts`](../fts/window-height.fts) |
| where a new window goes | `ribbon_policy_insert` | [`fts/insertion.fts`](../fts/insertion.fts) |
| what takes focus when a window closes | `ribbon_policy_close` | [`fts/focus-after-close.fts`](../fts/focus-after-close.fts) |
| what happens to the offset when the monitor changes | `ribbon_policy_output` | [`fts/output-change.fts`](../fts/output-change.fts) |

**The models are the source of truth, and CI proves the code agrees with
them.** Each exists on two surfaces — `name.fts` in Russian, `name.en.fts` in
English — which are not translations of each other but two spellings compiled to
one canonical document. The same vectors go through the code FTS generates and
through the live window manager, and a mismatch in a single vector fails the
build naming the utility, the surface and the vector. See
[`fts/README.md`](../fts/README.md).

**FTS never runs inside the window manager.** It runs in CI. digitwm builds with
a C compiler, `yacc` and three X libraries; nothing here adds a runtime
dependency on Node.

The loop over columns stays in C, because a loop is mechanical. The numbers the
loop substitutes are policy, and policy belongs where it can be read and tested.

## Commands

| Command | Default key | What it does |
|---|---|---|
| `ribbon-focus-left` | `4-h` | focus the column to the left |
| `ribbon-focus-right` | `4-l` | focus the column to the right |
| `ribbon-focus-up` | `4-k` | focus the window above in the stack; the canvas follows |
| `ribbon-focus-down` | `4-j` | focus the window below in the stack; the canvas follows |
| `ribbon-move-left` | `4S-h` | carry the window one column left, making a column at the edge |
| `ribbon-move-right` | `4S-l` | carry the window one column right |
| `ribbon-move-up` | `4S-k` | move the window one place up its own stack |
| `ribbon-move-down` | `4S-j` | move the window one place down its own stack |
| `ribbon-column-swap-left` | `4CS-h` | exchange the column with the one on its left, windows and all |
| `ribbon-column-swap-right` | `4CS-l` | exchange the column with the one on its right |
| `ribbon-width-cycle` | `4-r` | step the column through the width presets |
| `ribbon-width-grow` | `4-equal` | next preset up |
| `ribbon-width-shrink` | `4-minus` | next preset down |
| `ribbon-center` | `4-c` | put the focus in the middle of the viewport, both axes |
| `ribbon-float-toggle` | — | take the window off the ribbon, or put a floating one on it |

`Mod4` is free in upstream cwm, so nothing inherited was rebound. Every other
cwm command is still there; what the ribbon does to each is in
[commands.md](commands.md).

## Putting the ribbon in order

The canvas has two axes and so does rearranging it. `ribbon-move-up` and
`ribbon-move-down` move a window inside the stack it already stands in;
`ribbon-column-swap-left` and `ribbon-column-swap-right` exchange whole
columns. Neither crosses into the other's business: the first never changes
which column a window is in — that is what `ribbon-move-left` and
`ribbon-move-right` are for — and the second never takes a window out of a
stack.

Three things follow from the model and are worth saying out loud:

- **a height belongs to a slot, not to a window.** The remainder of the
  division goes to the last window of a column, so two windows trading places
  at the bottom of a stack trade their heights with them. The column ends up
  exactly as tall as it was, which is why nothing outside it moves;
- **the swap carries everything.** A column travels with its windows in their
  order, its focus and its width preset. Since the widths are the same widths
  in another order, the ribbon keeps its length and every column that was not
  one of the two keeps its place on it;
- **there is no wrap.** A window at the top of its stack and a column at the
  left end of the ribbon stay where they are. A ribbon is a row with two ends,
  not a ring.

None of it has to be taken on trust. The probe replays the same model calls
the commands make, with no X server in the room:

```sh
./cwm -C "layout-probe layout viewport=1280x800 columns=1,3,1 presets=0,2,3 focus=1 swap=left ids=1"
./cwm -C "layout-probe layout viewport=1280x800 columns=3 focus=0 focus-window=1 reorder=down ids=1"
```

Each prints the ribbon before and after, and each window line ends with the
identity of the window — the one thing coordinates cannot say when two windows
trade slots of equal size.

## Settings

```
ribbon           yes     # the ribbon owns the layout
ribbongap        8       # between columns, between stacked windows, and as
                         # the margin kept at the viewport edge
ribbonminwidth   120     # narrowest a column may become
ribbonminheight  60      # shortest a window may become
ribbonwidths     33 50 67 100   # the four presets, in percent of the viewport
ribbonhide       no      # unmap what the viewport does not show
ribbonwarp       yes     # carry the pointer into the window that took the focus
```

The gap is subtracted before the percentage, so two 50 % columns and the gap
between them come to exactly one viewport. A 100 % column is the whole viewport,
gap and all, since nothing stands beside it.

`ribbonhide` was decided by measurement, and the numbers are in
[offscreen.md](offscreen.md).

`ribbonwarp no` leaves the pointer where you put it down — on a focus command
and on a window arriving on the ribbon alike. The focus still moves: the ribbon
hands the keyboard over itself, and the crossing its own scroll causes under a
resting pointer is swallowed, so nothing snaps back. What it costs is that the
pointer and the keyboard are then on different windows, and the next nudge of
the mouse across a window boundary gives the focus to whatever is under the
pointer — that is cwm following the mouse, not a decision of the ribbon. Both
sides of the switch are taken off a live server by `tools/measure-warp.sh`.

Full descriptions are in `cwmrc(5)`.

## Windows that do not want a column

Dialogs, transients and docks float — that is `ribbon_policy_insert`, and it is
a model like the rest. A fullscreen window keeps its slot in the column and its
own geometry, because EWMH fullscreen is a request from the client rather than
a layout choice. `window-freeze` does the same on purpose: freeze a window and
it keeps whatever geometry you give it while still holding its place.

## More than one monitor

Each RandR output has its own ribbon, its own columns and its own offset; see
[monitors.md](monitors.md).
