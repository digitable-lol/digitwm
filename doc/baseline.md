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
in the portal's specification `docs/sdd/digitwm-window-manager.md`. cwm is
small — 7 883 lines of C, `x11`, `xft` and `xrandr` and nothing else, two
decades of maintenance in OpenBSD base — and, the part that decided it, **it
has no tiling model to tear out**: cwm is a floating window manager, so the
ribbon was added onto clean ground rather than grafted over someone else's
frame tree. A measurement of papersway would not now change the base; it would
only tell us how a neighbour performs.

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

**There were three runs, and the numbers below are consolidated over them.**
The first, on 2026-08-05, twelve windows. Two more on 2026-08-08, eight runs
back to back each with the same switches, 96 insertions apiece: one at a load
average of **57–94** across eight cores, the other at **34–49**. The load is
written down here because it is the main variable of this measurement: other
jobs were running on the machine at the time and could not be stopped, and an
"idle machine" was never once available. The 08-08 measurement was made
precisely to settle a disagreement of three sources about one number, and it
did not settle it in favour of the narrow range — see below.

## Flicker: zero, and it was not zero before

**No window that is already open redraws when another one opens.** Not one, in
eleven insertions.

Windows move in one step, and that is not an oversight: an X11 window manager
does not own the frame, so digitwm does not animate — fades and slides come from
a compositor run beside it.

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

The 2026-08-08 re-measurement confirmed it: **zero redraws across eight runs
back to back, 96 insertions**. One observation from that series is worth writing
down so that it is not mistaken for the flicker coming back: on the heavily
loaded machine one run counted two redraws, and both belonged to windows whose
own first frame was more than a second late (1 009 and 1 303 ms from
`XMapWindow`). The script counts as a redraw any `expose` on an already-open
window within a second of the next one starting, and a window that had not
managed to draw itself by that moment meets that definition with its FIRST
frame. So it is an artefact of how the counting works under load, not a redraw
of a neighbour; on the less busy machine, where no insertion ran past 20 ms, the
count stayed zero in all eight runs.

## Insertion latency

**These are two different numbers and must not be mixed up.** From process
start everything is measured together: `fork`, `exec`, connecting to the X
server, the client's own start-up — and the window manager only at the end.
From `XMapWindow` to the first finished frame exactly its share is measured:
accept the `MapRequest`, choose the column, hand out the geometry, map. The
first column answers "how soon do I see the window", and the machine answers
that, not the manager. The second answers "is the manager slow", and only it is
about this product.

| | from process start | from `XMapWindow` |
|---|---|---|
| best | 5 ms | 0 ms |
| median at load 34–49 | 10 ms | 4 ms |
| median at load 57–94 | 108 ms | 5 ms |
| typical | 10–800 ms | 1–10 ms |
| worst seen | 1 894 ms | 887 ms |

Nearly two hundred insertions on 08-08 (192, in two sets of 96) give the answer
this measurement was made for. **When the load on the machine doubled, the
median of the first column grew tenfold — from 10 ms to 108 — and the median of
the second did not move: 4 ms against 5.** That is the whole argument for
keeping these numbers apart: the first column measures the queue on eight busy
cores, the second measures the window manager's work, and the load on the
machine barely touches it.

The tail, however, does touch it: on the less busy machine 90 of 96 insertions
came in under 10 ms with a worst of 20 ms, on the busy one 62 of 96 with a worst
of 887 ms. The manager's own share does not stretch by itself, but it waits in
the queue like everything else.

**The thing to quote is the shape of that distribution, not a narrow range.** A
narrow range appears on its own: take one lucky run and its interquartile
spread and you get "2–6 ms", and that is true about exactly that run. Across all
192 insertions, 113 fall inside 2–6 ms — fewer than two in three. `1–10 ms` is
the narrowest claim that survives a change of load: 152 insertions out of 192,
with both medians inside it. That is what stands in the table, and what stands
on the product page.

The same goes for the first column: "5–15 ms from process start" is true of 77
insertions out of 96 on the less busy machine and of 13 out of 96 on the busy
one. It is a number about the machine it was measured on, not about the product,
and as a claim about the product it must not be quoted at all.

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
