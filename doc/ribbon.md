# The ribbon

**Русская версия: [ribbon.ru.md](ribbon.ru.md).**

Windows live on an endless horizontal row of **columns**. The screen is a
**viewport** onto that row; moving focus moves the viewport, and the row keeps
its shape. A column holds a **stack** of one or more windows, split vertically.

That is the whole model. Everything below is what follows from it.

## Two promises

```
opening a window alters the geometry of no window already on the ribbon
the focused column always lies wholly inside the viewport
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

The second promise is what makes the first bearable: the ribbon is longer than
the screen, so the viewport is pulled along to keep the column you are working
in fully visible, with a gap's width of the neighbour showing when there is
room for it.

Both are checked against the running binary, not asserted in a comment:

```sh
node fts/harness/invariants.mjs --wm ./cwm
```

## The numbers are a specification

Six decisions drive the layout, and none of them is buried in C:

| Decision | C function | FTS model |
|---|---|---|
| how far the viewport scrolls after a focus change | `ribbon_policy_offset` | [`fts/scroll-offset.fts`](../fts/scroll-offset.fts) |
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
| `ribbon-focus-up` | `4-k` | focus the window above in the stack |
| `ribbon-focus-down` | `4-j` | focus the window below in the stack |
| `ribbon-move-left` | `4S-h` | carry the window one column left, making a column at the edge |
| `ribbon-move-right` | `4S-l` | carry the window one column right |
| `ribbon-width-cycle` | `4-r` | step the column through the width presets |
| `ribbon-width-grow` | `4-equal` | next preset up |
| `ribbon-width-shrink` | `4-minus` | next preset down |
| `ribbon-center` | `4-c` | centre the focused column in the viewport |
| `ribbon-float-toggle` | — | take the window off the ribbon, or put a floating one on it |

`Mod4` is free in upstream cwm, so nothing inherited was rebound. Every other
cwm command is still there; what the ribbon does to each is in
[commands.md](commands.md).

## Settings

```
ribbon           yes     # the ribbon owns the layout
ribbongap        8       # between columns, between stacked windows, and as
                         # the margin kept at the viewport edge
ribbonminwidth   120     # narrowest a column may become
ribbonminheight  60      # shortest a window may become
ribbonwidths     33 50 67 100   # the four presets, in percent of the viewport
ribbonhide       no      # unmap what the viewport does not show
```

The gap is subtracted before the percentage, so two 50 % columns and the gap
between them come to exactly one viewport. A 100 % column is the whole viewport,
gap and all, since nothing stands beside it.

`ribbonhide` was decided by measurement, and the numbers are in
[offscreen.md](offscreen.md).

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
