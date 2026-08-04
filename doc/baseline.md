# The three numbers, and the control sample that is missing

**Русская версия: [baseline.ru.md](baseline.ru.md).**

`DGT-WM-01` asked for a control sample — `papersway` over `i3` — and three
numbers from it: flicker, insertion latency, and what happens to windows that
are hidden. The point of the sample was to settle whether the fork was worth
starting at all.

## What happened to the control sample

**It was not measured, and this document does not pretend otherwise.** Neither
i3 nor papersway is installed on the machine digitwm is being written on, and
installing a GPL-3.0-or-later window manager to time it is a decision for the
owner rather than for a script: the licence contour of this project
([NOTICE](../NOTICE)) turns on nobody taking a line from papersway, and the
safest way to keep that true is not to have it open in the next terminal.

The decision the sample was meant to support has meanwhile been taken on other
grounds and carried out: the base is cwm, chosen by a written comparison of
dwm, spectrwm, bspwm, evilwm, herbstluftwm, sdorfehs and "from scratch on xcb"
in the portal's specification, and the reason is in
[README.md](../README.md#why-cwm-as-the-base) — cwm has no tiling model to tear
out. A measurement of papersway would not now change the base; it would only
tell us how a neighbour performs.

So what follows is the other half of the task: **the same three numbers for
digitwm itself**, so that there is something to compare against when a machine
with i3 and papersway on it turns up.

## How they were measured

```sh
sh tools/measure-insert.sh -d :77 -n 12 -w 500
```

Xvfb at 1280x800, one window opened at a time, each of them a
`tools/redraw-probe` client that reports every event it receives and the moment
it finished drawing. A software X server on a machine that was doing other work
at the time — which is visible in the numbers and is said here rather than
hidden.

## Flicker: zero, and it was not zero before

**No window that is already open redraws when another one opens.** Not one, in
eleven insertions.

That number was 1 per insertion until this measurement was taken, and finding
out *which* window redrew is what fixed it. The ribbon puts the new column
where an old one was standing and then scrolls the viewport; the new window was
being mapped first and covered its neighbour for the few milliseconds it took
to move it. X discards the contents of a window that is fully obscured, so the
neighbour then had to redraw itself whole — a flicker on a window that nothing
had asked to change, and a direct contradiction of the promise the ribbon is
built on.

The fix is one call moved: `ribbon_sync()` runs *before* the newcomer is
mapped, so the neighbours are already out of the way. The event log shows the
difference — the geometry of an existing window never changed in either case
(`634x798` throughout, only `x` moving), but the sequence
`VisibilityFullyObscured → VisibilityUnobscured → Expose 0 0 634 798` is gone.

This is the live counterpart of what `fts/harness/invariants.mjs` proves
arithmetically. The harness says no existing window's geometry changes; the
measurement says the screen agrees, and it caught something the harness could
not see.

## Insertion latency

| | from process start | from `XMapWindow` |
|---|---|---|
| best | 7 ms | 0 ms |
| typical | 300–800 ms | 1–10 ms |
| worst seen | 1 200 ms | 210 ms |

The first column is mostly `fork`, `exec` and connecting to the X server, and
says nothing about the window manager. The second is its share: accept the
`MapRequest`, choose the column, hand out the geometry, map. In most insertions
that is single-digit milliseconds; the outliers line up with the machine being
busy, and on a software server with a loaded machine there is no honest way to
separate them further. **The number to quote is the shape of it — the window
manager's own part is far below the point where an eye notices — not a median
from eleven noisy samples.**

## Windows that go off the viewport

With twelve windows in half-width columns on a 1280-point viewport, most of the
ribbon is off screen at any moment. **Not one client received an `UnmapNotify`**
— they stay mapped at a coordinate outside the screen, which is what
`ribbonhide no` means and what [offscreen.md](offscreen.md) measured the cost
of.

The alternative behaviour is one setting away, and the numbers behind the
default are in that document: with a heavy redraw, unmapping costs 2.7 times
the median and 6.7 times the worst case.

## What is still owed

A run of papersway over i3 on a machine that has them, with these three numbers
taken the same way. Until then the comparison is stated as missing and not as
"digitwm is faster" — nothing here has been compared to anything.
