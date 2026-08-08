# The panel: ours or someone else's

**Русская версия: [panel.ru.md](panel.ru.md).**

cwm has no panel, digitwm had none either, and the product page said so in
plain words. The question was never "draw a bar" but "who maintains it" - and
it is settled by a number rather than by taste.

**Decision: the panel is external. What we write is not a panel but the room
for one.**

## What our own would cost

The estimate for a panel of our own is in the specification
(`docs/sdd/digitwm-window-manager.md`, "what we would have to write") and comes
from living trees: `bar.c` in sdorfehs is 1,093 lines, `drw.c` in dwm is 471
lines of text drawing. **That is ~1,550 lines forever**: markup parsing, fonts
and UTF-8, a glyph cache, redrawing, and polling the battery, the network, the
volume and the load - on each of the three operating systems digitwm promises.

Room for someone else's panel cost **310 lines** across six files, **178 of
them not comments**:

| File | Lines | What |
| --- | --- | --- |
| `client.c` | 119 | read the strut, stop moving and bordering docks, remeasure on arrival and departure |
| `screen.c` | 91 | subtract from the work area what the panels took |
| `ribbon.c` | 55 | two pure policy functions: does a claim meet this monitor, and how much does it take |
| `calmwm.h` | 31 | `struct strut` and declarations |
| `xevents.c` | 12 | the panel rewrote its strut - remeasure |
| `xutil.c` | 2 | two atoms |

**1,550 against 310 is five to one; against 178 lines of actual code it is
nearly nine to one.** And that one-off comparison understates it: a foreign
panel brings its own battery, network and volume modules, while our own would
need another one written every time the owner wants another number on the bar.

What we pay: one session dependency and someone else's configuration style. The
panel starts from `~/.config/digitwm/autostart`, and without it the ribbon works
exactly as before, just with no bar.

## Which one, and why

**Not one** X11 panel was installed on this machine: no polybar, no lemonbar, no
tint2, no yambar, no xmobar, no dzen2 (`command -v` on each of them: empty).
There is `waybar`, which is for Wayland, and `xfce4-panel`, which drags a whole
foreign desktop behind it. So "use what is already there" was open to nobody:
something has to be installed either way, and the only question is what.

Measured on Ubuntu 24.04 amd64: packages downloaded and unpacked into a
directory of their own, sizes and licences taken from the packages themselves.

| Panel | Packages | Unpacked | Licence | Own modules | `_NET_WM_STRUT_PARTIAL` |
| --- | --- | --- | --- | --- | --- |
| **polybar 3.7.1** | **5** | **2,456 KB** | **Expat (MIT)** | **20**, among them battery, cpu, memory, network, alsa, pulseaudio, date | **yes** |
| yambar 1.10.0 | 2 | 612 KB | Expat + BSD-3 | 9: battery, network, pulse, clock, disk-io, … | yes |
| tint2 17.0.1 | 1 | 1,668 KB | **GPL-2 / GPL-2+** | a taskbar and a tray, not sensors | yes |
| dzen2 0.9.5 | 1 | 228 KB | Expat | none at all | yes |
| lemonbar 1.4 | 1 | 84 KB | Expat | none at all | yes |

The owner named six things: load, battery, network, volume, "and the rest", all
of it done well. Then:

- **lemonbar and dzen2** are out not on size but on work: they have no sensors
  whatsoever (`strings` on either binary finds neither `/sys` nor `/proc`). The
  whole content of the bar would be ours to write - which is our own panel
  again, only in shell and without a glyph cache.
- **tint2** is out on purpose: it is a taskbar with a tray, not sensors. And on
  its licence - see below.
- **yambar** is four times lighter than polybar and would do; it has 9 modules
  against 20, no alsa (pulse only) and no cpu or memory - those would be
  scripts.
- **polybar** covers the owner's whole list with its own modules and needs
  nothing written.

**polybar** it is. It is the heaviest of the permissive ones - 2.4 MB against
0.6 MB for yambar - and that is the only thing we pay for it.

### What the licence means for the paid archive

The panel is a separate program the session starts like any other; its code is
not linked into digitwm and never enters our binary. But the Workbench archive
is sold, and the difference still matters.

- **polybar, yambar, dzen2, lemonbar are Expat (MIT).** Putting the binary in a
  paid archive is allowed, with one requirement: keep the licence text and the
  copyright notice next to it. No opening of sources, no infection of our own
  code, nothing.
- **tint2 is GPL-2.** A separate process does not infect digitwm with copyleft,
  but putting its binary in a paid archive is distribution, and distribution
  carries the duty to hand the sources to everyone who got the binary. For an
  archive that is sold, that is separate work and a separate risk. **We do not
  take it.**

digitwm's own licence (ISC) is unchanged by any of these: the panel stays
outside.

## The room for a panel, and how it works

The specification recorded that the manager "already knows the panel mode
through `_NET_WM_WINDOW_TYPE_DOCK` and `_NET_WM_STRUT_PARTIAL`". **Half of that
was untrue, and running it showed so immediately.**

What was actually the case before this work:

1. `_NET_WM_STRUT_PARTIAL` appeared nowhere in the tree. The panel set its
   strut, the manager never read it, and the columns stayed 798 pixels tall,
   sliding under the bar.
2. Docks were **moved**: the panel asked for 1280x28 at 0,0, and
   `client_placement()` put it at 0,145 - across the windows it was supposed to
   sit above.
3. Opening any window while a panel was up **crashed the manager**.
   `client_init()` allocates a client with `xmalloc()`, which does not zero, and
   the ribbon fields are filled in only by `ribbon_col_add()` - which never runs
   for a floating window, that is, for docks, dialogs and transients. The
   garbage column pointer lived until the first question of "which ribbon is
   current": `ribbon_current()` reads `cc->rbcol->rb` of the active client. It
   died in `ribbon.c:652`, on the first try, every try.

Now: the `dock` type still means "the ribbon does not touch this window", and
the strut is read and taken out of every monitor's work area. A claim only
reaches the monitor its span reaches - a panel on the left monitor does not
shorten the right one. Two panels on one edge do not stack: the deeper one
wins. The strut is taken after the configured `gap`, so a panel that fits
inside a gap you have already given away costs nothing more.

Both decisions - "does it reach" and "how much does it take" - are the pure
functions `ribbon_policy_span()` and `ribbon_policy_reserve()`, and each has an
FTS model of its own: [`fts/strut-span.fts`](../fts/strut-span.fts) and
[`fts/strut-reserve.fts`](../fts/strut-reserve.fts) on both surfaces, 38
vectors checked against the live binary through `layout-probe`. **For a day and
a half they were missing**, and that is worth saying plainly: commit `585bf4d`
put both functions straight into C, bypassing `fts/`, which put the numbers of
the strip back where the project had walked away from - into code, where they
change silently. Among the rules of those models are both of the non-obvious
decisions: ends meeting at one point count as meeting, and a strip deeper than
the region leaves it empty rather than negative.

## What "collapsible" means in the ribbon model

The ribbon has one axis: the viewport travels sideways. A panel takes height,
which means it touches not one column but **all of them at once** - and
"collapse the panel" in this model means "give the band back to every column
at the same time".

The panel collapses itself, in one of two ways, and both work:

- **unmap its own window** - what polybar does on `polybar-msg cmd hide`. The
  manager sees `UnmapNotify`, drops the client, and the strut goes with it;
- **keep the window and zero the strut** - what others do. The manager sees
  `PropertyNotify` on `_NET_WM_STRUT_PARTIAL` and remeasures.

In `cwmrc` this is `4S-b`.

## The numbers

`sh tools/measure-panel.sh -d :92` - Xvfb 1280x800, three columns, a panel 28
pixels tall on the top edge, `ribbongap 8`:

```
== column geometry
  no panel:              -644,0 634x798   0,0 634x798   644,0 634x798
  panel shown:           -644,28 634x770  0,28 634x770  644,28 634x770
  panel collapsed:       -644,0 634x798   0,0 634x798   644,0 634x798
  panel back:            -644,28 634x770  0,28 634x770  644,28 634x770

== work area (_NET_WORKAREA)
  no panel:              0,0,1280,800
  panel shown:           0,28,1280,772
  panel collapsed:       0,0,1280,800
  panel back:            0,28,1280,772

== viewport offset: left edge of the leftmost column
  in all four states: -644

== redraws of already open windows
  appearing:        2 redraws across 3 windows
  collapsing:       1 redraw across 3 windows
  expanding:        2 redraws across 3 windows

== relayout latency: from the command to the last window's finished frame
  appearing:        10 ms
  collapsing:       4 ms
  expanding:        4 ms
```

The redraw counts wander from run to run, and there is no point pretending
otherwise. Five runs in a row: appearing 2 every time, collapsing between 0 and
2, expanding between 0 and 2. Latency: appearing 4-11 ms, collapsing 4-7 ms,
expanding 1-9 ms. The order of magnitude holds; the exact number does not.

Three things the measurement exists for:

1. **Height changes for every column at once and by exactly the panel's
   height:** 798 → 770 with a 28-pixel panel and a gap of 8 (798 − 28 = 770).
2. **The viewport offset survives both relayouts:** −644 in all four states.
   Collapsing the panel does not cost you your place on the ribbon.
3. **There are redraws, and there are few:** zero to two per relayout with
   three windows open. The ribbon's promise - "neighbours are moved, not squeezed" -
   is one a panel breaks by definition, since it squeezes everybody, and the
   price of that is now measured.

The third column is missing from the redraw counts on purpose: it stands past
the edge of the viewport and has nothing to draw.

## What was checked against a live panel

`polybar 3.7.1` from the Ubuntu package, with the configuration the session
installer lays down (`~/.config/digitwm/polybar.ini`), palette Digitable Focus
carbon:

- polybar sets `_NET_WM_WINDOW_TYPE_DOCK` and
  `_NET_WM_STRUT_PARTIAL = 0, 0, 28, 0, 0, 0, 0, 0, 0, 1279, 0, 0`;
- the ribbon gives the band up: windows become 632x768 at (0,28) and (644,28);
- `_NET_WORKAREA` is `0, 28, 1280, 772`;
- `polybar-msg cmd toggle` collapses and expands it, and the ribbon gives the
  band back and takes it again;
- the bar's background in a screenshot is exactly `#05080D`, that is the
  `background` of the carbon palette in `focus-palettes.json`. The colour was
  not picked again by us; it came from there;
- 6 modules of 7 loaded. The seventh, the battery, switched itself off with a
  line in the log: "No suitable way to get current charge state". The measuring
  machine is a virtual one and its `/sys/class/power_supply` is empty. **The
  battery has not been seen on a live wearable device, and that is recorded
  honestly below.**

## Icons: why the panel has none

The owner asked for quality icons "in our system". The Digitable set is the
portal's SVG sprite, `layouts/partials/icon-sprite.html`: 36 shapes, a 24x24
`viewBox`, a 1.8 stroke, pulled in through `<use href="#dgt-icon-…">`.

A panel label is drawn by Xft - a font glyph, not a stroked vector. **SVG
cannot go there by any route.** On top of that, of the six things the panel
shows, the set contains exactly one: `network`. There is no battery, no cpu, no
memory and no volume in it at all (searched the sprite: zero matches).

So the panel carries short words in the accent colour instead: `CPU`, `MEM`,
`NET`, `VOL`, `BAT`. They read in any palette and do not depend on which fonts
the system has - which is not a detail: the font the session configures the
terminal and the shell prompt with, `MesloLGS NF`, the session itself **does not
install** (it is listed among the missing, `session/README.md`), and this
machine does not have it.

What to do instead: build a **Digitable font** out of the same source shapes -
draw the battery, the cpu, the memory and the volume up to a set, and compile
the lot into a TTF in a private code range. Then the same shapes would be on the
portal and on the panel, and that would genuinely be "our system" rather than
someone else's glyphs. Until such a font exists, taking `FontAwesome` (which is
on this machine) or Nerd Fonts means putting somebody else's drawings on a
Digitable panel, and that is worse than words.

## What this document does not prove

- **More than one monitor.** The claim is computed per span and the code knows
  how, but Xvfb here has one output. Two live monitors are untested.
- **The battery.** The measuring machine has none; the module switches itself
  off and says why, but nobody has seen a reading on a wearable device.
- **Panels at the bottom and the sides.** The arithmetic is symmetric and
  `tools/strut-probe -b` knows the bottom edge, but the measurement in this
  document is the top one only.
- **polybar on FreeBSD and NetBSD.** digitwm promises three systems; one was
  checked. Should polybar fail to build on the other two, the replacement is
  obvious: yambar on the same scheme, a different configuration file, the same
  room for a panel.
