# Contributing

**Русская версия: [CONTRIBUTING.ru.md](CONTRIBUTING.ru.md).**

digitwm is a fork of [cwm](https://github.com/leahneukirchen/cwm) with 1 146
upstream commits kept intact. Two things follow, and they are not negotiable.

## What may not be done

- **Do not copy code from papersway** (GPL-3.0-or-later) **or from sdorfehs and
  ratpoison** (GPL-2.0-or-later). Reading them to understand an idea is fine.
  Copying a line is not: it would relicense this tree, close the path of patches
  back to cwm, and undo the work of changing the base. See [NOTICE](NOTICE).
- **Do not touch the copyright headers** of existing files, and do not edit
  `LICENSE`. It carries the verbatim ISC text and nothing else, so that the
  licence is recognised automatically.
- **Do not add a dependency.** `x11`, `xft`, `xrandr`, a C compiler and `yacc`
  are the whole list, and NetBSD is reachable exactly because of that. In
  particular, nothing under `fts/` may become a runtime dependency: **FTS runs
  in CI, never inside the window manager.**

## The order a layout change is made in

The layout policy exists twice — as C and as an FTS model on two surfaces — and
CI compares them vector by vector. So a change to any number that drives the
layout is made in **one commit**, in this order:

1. **The model first**, in `fts/<name>.fts`. It is the source of truth, and
   writing the rule first is what forces the question "what is the rule" to be
   answered before "how do I code it".
2. **The other surface**, `fts/<name>.en.fts`. They are not translations of one
   another but two spellings of one document, and `surfaces.mjs` compares them
   skeleton to skeleton — numbers, operators, the order of rules. A change made
   on one surface only fails the build.
3. **A worked example on the new boundary**, in both surfaces. A rule with no
   example is a rule nobody has read out loud.
4. **The C**, in `ribbon_policy_*`. These functions take numbers, return a
   number, touch no state and call no X function — the probe calls them
   directly, and that is what makes the comparison mean anything.
5. **A vector** in `fts/vectors/<name>.json`, on the boundary the change is
   about. It should fail before step 4 and pass after it; if it passes before,
   it is not testing the change.
6. **The documentation**, in both languages. `doc/*.md` and `doc/*.ru.md` must
   say the same thing: **a difference in facts is worse than a missing
   translation.**

Then run what CI runs — the list is in [doc/build.md](doc/build.md).

Fields the caller computes (`even-share`, `left-slack`, and the rest) are
declared in exactly one place, `fts/harness/derive.mjs`, and the harness checks
the models against that table. No `if`, no `min`, no `max` and no threshold may
move out of a model into `derive.mjs`: the branching and the bounds *are* the
policy.

## Changing something the models do not describe

Two promises of the ribbon are about the relation between two of its states
rather than about one number, so no model holds them:

```
opening a window alters the geometry of no window already on the ribbon
the focused column always lies wholly inside the viewport
```

They are checked by `fts/harness/invariants.mjs` against the running binary, and
the behaviour of a ribbon across a monitor coming and going by
`fts/harness/hotplug.mjs`. If a change touches insertion, scrolling or outputs,
it belongs in those harnesses, and the new check has to be shown to go red
before it goes green — that is what the `--selfcheck` mode of each is for.

## Documentation

Every document exists twice: `X.md` in English and `X.ru.md` in Russian, with a
link to the other in the first lines of each. The set is:

| Document | About |
|---|---|
| [README.md](README.md) / [README.ru.md](README.ru.md) | what digitwm is |
| [doc/ribbon.md](doc/ribbon.md) / [doc/ribbon.ru.md](doc/ribbon.ru.md) | the layout model, its commands and settings |
| [doc/commands.md](doc/commands.md) / [doc/commands.ru.md](doc/commands.ru.md) | every command, and what the ribbon did to it |
| [doc/monitors.md](doc/monitors.md) / [doc/monitors.ru.md](doc/monitors.ru.md) | more than one monitor, and hotplug |
| [doc/offscreen.md](doc/offscreen.md) / [doc/offscreen.ru.md](doc/offscreen.ru.md) | windows outside the viewport, with the numbers behind the default |
| [doc/baseline.md](doc/baseline.md) / [doc/baseline.ru.md](doc/baseline.ru.md) | flicker, insertion latency and hidden windows, measured live |
| [doc/build.md](doc/build.md) / [doc/build.ru.md](doc/build.ru.md) | building, installing, running, checking |
| [fts/README.md](fts/README.md) / [fts/README.ru.md](fts/README.ru.md) | the models and the harnesses |
| [pkgsrc/README.md](pkgsrc/README.md) / [pkgsrc/README.ru.md](pkgsrc/README.ru.md) | the package |
| [session/README.md](session/README.md) | the environment around the window manager |

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
