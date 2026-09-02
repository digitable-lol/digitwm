# The pkgsrc port

**Русская версия: [README.ru.md](README.ru.md).**

`wm/digitwm` is a pkgsrc package for the window manager alone — no session, no
themes, no agent. It is here rather than in pkgsrc because it has not been
submitted yet; copy it in and build it:

```sh
cp -r pkgsrc/wm/digitwm /usr/pkgsrc/wm/digitwm
cd /usr/pkgsrc/wm/digitwm
make makesum          # writes distinfo from the release tarball
make install
```

## Why there is no `distinfo`

`distinfo` holds the checksums of the distribution file. **There is no release
tarball yet** — no tag has been pushed — so there is nothing to take a checksum
of, and a made-up hash would be worse than a missing file: it would fail at
fetch time with a message about corruption rather than about a missing release.

Which file the checksums will cover is already fixed. `DISTNAME` is
`digitwm-0.1.0` and `GITHUB_TAG` is `v${PKGVERSION_NOREV}`, and for a tag
pkgsrc's `mk/fetch/github.mk` builds the name as `${DISTNAME}${EXTRACT_SUFX}`:
it will fetch `https://github.com/digitable-lol/digitwm/archive/v0.1.0.tar.gz`
and store it as `digitwm-0.1.0.tar.gz`. So two steps, in this order:

1. whoever owns the release pushes the tag `v0.1.0`;
2. in a pkgsrc tree, one command writes the file:

```sh
cd /usr/pkgsrc/wm/digitwm && make makesum
```

That writes `distinfo` with the `BLAKE2s`, `SHA512` and `Size` lines for
`digitwm-0.1.0.tar.gz`. Step 2 cannot run before step 1: the version in the
Makefile is the one the first release will carry, not one already published.

## What has and has not been checked

Everything below was measured on Linux. **There is no NetBSD machine here**, so
none of it says that the port has been built by pkgsrc.

**Measured.** From the repository root, into an empty staging directory, with
the very flags the package passes (`INSTALL_MAKE_FLAGS`: `PREFIX` and
`MANPREFIX=${PREFIX}/${PKGMANDIR}`):

```sh
make                                                        # exit 0
make install PREFIX="$PWD/stage" MANPREFIX="$PWD/stage/man"
( cd stage && find . -type f -o -type l )
```

`make` exits 0 (with four compiler warnings), and the install puts exactly three
files in place:

```
bin/cwm
man/man1/cwm.1
man/man5/cwmrc.5
```

That is `PLIST`, line for line, checked both ways: nothing is installed that
`PLIST` does not list, and nothing is listed that is not installed. The `man/`
at the start of a `PLIST` line is not a guess about the layout — pkgsrc rewrites
it to `${PKGMANDIR}/` itself (`mk/plist/plist-man.awk`), so the same `PLIST` is
right whether `PKGMANDIR` is `man`, as it is by default, or `share/man`; both
were run and both produced the three files. `DESTDIR` was run too and is
honoured; the package does not have to pass it, because pkgsrc appends it to the
install command (`mk/install/install.mk`).

**Measured: `pkglint` is clean apart from the missing `distinfo`.** `pkglint`
is a portable Go program, so it does not need NetBSD — only a pkgsrc checkout to
read the infrastructure from:

```sh
go install github.com/rillig/pkglint/v23/cmd/pkglint@v23.21.1
cp -r pkgsrc/wm/digitwm /usr/pkgsrc/wm/digitwm
cd /usr/pkgsrc/wm/digitwm && pkglint -Wall .
```

Against a pkgsrc checkout of April 2026 (`mk/bsd.pkg.mk` 1.2061), version
23.21.1 — the one pkgsrc itself packages — prints one warning and nothing else:

```
WARN: distinfo: A package that downloads files should have a distinfo file.
```

which is the missing `distinfo` of the section above, and cannot be removed
before the tag exists. The dependencies, the categories, the licence, the
`MASTER_SITES` spelling and the `PLIST` all pass.

**Not checked.** The port itself has never been built or installed by pkgsrc:
`bmake` and `pkgin` are absent here, and so is a NetBSD machine. Each of these
is a command for whoever has one — run them in the copied package directory
`/usr/pkgsrc/wm/digitwm`:

```sh
make makesum      # only after v0.1.0 is pushed; writes distinfo
make              # the port has never been built by pkgsrc
make install      # the port has never been installed by pkgsrc
make package      # no binary package has ever been made
make clean        # and then check the tree is as it was
```

and, after installing, that the window manager actually comes up on NetBSD:

```sh
cwm -n            # names the configuration file it would read
cwm               # and then, on a display, that it comes up
```

Do not report the port as tested until `make install` above has gone green on a
NetBSD machine. What is written here is right by measurement where a measurement
is given and by reading the guide everywhere else.

## The name of the binary

The binary is still called **`cwm`**, and so are the manual pages, so that a
configuration written for cwm and everything written about cwm carry over
unchanged — the file itself is read from `~/.digitable/digitwm/digitwmrc` first
and from `~/.cwmrc` after it (`cwm(1)`, FILES). That is why the
package sets `CONFLICTS+= cwm-[0-9]*`: the two cannot be installed side by
side.

Whether the fork should eventually rename its binary is a decision with a cost
on both sides — a rename breaks every configuration file, session script and
`.xinitrc` that already says `cwm`; keeping the name blocks installing the two
together — and it belongs to the owner, not to the port.

## The dependencies, and why they are so few

The package declares three libraries through buildlink and two tools, and
nothing else:

```
USE_LANGUAGES=	c
USE_TOOLS+=	pkg-config yacc
.include "../../x11/libX11/buildlink3.mk"
.include "../../x11/libXft/buildlink3.mk"
.include "../../x11/libXrandr/buildlink3.mk"
```

That is the same list `bootstrap.sh` installs for a source build — a C
compiler and make, the headers of x11, xft and xrandr, `bison` and
`pkg-config` — expressed the way pkgsrc expects it. `USE_TOOLS+= yacc` covers
what `bootstrap.sh` calls `bison`: on NetBSD it resolves to `/usr/bin/yacc`
from the base system, elsewhere pkgsrc pulls in `devel/bison`, and the parser
needs no more than that, since `parse.y` uses classic yacc constructs only.
`USE_TOOLS+= pkg-config` resolves to `devel/pkgconf`. No `DEPENDS` line is
needed: the three buildlink includes are full dependencies, at build time and
at run time both, and the sources include no X headers beyond those three
libraries.

What `bootstrap.sh` installs on top of that — editor, terminal, shell,
multiplexer, themes, agent — is deliberately not in `DEPENDS`; see below. The
FTS models under `fts/` are checked in CI and are neither built nor installed,
which is what keeps this package free of a Node dependency on NetBSD.

## The whole environment, rather than the window manager

The port installs the window manager and nothing else. The environment the
window manager is meant to live in — editor, terminal, shell, multiplexer,
Digitable Focus themes and the Digit agent — is installed by
[`bootstrap.sh`](../bootstrap.sh) at the repository root, which is the "one
command" of `DGT-WM-12`:

```sh
sh bootstrap.sh --plan     # see what would happen
sh bootstrap.sh            # do it
session/verify.sh          # check the result
```

The split is deliberate. A package manager installs software; the session lays
out configuration in a user's home directory and has no business doing that
from a package's post-install script.
