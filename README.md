# digitwm

A scrollable-tiling window manager for X11: windows stand in columns on an
endless horizontal ribbon, and the screen is a viewport that slides along it, so
a new window pushes the ribbon instead of squeezing its neighbours. The binary,
the configuration file and the manual pages keep cwm's names — `cwm`, `cwmrc`,
`cwm(1)`, `cwmrc(5)`.

**Русская версия: [README.ru.md](README.ru.md).**

## Build

A C compiler, `make` (BSD or GNU), `yacc` or `bison`, `pkg-config`, and the
headers of `x11`, `xft` and `xrandr`. That is the whole list: nothing under
`fts/` is a runtime dependency, so Node.js is needed neither to build nor to
run.

```sh
make
make install PREFIX=$HOME/.local   # bin/cwm, share/man/man1/cwm.1, share/man/man5/cwmrc.5
```

`PREFIX`, `DESTDIR` and `MANPREFIX` are honoured. The package names per system,
and the one-command path that installs the dependencies and the environment as
well (`sh bootstrap.sh`, `sh bootstrap.sh --plan` first), are in
[doc/build.md](doc/build.md).

## Run

```sh
exec cwm                 # the window manager alone
exec digitwm-session     # it, plus the environment session/install.sh lays out
```

`session/install.sh` also writes `digitwm.desktop` into
`~/.local/share/xsessions` (`--system` puts it in `/usr/share/xsessions`), so a
display manager offers the session by name.

## Configure

The session starts `cwm -c ~/.config/digitwm/cwmrc`; started on its own, `cwm`
reads `~/.cwmrc`. Every setting is described in `cwmrc(5)`; the ribbon's own
settings, key bindings and commands are in [doc/ribbon.md](doc/ribbon.md) and
[doc/commands.md](doc/commands.md). A configuration written for upstream cwm
carries over unchanged; one that still uses the old command names goes through
upstream's `migrate-config.pl`:
`perl migrate-config.pl <old >new`.

## Check a change

These need no X display and run against the tree you have just built:

```sh
make
sh tools/no-x-build.sh                          # ribbon.c compiles without Xlib, the seam is wsi.h
node fts/harness/invariants.mjs --wm ./cwm      # the two promises, on the built binary
node fts/harness/hotplug.mjs    --wm ./cwm      # a monitor leaving and coming back
python3 tools/check-licensing.py                # the licence gate CI runs on every push
./cwm -C 'layout-probe layout viewport=1280x800 gap=8 border=1 columns=1,3,1 presets=0,2,3 focus=1'
```

The last line is the layout policy answering for itself — the same call the
conformance harness makes, and a way to see what a change did to a layout
without starting a session. `make macos-check` checks the portable half of the
macOS layer and needs no macOS.

The rest of what CI runs — `surfaces.mjs`, `conformance.mjs`, `selftest.mjs` and
the models under `fts/` themselves — needs the FTS toolkit, cloned by tag from
the language repository. Those commands are in [doc/build.md](doc/build.md), and
`.github/workflows/fts-conformance.yml` runs them in that order. The order in
which a change to the layout is made is in [CONTRIBUTING.md](CONTRIBUTING.md):
the model first, the C fourth.

## The tree

| | |
|---|---|
| `ribbon.c` | the ribbon: columns, stacks, the viewport, insertion, focus. It names no X11 — the eleven things it asks of the window system are declared in `wsi.h` |
| `probe.c` | `layout-probe`: the layout policy answered without opening a display |
| `calmwm.c`, `client.c`, `screen.c`, `xevents.c`, `group.c`, `kbfunc.c`, `menu.c`, `search.c` | from cwm: clients, screens, events, groups, key bindings, menus |
| `conf.c`, `parse.y` | defaults and the `cwmrc` parser |
| `fts/` | the layout models and the harnesses over them. They run in CI, never inside the window manager |
| `doc/` | the documents, each of them in two languages |
| `session/` | the environment around the window manager, and its installer |
| `tools/` | the measurement scripts and the probe clients they drive |
| `pkgsrc/`, `macos/` | the pkgsrc package; the checkable half of the macOS layer |

## Documentation

Every document is here twice, in English and in Russian, and they say the same
things — a difference in facts is worse than a missing translation.

| | |
|---|---|
| [doc/build.md](doc/build.md) | building, installing, running, checking a change |
| [doc/ribbon.md](doc/ribbon.md) | the layout model, its commands and its settings |
| [doc/commands.md](doc/commands.md) | every command, and what the ribbon did to it |
| [doc/monitors.md](doc/monitors.md) | more than one monitor, and what happens on hotplug |
| [doc/offscreen.md](doc/offscreen.md) | windows outside the viewport, and the numbers behind the default |
| [doc/baseline.md](doc/baseline.md) | flicker, insertion latency, hidden windows — measured, and what is still missing |
| [doc/panel.md](doc/panel.md) | the panel: ours against someone else's, the numbers behind the choice, and what a panel does to the ribbon |
| [doc/themes.md](doc/themes.md) | where the installer gets Workbench themes: three sources, and the line it does not cross |
| [doc/portability.md](doc/portability.md) | what here is X11 and what is arithmetic, measured — and what a macOS port would cost |
| [doc/macos.md](doc/macos.md) | the plan of the macOS port: the stages, what will not be there by name, and the numbers at which it is closed |
| [doc/browser.md](doc/browser.md) | what of this can be shown in a browser for real, and what cannot |
| [doc/terminal.md](doc/terminal.md) | the specification of the ribbon in a terminal: what ports, what will not be there, and how it is checked |
| [fts/README.md](fts/README.md) | the models, the harnesses, and where they stop |
| [session/README.md](session/README.md) | the environment around the window manager: what you get and what you do not |
| [pkgsrc/README.md](pkgsrc/README.md) | the package, and what is missing until the first release |
| [CONTRIBUTING.md](CONTRIBUTING.md) | the order a layout change is made in |

## Status

Early. The ribbon is there and checked by the harnesses — columns, stacks, the
scrolling viewport, insertion, focus, width presets, per-output ribbons — but
nobody has lived in it for a week, the panel has been checked on one monitor and
without a battery ([doc/panel.md](doc/panel.md)), and the delivery paths
(`bootstrap.sh`, the pkgsrc port) have been exercised on Debian and by reading
rather than on every system they claim. Each document says which of its
statements were measured and which were not.

Licence: the terms of the tree are in [LICENSE](LICENSE); the provenance of the
inherited [cwm](https://github.com/leahneukirchen/cwm) code and the single
exception (`queue.h`, BSD-3) are in [NOTICE](NOTICE). Upstream's headers are
never touched, our own files carry their own, and `tools/check-licensing.py` is
what keeps the two apart. Part of [Digitable](https://digitable.life); the
write-up with the numbers from the harnesses is at
[courses.digitable.life/digitwm](https://courses.digitable.life/digitwm/), which
also says why digitwm stays out of the paid Workbench archive though the licence
does not forbid it.
