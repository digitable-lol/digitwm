# Windows the viewport does not show

**Русская версия: [offscreen.ru.md](offscreen.ru.md).**

The ribbon is longer than the screen, so at any moment some windows are off to
the side. There are two ways to treat them, and the choice is a setting:

```
ribbonhide no    # default: they stay mapped at a coordinate off the screen
ribbonhide yes   # they are unmapped until they scroll back in
```

Keeping them mapped means the X server holds their contents ready; the client
draws into a window it cannot see. Unmapping them stops that drawing, at the
price of a full map and a full redraw every time the ribbon brings one back.

The question is which costs more where it is felt: **how long after a scroll
until the incoming window is finished drawing.**

## How it was measured

`tools/redraw-probe.c` is a client that says when it was redrawn. It draws on
`Expose` the way an ordinary application does — a fill and a grid of lines,
`-w` segments of it — and prints, against `CLOCK_REALTIME`, when each event
arrived and when the drawing was finished (`XSync`, not `XFlush`: the interest
is in when the server was done, not when the request left).

`tools/measure-offscreen.sh` runs the same scroll under both settings: an Xvfb
server at 1280x800, six probe clients in six columns, the ribbon driven from
one end to the other and back with `super+l` / `super+h`, one keystroke at a
time with the wall-clock time of each keystroke recorded. Every `drawn` line
within a second of a keystroke belongs to it; the reported number is the last
of them — the moment the screen stopped changing.

```sh
sh tools/measure-offscreen.sh -d :77 -n 6 -r 40        # ordinary redraw
sh tools/measure-offscreen.sh -d :77 -n 6 -r 24 -w 20000  # heavy redraw
```

**Xvfb is a software server.** These numbers bound the cost of the protocol and
of the client's own drawing. They say nothing about a compositor or a GPU, and
nothing about clients that behave differently when unmapped. That limit is the
reason the numbers are quoted with the method rather than on their own.

## The numbers

Three runs, six clients, six columns, 1280x800x24:

| Run | Setting | Median, ms | 90th, ms | Worst, ms | Expose / scroll | unmap / scroll |
|---|---|---|---|---|---|---|
| 30 scrolls, 2 000 segments | `ribbonhide no` | 25.0 | 50.8 | 88.4 | 1.1 | 0 |
| 30 scrolls, 2 000 segments | `ribbonhide yes` | 30.3 | 44.5 | 60.9 | 2.5 | 1.0 |
| 40 scrolls, 2 000 segments | `ribbonhide no` | 20.3 | 28.0 | 57.4 | 1.1 | 0 |
| 40 scrolls, 2 000 segments | `ribbonhide yes` | 19.1 | 29.2 | 43.0 | 2.5 | 1.0 |
| 24 scrolls, 20 000 segments | `ribbonhide no` | 40.3 | 50.4 | 99.0 | 1.1 | 0 |
| 24 scrolls, 20 000 segments | `ribbonhide yes` | 108.1 | 155.6 | **661.0** | 2.5 | 1.1 |

## The decision

**The default stays `ribbonhide no`.**

With a cheap redraw the two settings are inside each other's noise: 25.0 against
30.3 in one run, 20.3 against 19.1 in the next, with the order of the two
reversing between runs. Nothing is bought there.

With a redraw ten times heavier — a page of text in a terminal is closer to the
second case than to the first — unmapping costs 2.7 times the median and 6.7
times the worst case: 108 ms against 40, and a 661 ms outlier against 99. That
is the difference between a scroll that feels instant and one that visibly
lands.

The event counts explain it and hold across every run: unmapping costs 2.5
`Expose` events per scroll against 1.1, plus a map and an unmap. A window that
stayed mapped is exposed only where it was genuinely clipped by the screen
edge; a window that was unmapped is exposed whole.

## What the setting is still for

`ribbonhide yes` remains, because the measurement above answers one question
and there are two:

- an animated client — a clock, a player, a graph — keeps drawing while it is
  mapped, even at a coordinate no one can see. Unmapping stops that, and this
  measurement did not weigh it: the probe draws only when asked to;
- some clients react to being unmapped. Players pause, terminals stop rendering,
  some toolkits drop their pixmaps. Whether that is a saving or a defect depends
  on the client, and it was not measured here either.

Both belong to `ribbonhide yes`, and both are stated rather than implied. What
was measured is what the table shows.
