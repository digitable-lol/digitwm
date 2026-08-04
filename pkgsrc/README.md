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
`make makesum` writes it in one step once `v0.1.0` exists.

For the same reason `DISTNAME` says `0.1.0` and `GITHUB_TAG` says
`v${PKGVERSION_NOREV}`: the version is the one the first release will carry, not
one that has already been published.

## What has and has not been checked

**Checked:** the package builds nothing unusual. The Makefile in the repository
root is portable BSD/GNU make, uses `pkg-config` for the three X libraries and
`yacc` for the configuration parser, honours `PREFIX`, `DESTDIR` and
`MANPREFIX`, and installs exactly the three files `PLIST` lists. `make install
PREFIX=…` into an empty directory was run and produced `bin/cwm`,
`share/man/man1/cwm.1` and `share/man/man5/cwmrc.5`.

**Not checked:** the port has not been built by pkgsrc. There is no pkgsrc tree
and no NetBSD machine here; `bmake`, `pkgin` and `pkglint` are all absent. What
the port says is right by construction and by reading the guide, not by having
gone green. Do not report it as tested until someone has run `pkglint` and
`make install` on it.

## The name of the binary

The binary is still called **`cwm`**, and so are the manual pages, so that a
`cwmrc` and everything written about cwm carry over unchanged. That is why the
package sets `CONFLICTS+= cwm-[0-9]*`: the two cannot be installed side by
side.

Whether the fork should eventually rename its binary is a decision with a cost
on both sides — a rename breaks every configuration file, session script and
`.xinitrc` that already says `cwm`; keeping the name blocks installing the two
together — and it belongs to the owner, not to the port.

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
