# Layout policy as an executable specification

**Русская версия: [README.ru.md](README.ru.md).**

Ten scalar decisions drive the ribbon — how far it scrolls along the row and
down the canvas after a focus change, how wide a column is, how tall a window in it is, where a new window
goes, what takes focus when a window closes, what happens to the offset when
the monitor changes size, whether the strip a panel claims reaches this
monitor at all, how much it takes off it, and what two facing panels are left
with when together they ask for more than there is. Each is a
[flang](https://github.com/digitable-lol/flang) spec here — one file, with
checked promises and worked examples on the boundaries, and the examples are
the vectors.

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

There is one file per policy, `fts/flang/<name>.flang`, and the second language
surface is not a second file: every `обеспечивает` promise carries a view
labelled `en:` beside it. The compiler does not compare the two — it reads the
label as part of the name — so `sh tools/check-flang-en-views.sh` does, pair by
pair, and shows on `--selfcheck` that it can go red.

**The specs never run inside the window manager.** They run in CI. digitwm
builds with a C compiler, `yacc` and three X libraries on Linux, FreeBSD and
NetBSD; nothing here adds a runtime dependency on flang or on Node.

## Running it

One binary and one `make`. The flang compiler is written in itself; `bootstrap`
is its seed in C and needs nothing but `cc`. There is no pin any more: the
compiler is taken from `main`, so a divergence between the tree and the live
language shows up on the day it happens.

```sh
git clone --depth 1 https://github.com/digitable-lol/flang ../flang
make -C ../flang/bootstrap -j4
PATH=$PWD/../flang/bootstrap:$PATH
make

for m in fts/flang/*.flang; do flang check "$m" --proof; done
for m in fts/flang/*.flang; do flang test  "$m"; done

sh tools/check-flang-en-views.sh
sh tools/check-flang-en-views.sh --selfcheck
flang io fts/flang/conformance.flang
flang io fts/flang/layout.flang
sh tools/check-flang-mutants.sh

node fts/harness/invariants.mjs --wm ./cwm
node fts/harness/invariants.mjs --wm ./cwm --selfcheck
node fts/harness/hotplug.mjs    --wm ./cwm
node fts/harness/hotplug.mjs    --wm ./cwm --selfcheck
```

`conformance.flang` is the bridge, and it answers the question `flang test`
cannot. The examples of a spec are a snapshot taken off the binary once; here
the two counts run today and apart — 201 vectors go through the spec and
through `cwm -C "layout-probe ..."`, which answers with the very code the
window manager runs, and a mismatch in a single vector fails the build naming
the utility, the vector and both numbers.

`layout.flang` does the same for whole scenarios, and does it harder than the
Node harness did. Instead of parsing the probe's output into a structure and
comparing a hand-written list of fields, the spec **prints the same text the
probe prints**, and the two byte streams are compared whole: 14 scenarios,
every column, every window, on the ribbon and on the screen. A field nobody
remembered to compare is no longer possible.

`sh tools/check-flang-mutants.sh` is the negative control, and it corrupts the
answer of C rather than the spec. Corrupting the spec proves nothing here: the
examples of a spec ARE the vectors, so any such corruption is caught earlier, by
`flang test`, on the spec's own examples. What the comparison stands for is the
other direction — a divergence arriving from the C side — and that is what is
faked, by substituting `cwm` with a shell wrapper that adds one to a number. Ten
utilities in one run, six kinds of layout line, sixteen corruptions, each of
which must be noticed AND named.

`invariants.mjs` answers a different question. The two promises the ribbon is
built on — *opening a window alters no window already on the ribbon*, and *the
focused column always lies wholly inside the viewport horizontally, and the
focused window of it vertically* — are statements about
the relation between two states of the ribbon, and no scalar spec can hold
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

## What the caller used to compute, and no longer does

An FTS condition compared one field against a constant or against a percentage
of one field; it could not add two fields. So the caller computed the
differences and passed them as fields — `left-slack`, `right-slack`,
`scroll-limit` and so on — and `fts/harness/derive.mjs` was the single table
saying which fields exist, in which order, and how each is computed.

In flang they are expressions inside the spec. `scroll-offset` and
`stack-offset` had thirteen fields each and have six; every spec's input now
equals the input of `layout-probe` character for character, and the table died
with the harness that held it. The rule it guarded still holds and needs no
guard any more: no `if`, no `min`, no `max` and no threshold lives outside the
spec, because the branching and the bounds are the policy, and the policy is
the spec.

## Where the language does not reach

Two places the FTS surface could not reach are gone. **Integer division** is
now `«Деление нацело»` in `integer-division.flang` — the C99 rule word for
word, truncation towards zero — so `column-width` answers with C's integer and
not with an exact fraction, and `window-height` divides the column by the
number of windows itself. **The `rule` field**, which the English FTS surface
turned into a rule declaration, is just a parameter name now.

Three places remain, and all three were measured rather than assumed.

**`число` cannot be narrowed to `нат`.** The specs declare their inputs as
`нат` — the domain is named by the type — and nothing in flang 0.7.10 turns a
computed `число` back into one: not a condition (`если значение не меньше 0 то
значение` in a function returning `нат` is FLANG_TYPE), not a precondition
(`требует значение не меньше 0` — the same refusal), not addition (`нат плюс
нат` is `число`). The one crossing the compiler accepts is a **record field** of
the declared type, and that one it does not check at all: `-7` goes into a `нат`
field in silence. `layout.flang` needs the crossing — it feeds a computed column
edge back into `«Смещение ленты после фокуса»` — and `«Мера»` there is it,
clamping negatives to zero itself, since nobody else will.

**The `en:` view is not compared with the main one.** The label is part of the
promise's name, so a view weakened to `результат не меньше -5` beside a main
`не меньше 0` passes `flang check` in silence. `tools/check-flang-en-views.sh`
is what compares them.

**The kernel has no "not less than a term".** Eight rules, and that shape is
not among them, so "width is at least the minimum" and "height is at least the
minimum" stay grids of examples rather than proofs no matter how the branches
are reordered.

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

The vectors were the same `fts/vectors/output-change.json` (the file still
existed then), and the utility is called by its Russian name in quotes exactly
as `conformance.mjs` called it — the name is part of the spec. The check also goes red: a copy of the model
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

### The models are across, 4 September 2026

All ten models now also live in `fts/flang/`, one file per model. The forecast
under "What a full move costs" has been checked by running it rather than by
reading it; where it held and where it did not is below.

**2 270 lines instead of 3 740.** "Roughly half" was the forecast; six tenths
is the answer, and the difference is not the language but the headers: the
"what changed against FTS and why" section runs thirty to seventy lines per
file, and dropping it would trade reading for counting. The policy itself
shrank harder than forecast: `scroll-offset` is 302 lines against 600,
`stack-offset` 308 against 616, and in both the seventeen rules fold into four
lines of arithmetic and one clamp.

**One field fewer for every derived one.** `scroll-offset` and `stack-offset`
had thirteen each and now have six — all seven that went were differences
computed by `derive.mjs`. Every model's input now equals the input of
`layout-probe` character for character.

**The kernel takes 11 promises of 25.** The forecast said "11 of 22", and the
count of proved ones matches exactly; the total is 25 because the port added
three — two `«Деление нацело»` equalities and "a strut of zero depth takes
nothing". In the ledger that is 22 proved claims out of 50 (every promise has
an `en:` variant, and the variant counts separately).

| model | promises | taken by the kernel |
|---|---|---|
| `insertion`, `strut-span` | 2 | 2 — the answer is built from literals |
| `strut-reserve` | 3 | 3 |
| `focus-after-close` | 2 | 1 — the second is taken FROM THE `нат` TYPE |
| `output-change`, `scroll-offset`, `stack-offset` | 2, 3, 3 | 1 each |
| `column-width`, `window-height`, `strut-pair` | 3, 3, 2 | 0 |

The reason for the zeros is named and measured: the kernel has eight rules,
among them "not greater than a term" and no "not LESS than a term" at all.
"Width is at least the minimum" and "height is at least the minimum" are goals
of exactly that shape, and no reordering of branches will take them.
`strut-pair` fails for another reason: its body is arithmetic in `пусть`
bindings rather than a tree of branches, and the kernel does not step inside a
`пусть`.

**The harness's only concession is no longer needed.** `truncate: true` on
`column-width` was there because FTS has no integer division and the model
answered with an exact fraction. `«Деление нацело»` repeats the C99 rule word
for word — truncation towards zero — and the model answers with the same
integer as `ribbon_policy_width()` on all nineteen vectors and on eighty random
inputs beyond them.

**★ The port found three false properties.** Under FTS a property is checked
only against the model's own examples; the vectors go past it. In flang a
promise is checked by the runtime after every return, examples included — and
three properties fell:

| model | `.fts` property | input where it is false | live cwm answers |
|---|---|---|---|
| `strut-pair` | "Nothing more than the whole region is given up" | `region-length=-10` — **a project vector** | 0, and `0 ≤ −10` is false |
| `strut-reserve` | "Nothing less than zero is taken" | `region-length=-5` | −5 |
| `window-height` | "Height is positive" | `viewport-height=0, min-height=0` | 0 |

The first matters most: that input has been sitting in
`fts/vectors/strut-pair.json` since 3 September and nobody caught it, because
`conformance.mjs` compares NUMBERS over the vectors and never asks about
properties. In `fts/flang/` all three promises carry the condition they were
silently resting on.

**What was still open that day.** The pin on `fts-pered-udaleniem` was still a
pin: the `.fts` files were not deleted, `derive.mjs` and the whole Node harness
read them, and there was no reason to retire 450 working checks. Whole
scenarios (`layout-probe layout`, the two insertion invariants) had not moved.
They were next, not the models — and the first half of that is what the next
section is about.

### The harness is gone too, the same day

Of 2 385 lines of Node in `fts/harness/`, **1 216 are left** — `invariants.mjs`,
`hotplug.mjs` and the `layout-probe` parser they share; 1 189 of them are the
code that was already there, and the other 27 are the headers saying why each
stayed. The `.fts` files, the `fts/vectors/*.json` and the frozen tag are gone
with the rest.

| file | was | now | what happened |
|---|---:|---:|---|
| `surfaces.mjs` | 142 | 0 | there is one file per spec; the `en:` view is guarded by `tools/check-flang-en-views.sh`, 32 lines of `awk` |
| `derive.mjs` | 353 | 0 | derived fields became expressions; the table had nothing left to hold |
| `conformance.mjs` | 471 | 0 | replaced by `conformance.flang` (scalars) and `layout.flang` (whole scenarios) |
| `selftest.mjs` | 223 | 0 | replaced by `tools/check-flang-mutants.sh`, which corrupts C's answer instead of the spec |
| `invariants.mjs` | 618 | 639 | stays: it is about the relation between two states of the ribbon, and no spec describes the ribbon as a value |
| `hotplug.mjs` | 367 | 382 | stays: three states of several ribbons at once, same reason |
| `probe.mjs` | 211 | 195 | stays for those two; `probeScalar()` and `probeLayout()` went with their callers |

**Coverage did not shrink, and the count fell anyway.** 450 became 215 because
450 counted every vector twice — once per language surface — plus ten checks
that a model's fields matched a table in the harness. There is one surface now,
and no table. What is compared is the same 201 scalar vectors and the same
whole scenarios, and the scenarios are compared harder: byte for byte against
the probe's own output instead of field by field against a list written by
hand.

**One thing had to change in the specs to make it possible.** `«Деление
нацело»` was written out twice, in `column-width.flang` and in
`window-height.flang`, and the copy was deliberate — a spec should read on its
own. It also made the two specs unusable together: any program importing both
fails with `FLANG_DUPLICATE_NAME`, and both `conformance.flang` and
`layout.flang` need both. The copy is now one file, `integer-division.flang`.

**What is still open.** `invariants.mjs` and `hotplug.mjs` are still Node, and
the reason is not the harness but the missing spec: nothing here describes the
ribbon, or a set of ribbons across outputs, as a value. Until something does,
there is no promise for those checks to belong to. Their headers say so.
