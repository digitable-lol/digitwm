# Layout policy as an executable specification

**Русская версия: [README.ru.md](README.ru.md).**

Ten scalar decisions drive the ribbon — how far it scrolls along the row and
down the canvas after a focus change, how wide a column is, how tall a window in it is, where a new window
goes, what takes focus when a window closes, what happens to the offset when
the monitor changes size, whether the strip a panel claims reaches this
monitor at all, how much it takes off it, and what two facing panels are left
with when together they ask for more than there is. Each is an
[FTS](https://github.com/digitable-lol/flang/tree/fts-pered-udaleniem)
model here, on both surfaces, with checked properties and worked examples on
the boundaries.

| Model | C function | `layout-probe` utility |
|---|---|---|
| `scroll-offset` | `ribbon_policy_offset` | `scroll-offset`, `«Смещение ленты после фокуса»` |
| `stack-offset` | `ribbon_policy_voffset` | `stack-offset`, `«Смещение полотна после фокуса»` |
| `column-width` | `ribbon_policy_width` | `column-width`, `«Ширина колонки по пресету»` |
| `window-height` | `ribbon_policy_height` | `window-height`, `«Высота окна в колонке»` |
| `insertion` | `ribbon_policy_insert` | `insertion`, `«Куда вставить окно»` |
| `focus-after-close` | `ribbon_policy_close` | `focus-after-close`, `«Фокус после закрытия»` |
| `output-change` | `ribbon_policy_output` | `output-change`, `«Смещение после смены монитора»` |
| `strut-span` | `ribbon_policy_span` | `strut-span`, `«Достаёт ли полоса до области»` |
| `strut-reserve` | `ribbon_policy_reserve` | `strut-reserve`, `«Сколько полоса отнимает у области»` |

The last two rows arrived later than the rest, and how is worth saying out
loud. `ribbon_policy_span` and `ribbon_policy_reserve` came in with commit
`585bf4d` straight into C, bypassing `fts/`, and for a day and a half the
numbers of the panel strip lived in the code alone: the promise "the numbers of
the ribbon are not buried in C" had a silent exception of two functions. Only
reading could catch that, so the correspondence is now guarded by a check — the
`ribbon_policy_*` names of `ribbon.c` are matched against the file names of the
models, and an eleventh policy without a model goes red by itself.

`name.fts` is the Russian surface and `name.en.fts` the English one. They are
not translations of each other: one parser, one canonical document, and CI
proves it by comparing the two canonical JSONs field by field.

**FTS never runs inside the window manager.** It runs in CI. digitwm builds
with a C compiler, `yacc` and three X libraries on Linux, FreeBSD and NetBSD;
nothing here adds a runtime dependency on Node.

## Running it

The clone is of the language repository, and it must be by tag. The toolkit has
no repository of its own any more: the old address answers `Repository not
found`, which is why the checks below never ran — the clone failed before any of
them. The toolkit itself is kept in `digitable-lol/flang` under the tag
`fts-pered-udaleniem`: the state on 16 August 2026, the day the old FTS project
was moved out of the language tree. It is not on that repository's `main` —
what lives there now is a language whose compiler is written in itself and which
does not read `.fts` at all. The tag is frozen and does not move, so everyone
builds the same toolkit.

```sh
git clone --branch fts-pered-udaleniem https://github.com/digitable-lol/flang ../fts
(cd ../fts && npm ci && npm run build)
make

for m in fts/*.fts; do node ../fts/dist/src/cli.js check "$m" >/dev/null; done
for m in fts/*.fts; do node ../fts/dist/src/cli.js test  "$m" >/dev/null; done

node fts/harness/surfaces.mjs   --fts ../fts
node fts/harness/conformance.mjs --fts ../fts --wm ./cwm
node fts/harness/selftest.mjs    --fts ../fts --wm ./cwm

node fts/harness/invariants.mjs --wm ./cwm
node fts/harness/invariants.mjs --wm ./cwm --selfcheck
```

`conformance.mjs` is the bridge. One set of vectors from `fts/vectors/` goes
two ways — through the TypeScript `fts generate` emits, and through
`cwm -C "layout-probe ..."`, which answers with the very code the window
manager runs — and a mismatch in a single vector fails the build naming the
utility, the surface and the vector. It also replays whole scenarios: the
loop over columns is written out in JavaScript, every number in it comes from
a model, and the result is compared with `layout-probe layout` line by line.

`selftest.mjs` breaks one constant in a copy of the models and requires the
harness to notice. A green harness that cannot go red proves nothing.

`invariants.mjs` answers a different question. The two promises the ribbon is
built on — *opening a window alters no window already on the ribbon*, and *the
focused column always lies wholly inside the viewport horizontally, and the
focused window of it vertically* — are statements about
the relation between two states of the ribbon, and no scalar model can hold
one. The probe therefore runs `ribbon_insert()`, the call the MapRequest
handler makes, and prints the state before and after it; the harness compares
the two over 320 generated ribbons (612 insertions, 5 771 windows in the
resulting states). Its `--selfcheck` breaks seven things in an answer the window
manager actually gave — a preset changed under an existing column, a window
made one pixel shorter, the viewport scrolled off the focused column, a column
moved off the grid, the canvas scrolled past the focused window, the canvas
lower than its tallest column, a column lying about the height of its stack —
and requires each to be reported by the check it was aimed at, not by a
neighbouring one.

The border of the first promise is drawn there rather than smoothed over. It
holds in full for a **new column**: neighbours are pushed along the ribbon, all
by the same amount, and no width or height changes. Inserting into the stack of
an existing column — `insert=stack` — must recompute the heights inside *that*
column, or the new window would have nowhere to go; what is checked then is the
weaker statement that nothing outside that one column moves.

## What the caller computes, and why

An FTS condition compares one field against a constant or against a
percentage of one field. It cannot add two fields. So the caller computes the
differences and passes them as fields — `left-slack`, `right-slack`,
`scroll-limit` and so on. The line this directory holds:

- derived fields are linear combinations of the raw inputs, nothing else;
- no `if`, no `min`, no `max`, no threshold ever moves out of a model into
  `derive.mjs`. The branching, the bounds and the constants are the policy,
  and the policy is the model.

`derive.mjs` is the single table that says which fields exist, in which order,
and how each is computed. The harness checks the models against it, so a field
renamed in one place and not the other fails CI instead of silently feeding
the model something else.

## The two places where the language does not reach

**Integer division.** FTS multiplies a field by a constant; it has neither
integer division nor a remainder, and both are exact rational arithmetic
rather than C's truncation.

- `column-width` computes `33 percent of inner-width`, an exact fraction. C
  computes `(vw - gap) * 33 / 100` and truncates. The harness truncates the
  model's answer on one named line and refuses to let the concession hide
  anything: the fraction has to sit inside the same unit as C's integer, or
  the vector fails. The model carries an example with the fraction spelled
  out — `419.76000000000005` where C answers `419` — so that the gap is
  visible in the specification and not only in the harness.
- `window-height` divides the column by the number of windows, and the number
  of windows is a field. No difference can stand in for that, so the caller
  passes `even-share` and `remainder`. The decision that stays in the model is
  the one that matters: *the remainder goes to the last window*, and the
  bounds around it.

**Names on the English surface.** Reserved English phrases are rewritten into
Russian by the start of the line, without regard to context, so a field named
`rule` turns `rule is number` into a rule declaration and the parse fails with
`FTS_NATURAL_FIELD`. The field `rule` of `layout-probe` is therefore spelled
`config-rule` in the model.

## Domain of the models

The models are exact against C for

```
viewport width > 0, gap >= 0, minimum width and height >= 0,
ribbon length >= 0, column left edge >= 0, column width >= 0, offset >= 0,
window count >= 0, window index inside the column
```

which is what a ribbon produces: `ribbon_measure()` lays columns out from
zero, and both offset policies return non-negative numbers. Twenty thousand
random vectors per model in that domain, checked against C, produce no
mismatch.

Outside it — a negative offset, a column at a negative coordinate — the models
do not answer wrongly, they *refuse*: the properties are violated and the
generated code throws. Sixty thousand out-of-domain vectors produce 8 649
thrown properties and not one silently wrong number.

## Known divergences

**An empty column used to skip both bounds — fixed.** `ribbon_policy_height`
answered `vh` for `nwin <= 0` and returned before the `minh`/`vh` clamps. With
a viewport shorter than the minimum — `viewport-height=144, window-count=0,
min-height=148` — C answered 144 while the model's property "height is at
least the minimum" was violated. It was not reachable through `ribbon_place()`,
which only asks about windows that exist, so it was a latent inconsistency
rather than a visible bug; it is now `MAX(vh, minh)`, which is what the rest of
the function does and what `ribbon_policy_width()` does with a viewport
narrower than its minimum. Two vectors hold the boundary — `пустая колонка ниже
минимума` and `пустая колонка ровно по минимуму` — and both surfaces carry the
rule and a worked example, so the guard cannot be quietly reverted.

**"Width no larger than the viewport" is not an invariant.** The specification
lists it as a property of `column-width`; the code deliberately breaks it when
the viewport is narrower than the minimum width, and says so in a comment.
FTS properties know no conditions, so the model states the lower bound only
and covers the upper one with examples.

**The insert conditions are not "deliberately non-overlapping".** The comment
above `ribbon_policy_insert` says they are; a dock that also asks for
fullscreen matches two of them with different answers, and C's `if` chain
resolves it by order. The model relies on the same order — every rule whose
condition holds runs, and the last `then result equals` wins — so the rules
stand in increasing precedence. The comment in C is the thing that is wrong,
not the code.

## Porting to flang: a sample, measurements, and what follows

The FTS toolchain no longer exists as a separate project, and `main` of that
same repository now carries `fspec/` — specifications written in the flang
language itself. Hence the question: does `fspec` replace this directory
outright? The answer came from porting one model, not from reasoning. The
sample lives in `fts/flang/`.

### What the runs showed

The compiler builds with a single `make -C bootstrap -j8`; the language's own
specification check, `bootstrap/flang io fspec/guard.flang`, takes **1 min
41 s** on this machine and answers: 42 specs, 295 claims, every one proved
from zero axioms; 100 promises in the snapshot, each with the same goal; 2
translation variants, each promising what the original does; 150 concepts,
none carrying two meanings. This is a working machine, not a sketch. Three of
its properties were checked against our own subject matter.

**Scalar policy is expressible.** `fts/flang/output-change.flang` is the same
policy as `output-change.fts`: 111 lines in place of 187 (93 Russian plus 94
English), because there are no longer two surfaces.

**Both places where FTS did not reach are closed in flang.** The arithmetic is
complete — plus, minus, times, divide, remainder. Derived fields are therefore
unnecessary: the scroll limit is not passed in by the caller but written as
`ribbon-length minus viewport-width`. The model has three fields instead of
four, and its input now equals the input of `layout-probe`. The same removes
`equal share` and `remainder` from `window-height`. The second place — English
names colliding with reserved phrases — disappears with the English file.

**The language surface is solved differently, and better.** Here it is a second
file plus `surfaces.mjs` comparing two canonical skeletons. In flang it is a
**translation variant of the promise** in the same file: `обеспечивает «en:
offset is not negative»` stands one line below the original, its goal is
compared with the original's character for character, and language tags are a
closed list of nine. Two files have room to diverge; two adjacent lines do not.

### What the port costs: the kernel takes exactly half the properties

A claim counts as proved only when the `check --proof` ledger says so. On our
sample it says: **4 claims, 2 proved, 2 grid.** Both spellings of the lower
bound are proved by the rule "goal split by condition". Both spellings of the
upper bound — "offset is within the ribbon" — are not.

The shape of the body is not neutral. `if offset < 0 then 0 else offset` with
`offset: number` gets "stated, not proved": the kernel does not read a branch
condition as an assumption. It discharges the goal when the branch condition
*is* the goal narrowed to that branch. So the sample puts the guard the other
way round — `offset not less than 0` first — and the promise becomes proved.
The policy did not change; all ten vectors confirm it.

Measured across all ten policies, holding **22** properties:

| property shape | count | kernel today |
|---|---|---|
| `result not less than 0` | 9 | **takes it** — goal split by condition, or a declared natural |
| `result not greater than <literal>` | 2 | **takes it** — boundedness rule |
| `result greater than 0` | 1 | no: `not less than` requires zero on the right |
| `result <comparison> <field>` | 10 | no: a TERM bound exists in the boundedness rule and not in the order rule |

**Eleven of twenty-two**, established by running the compiler rather than by
reading it: with body `if n not greater than limit then n else limit` the goal
`result not greater than limit` stays "stated, not proved", while the same
shape with a literal is proved. The property does not vanish — the runtime
checks it after every return — but there is no promise about all inputs.

### Checking against the live binary does not go away

The main worry was that moving would trade 448 checks against a live `cwm` for
proofs about text. It did not hold. The language's order dictionary contains
`«Запустить процесс»` (run a process), so a flang plan does exactly what
`conformance.mjs` does. `fts/flang/conformance.flang` is 152 lines with no
JavaScript:

```sh
make -C bootstrap -j8                                   # in a flang clone
bootstrap/flang check fts/flang/output-change.flang --proof
bootstrap/flang test  fts/flang/output-change.flang
bootstrap/flang io    fts/flang/conformance.flang
```

Answers: `4 claims: 2 proved, 2 grid`; `11 examples, 11 passed, 0 failed`;
`model and cwm agree: 10 vectors, zero divergences` (10 orders, exit 0).

The vectors are the same `fts/vectors/output-change.json`, and the utility is
called by its Russian name in quotes exactly as `conformance.mjs` calls it —
the name is part of the model. The check also goes red: a copy of the model
with `+1` in one branch exits 1 and names **5 vectors out of 10**, with both
numbers on each.

One caveat: orders execute relative to the directory of the `.flang` file
itself, so the plan says `../../cwm` — the same reason `fspec/settings.txt`
says `../bootstrap/flang`.

### What a full move costs, and what to do about it

Ten policies, 20 model files, 3 740 lines, 87 rules, 81 examples, 22
properties. Extrapolating from the sample: roughly half the lines (one surface
instead of two), fewer fields by every derived one, and 11 of the 22
properties become proved.

Gained: proof over ALL inputs where today there are only vectors; one file
instead of two, with surfaces compared by a line rather than by a harness; no
Node in model checking; no pin to a frozen tag.

Lost: half the properties move from "checked by the runtime" to the same thing
without a promise; `derive.mjs` and its field table die together with derived
fields, and with them the check that model fields match the harness table;
whole scenarios (`layout-probe layout`) were not ported and stay in JavaScript.

**Proposal for CI: two steps, not a replacement.** The old conformance run on
tag `fts-pered-udaleniem` stays as it is — it gives 448 checks, 20 models and
whole scenarios, and it is currently green. A flang gate is added beside it for
what has already been ported. The argument against replacement is direct: one
policy of ten is ported, 2 claims of 4 are proved on it, and retiring 448
working checks for 11 future proofs is a losing trade. The argument against
doing nothing: the tag is frozen, nothing behind it is developed any more, and
a pin is a patch. Order of work: port one policy at a time, each with its own
`conformance.flang`, and drop the pin on the day all ten and the whole
scenarios are across.

### What actually happened, 3 September 2026

All ten policies are across, and not one of them came here. They live in
[flang-ribbon](https://github.com/digitable-lol/flang-ribbon), a library of its
own with its own CI; `ribbon-flang/flang-ribbon` pins it by fingerprint, the C
it is emitted to is committed in `ribbon-flang/out-c/`, and the ten
`ribbon_policy_*` of `ribbon.c` are wrappers over it. What that buys and what
it costs is in [../ribbon-flang/README.md](../ribbon-flang/README.md).

**The proposal above held, in both halves.** Nothing was retired: the
conformance run on the frozen tag still runs, and it now says **450** checks —
the two extra are a new whole-layout scenario, not a concession. What changed is
which code answers it: `layout-probe` now reports numbers that came out of
flang, and the models compare against those. Two independent statements of one
policy, in two languages, compared against a live binary — which is more than
either had alone, and is why the models are not now redundant.

The port was proved the way this directory prefers: by running both. The
library's own run against the pre-flang `ribbon.c` covers **526 871** inputs
with **0** discrepancies and three byte-identical answer streams; digitwm
repeats it from its own side, against the same pinned `ribbon.c` and the same
grid, in `sh tools/check-ribbon-flang.sh`. Two of the losses named above did
not happen either: whole scenarios never moved, and `derive.mjs` is still here
with its field table, because the models are still here.

**What is still open.** The pin on `fts-pered-udaleniem` is still a pin — the
models are FTS, and the toolkit behind that tag is frozen. Moving the models
themselves to flang is a separate piece of work from moving the arithmetic, and
only the second one is done.
