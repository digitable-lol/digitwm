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
not `digitwm`: a configuration written for cwm and everything written about cwm
carry over unchanged — see [pkgsrc/README.md](../pkgsrc/README.md) on what that
costs. The configuration file itself is ours; the search order is below.

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
starts `cwm -c` on it). Started on its own, `cwm` takes the first of these that
exists:

| | file | |
|---|---|---|
| 1 | the file named with `-c` | nothing overrides it |
| 2 | `$DIGITWMRC` | when set and not empty |
| 3 | `~/.digitable/digitwm/digitwmrc` | our own name, in the directory this family of tools keeps its settings in |
| 4 | `~/.cwmrc` | cwm's name, read so that whoever arrived from cwm keeps working — and said out loud, once, on stderr, when it is the file being read |

`cwm -n` prints the file the search settled on and stops. The order is one
piece of code, `confpath.c`, which the macOS build compiles too
([macos.md](macos.md)); `tools/check-config-order.sh` asks both binaries the
same seven questions in CI, and `--selfcheck` shows it reddening on a broken
order. Every setting is described in `cwmrc(5)`; the ribbon's own are collected
in [ribbon.md](ribbon.md).

## Checking a change

The specs are checked by the flang compiler, one binary that needs nothing but
`cc`; the same binary runs both comparisons against the live window manager.
Node is left with exactly two harnesses; why those two stayed is in their own
headers and in [`fts/README.md`](../fts/README.md).

```sh
make                                    # it has to compile first

git clone --depth 1 https://github.com/digitable-lol/flang ../flang
make -C ../flang/bootstrap -j4
PATH=$PWD/../flang/bootstrap:$PATH

for m in fts/flang/*.flang; do flang check "$m" --proof; done
for m in fts/flang/*.flang; do flang test  "$m"; done

sh tools/check-flang-en-views.sh
sh tools/check-flang-en-views.sh --selfcheck
flang io fts/flang/conformance.flang
flang io fts/flang/layout.flang
sh tools/check-flang-mutants.sh

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
