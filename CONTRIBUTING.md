# Contributing

**Русская версия: [CONTRIBUTING.ru.md](CONTRIBUTING.ru.md).**

digitwm is a fork of [cwm](https://github.com/leahneukirchen/cwm) with 1 146
upstream commits kept intact. Two things follow, and they are not negotiable.

## What may not be done

- **Do not copy code from papersway** (GPL-3.0-or-later) **or from sdorfehs and
  ratpoison** (GPL-2.0-or-later). Reading them to understand an idea is fine.
  Copying a line is not: it would relicense this tree, close the path of patches
  back to cwm, and undo the work of changing the base. See [NOTICE](NOTICE).
- **Do not touch the copyright headers** of the inherited files, and do not
  edit `LICENSE` or `LICENSE.upstream`. `LICENSE` carries the verbatim BSD
  2-Clause text of our own code and nothing else, so that the licence is
  recognised automatically; `LICENSE.upstream` carries the ISC notice of the
  code inherited from cwm with all nine copyright lines. Which file is which is
  in [NOTICE](NOTICE).
- **A new file of ours opens with two lines**: `SPDX-FileCopyrightText: 2026
  Digitable <https://digitable.life>` and `SPDX-License-Identifier:
  BSD-2-Clause`. `ISC` there is a failure and not a small one: ISC is the
  licence of the inherited files, and marking one of ours with it misstates
  where the file came from. `tools/check-licensing.py` fails on both, and names
  the file.
- **Do not add a dependency.** `x11`, `xft`, `xrandr`, a C compiler and `yacc`
  are the whole list, and NetBSD is reachable exactly because of that. In
  particular, nothing under `fts/` may become a runtime dependency: **FTS runs
  in CI, never inside the window manager.** The flang compiler is not on the
  list either, and that is the whole reason `ribbon-flang/out-c/` is committed
  rather than emitted during the build.
- **Do not edit `ribbon-flang/out-c/`.** It is what the flang compiler printed
  out of the pinned `ribbon-flang/flang-ribbon` submodule, and a hand edit
  there is a defect, not a change: `make -C ribbon-flang emit` begins with
  `rm -rf out-c` and will wipe it without a word. `make -C ribbon-flang verify`
  re-emits into a temporary directory and diffs, so a tree that has drifted
  from its submodule is found rather than discovered later. The argument for
  keeping compiler output under version control at all is in
  [ribbon-flang/README.md](ribbon-flang/README.md).

## The order a layout change is made in

The layout policy exists twice — as C and as an FTS model on two surfaces — and
CI compares them vector by vector. So a change to any number that drives the
layout is made in **one commit**, in this order:

1. **The spec first**, in `fts/flang/<name>.flang`. It is the source of truth,
   and writing the rule first is what forces the question "what is the rule" to
   be answered before "how do I code it".
2. **The other surface**, in the same file: every `обеспечивает` obligation
   carries a view labelled `en:` beside it, and the two say the same thing word
   for word. `sh tools/check-flang-en-views.sh` compares them; a change made on
   one view only fails the build.
3. **A worked example on the new boundary**, in the spec. A rule with no example
   is a rule nobody has read out loud — and here an example is also a vector:
   the same numbers the live window manager is asked in step 5.
4. **The arithmetic**, and it no longer lives here: it is in the flang library
   [flang-ribbon](https://github.com/digitable-lol/flang-ribbon), which
   `ribbon-flang/flang-ribbon` pins by fingerprint. Change the behaviour there,
   re-run its own comparison against the reference, then move the fingerprint
   here, re-emit (`make -C ribbon-flang`) and put both in **one commit**. The
   ten `ribbon_policy_*` in `ribbon.c` stay what they are — wrappers that hand
   numbers over and take a number back — and the probe still calls them
   directly, which is what makes the comparison mean anything.
   `sh tools/check-ribbon-flang.sh` answers the question the change raises:
   does the ribbon still answer what it answered before flang, byte for byte,
   over the library's own grid of 526 871 inputs.
5. **A vector** in the list of `fts/flang/conformance.flang` (or, for a whole
   layout, of `fts/flang/layout.flang`), on the boundary the change is about. It
   should fail before step 4 and pass after it; if it passes before, it is not
   testing the change.
6. **The documentation**, in both languages. `doc/*.md` and `doc/*.ru.md` must
   say the same thing: **a difference in facts is worse than a missing
   translation.**

Then run what CI runs — the list is in [doc/build.md](doc/build.md).

There are no fields for the caller to compute any more. FTS had no way to
subtract one field from another, so `even-share`, `left-slack` and the rest were
computed outside the model by `fts/harness/derive.mjs` and handed over as
fields; in flang they are expressions inside the spec, and the table died with
the harness. The rule it guarded still holds and now needs no guard: no `if`, no
`min`, no `max` and no threshold may live outside the spec, because the
branching and the bounds *are* the policy.

## Changing something the models do not describe

Two promises of the ribbon are about the relation between two of its states
rather than about one number, so no model holds them:

```
opening a window alters the geometry of no window already on the ribbon
the focused column always lies wholly inside the viewport horizontally,
and the focused window of it vertically
```

They are checked by `fts/harness/invariants.mjs` against the running binary, and
the behaviour of a ribbon across a monitor coming and going by
`fts/harness/hotplug.mjs`. If a change touches insertion, scrolling or outputs,
it belongs in those harnesses, and the new check has to be shown to go red
before it goes green — that is what the `--selfcheck` mode of each is for.

## Documentation

Every document exists twice: `X.md` in English and `X.ru.md` in Russian, with a
link to the other in the first lines of each. One pair is spelled differently —
`LICENSE-EN.md` and `LICENSE-RU.md` — because the bare `LICENSE` is the
operative text and neither surface may take that name. The set is:

| Document | About |
|---|---|
| [README.md](README.md) / [README.ru.md](README.ru.md) | what digitwm is |
| [doc/ribbon.md](doc/ribbon.md) / [doc/ribbon.ru.md](doc/ribbon.ru.md) | the layout model, its commands and settings |
| [doc/commands.md](doc/commands.md) / [doc/commands.ru.md](doc/commands.ru.md) | every command, and what the ribbon did to it |
| [doc/monitors.md](doc/monitors.md) / [doc/monitors.ru.md](doc/monitors.ru.md) | more than one monitor, and hotplug |
| [doc/offscreen.md](doc/offscreen.md) / [doc/offscreen.ru.md](doc/offscreen.ru.md) | windows outside the viewport, with the numbers behind the default |
| [doc/baseline.md](doc/baseline.md) / [doc/baseline.ru.md](doc/baseline.ru.md) | flicker, insertion latency and hidden windows, measured live |
| [doc/panel.md](doc/panel.md) / [doc/panel.ru.md](doc/panel.ru.md) | the panel: ours against someone else's, and what a panel does to the ribbon |
| [doc/build.md](doc/build.md) / [doc/build.ru.md](doc/build.ru.md) | building, installing, running, checking |
| [doc/portability.md](doc/portability.md) / [doc/portability.ru.md](doc/portability.ru.md) | what here is X11 and what is arithmetic, and what a macOS port would cost |
| [doc/macos.md](doc/macos.md) / [doc/macos.ru.md](doc/macos.ru.md) | the macOS port: the decision, the order of the stages, and the thresholds to stop at |
| [doc/terminal.md](doc/terminal.md) / [doc/terminal.ru.md](doc/terminal.ru.md) | the ribbon in a terminal: the specification of route A |
| [doc/browser.md](doc/browser.md) / [doc/browser.ru.md](doc/browser.ru.md) | the browser: what was refused and why, and the page on which the ribbon itself decides |
| [doc/themes.md](doc/themes.md) / [doc/themes.ru.md](doc/themes.ru.md) | the colour schemes: the three sources `session/install.sh` takes them from, in order |
| [fts/README.md](fts/README.md) / [fts/README.ru.md](fts/README.ru.md) | the models and the harnesses |
| [pkgsrc/README.md](pkgsrc/README.md) / [pkgsrc/README.ru.md](pkgsrc/README.ru.md) | the package |
| [session/README.md](session/README.md) | the environment around the window manager |
| [LICENSE-EN.md](LICENSE-EN.md) / [LICENSE-RU.md](LICENSE-RU.md) | the licence in plain words: what you may do, what we ask, and why the tree holds three licences |

The models are the source of truth for anything about layout: a document
describes what a model says and links to it, rather than restating the rule in
its own words where the two can drift apart.

`cwmrc(5)` and `cwm(1)` are the manual pages, they are English only, and they
are upstream's format — a setting is documented there as well as in `doc/`, and
the manual page is the one a user reads first.

## Commit messages

Russian, and they explain **why**, not what the diff already shows. The subject
line says what changed in the behaviour of the program, not which files were
touched. What was measured goes in with its numbers; what was not checked is
said outright rather than left to be assumed.
