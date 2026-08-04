# Layout policy as an executable specification

**Русская версия: [README.ru.md](README.ru.md).**

Six scalar decisions drive the ribbon — how far it scrolls after a focus
change, how wide a column is, how tall a window in it is, where a new window
goes, what takes focus when a window closes, what happens to the offset when
the monitor changes size. Each is an [FTS](https://github.com/digitable-lol/fts)
model here, on both surfaces, with checked properties and worked examples on
the boundaries.

| Model | C function | `layout-probe` utility |
|---|---|---|
| `scroll-offset` | `ribbon_policy_offset` | `scroll-offset`, `«Смещение ленты после фокуса»` |
| `column-width` | `ribbon_policy_width` | `column-width`, `«Ширина колонки по пресету»` |
| `window-height` | `ribbon_policy_height` | `window-height`, `«Высота окна в колонке»` |
| `insertion` | `ribbon_policy_insert` | `insertion`, `«Куда вставить окно»` |
| `focus-after-close` | `ribbon_policy_close` | `focus-after-close`, `«Фокус после закрытия»` |
| `output-change` | `ribbon_policy_output` | `output-change`, `«Смещение после смены монитора»` |

`name.fts` is the Russian surface and `name.en.fts` the English one. They are
not translations of each other: one parser, one canonical document, and CI
proves it by comparing the two canonical JSONs field by field.

**FTS never runs inside the window manager.** It runs in CI. digitwm builds
with a C compiler, `yacc` and three X libraries on Linux, FreeBSD and NetBSD;
nothing here adds a runtime dependency on Node.

## Running it

```sh
git clone https://github.com/digitable-lol/fts ../fts
(cd ../fts && npm ci && npm run build)
make

for m in fts/*.fts; do node ../fts/dist/src/cli.js check "$m" >/dev/null; done
for m in fts/*.fts; do node ../fts/dist/src/cli.js test  "$m" >/dev/null; done

node fts/harness/surfaces.mjs   --fts ../fts
node fts/harness/conformance.mjs --fts ../fts --wm ./cwm
node fts/harness/selftest.mjs    --fts ../fts --wm ./cwm
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
