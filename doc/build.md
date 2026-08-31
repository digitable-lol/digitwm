# Building, installing, running

**Русская версия: [build.ru.md](build.ru.md).**

## What it needs

A C compiler, `make` (BSD or GNU, both work), `yacc` or `bison`, `pkg-config`,
and the headers of three libraries: `x11`, `xft`, `xrandr`. That is the whole
list, and it is the same list cwm has — the ribbon added no dependency, which
is what keeps NetBSD reachable.

**Node.js is not among them.** The layout models under `fts/` are checked in CI
and never run inside the window manager.

## By hand

```sh
make
make install PREFIX=$HOME/.local    # bin/cwm, man1/cwm.1, man5/cwmrc.5
```

`PREFIX`, `DESTDIR` and `MANPREFIX` are honoured. The binary is called `cwm`,
not `digitwm`: a `cwmrc` and everything written about cwm carry over unchanged
— see [pkgsrc/README.md](../pkgsrc/README.md) on what that costs.

## One command, with the environment

```sh
sh bootstrap.sh --plan     # see what would happen, change nothing
sh bootstrap.sh            # build deps, build, install, then the session
session/verify.sh          # check the result
```

`bootstrap.sh` installs what digitwm is built *with* — compiler, yacc, the three
sets of X headers — and hands over to `session/install.sh`, which lays out the
environment: editor, terminal, shell, multiplexer, Digitable Focus themes and
the Digit agent. The split is on purpose: build dependencies are installed as
root through a package manager, the session is laid out in a user's home
directory and should not need root at all.

| System | Package manager | What is installed |
|---|---|---|
| Arch | `pacman` | `base-devel libx11 libxft libxrandr bison pkgconf` |
| Debian, Ubuntu | `apt` | `build-essential libx11-dev libxft-dev libxrandr-dev bison pkg-config` |
| Fedora | `dnf` | `gcc make libX11-devel libXft-devel libXrandr-devel bison pkgconf-pkg-config` |
| openSUSE | `zypper` | the same names openSUSE uses |
| FreeBSD | `pkg` | `libX11 libXft libXrandr bison pkgconf` |
| NetBSD | `pkgin` | `bison pkgconf` — X11 is in the base system |
| OpenBSD | — | nothing: X11 and yacc are in the base system |
| macOS | — | no build yet: an X11 window manager has nothing to manage there. The native port is decided and planned in [macos.md](macos.md); what it would cost is measured in [portability.md](portability.md) |

`--no-session` stops after the window manager; `--no-packages` never touches the
package manager; anything else is passed to `session/install.sh` untouched
(`--palette`, `--skip-install`, `--no-rc`, `--yes`).

Already have the toolchain? `bootstrap.sh` asks `pkg-config` rather than the
package database, and skips the package step when the headers are already there
— so it does not ask for a password it does not need.

**What was actually run:** the plan and the build-and-install path, on
Debian/Ubuntu, into an empty prefix. The package step on Arch, FreeBSD and
NetBSD is written from their package names and has not been executed here;
`--plan` prints the exact command so you can check it before it runs.

## As a package

`pkgsrc/wm/digitwm` is a pkgsrc package for the window manager alone, with the
state of it — and what is missing until the first release tag — in
[pkgsrc/README.md](../pkgsrc/README.md).

## Running it

```sh
exec digitwm-session          # what session/install.sh puts in ~/.xinitrc
exec cwm                      # the window manager on its own
```

A `digitwm.desktop` is installed into `~/.local/share/xsessions`, so a display
manager will offer the session by name.

Configuration lives in `~/.config/digitwm/cwmrc` (the session puts it there and
starts `cwm -c` on it) or in `~/.cwmrc`. Every setting is described in
`cwmrc(5)`; the ribbon's own are collected in [ribbon.md](ribbon.md).

## Checking a change

The FTS toolkit is cloned from the language repository by tag: it has no
repository of its own any more, and the old address answers `Repository not
found`. `fts-pered-udaleniem` is its state on the day it was moved out of the
language tree; the tag is frozen, and `main` there does not carry it. Why that
is so is in [`fts/README.md`](../fts/README.md).

```sh
make                                    # it has to compile first

git clone --branch fts-pered-udaleniem https://github.com/digitable-lol/flang ../fts
(cd ../fts && npm ci && npm run build)

for m in fts/*.fts; do node ../fts/dist/src/cli.js check "$m" >/dev/null; done
for m in fts/*.fts; do node ../fts/dist/src/cli.js test  "$m" >/dev/null; done

node fts/harness/surfaces.mjs    --fts ../fts
node fts/harness/conformance.mjs --fts ../fts --wm ./cwm
node fts/harness/selftest.mjs    --fts ../fts --wm ./cwm
node fts/harness/invariants.mjs  --wm ./cwm
node fts/harness/invariants.mjs  --wm ./cwm --selfcheck
node fts/harness/hotplug.mjs     --wm ./cwm
node fts/harness/hotplug.mjs     --wm ./cwm --selfcheck
```

That is exactly what `.github/workflows/fts-conformance.yml` runs, in the same
order. What each of them proves is in [`fts/README.md`](../fts/README.md); the
order in which a change to the layout has to be made is in
[CONTRIBUTING.md](../CONTRIBUTING.md).

`tools/measure-offscreen.sh` is not part of that set: it needs `Xvfb` and
`xdotool`, takes minutes rather than seconds, and answers a question about
timing rather than about correctness ([offscreen.md](offscreen.md)).
