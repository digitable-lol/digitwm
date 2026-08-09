# The browser: what of this can be shown for real

**Русская версия: [browser.ru.md](browser.ru.md).**

The owner's question is short: "can digitwm run in a browser". The answer splits
in two, and the two must not be mixed up.

The first — "can a visitor drive real windows of real programs" — is no, and
below is what that rests on and why the refusal has not changed. The second —
"can a visitor watch the ribbon itself decide, rather than a re-telling of it" —
is yes, it is done, and its price is a number.

## What was refused, and why

**An x86 emulator in the browser (v86).** 2.45 MB of runtime, an image from
20 MB, 27.6 s to the first frame, 883 MB of guest memory — a phone kills the tab
first. And decisively: v86 takes no 64-bit kernels, and our binary is x86-64.

**A remote desktop (Xpra, noVNC).** Refused on security, not on speed. Inside
one X session a client reads its neighbours' input and takes their screenshots,
and a window manager launches programs by its very nature. Putting that on the
internet hands out a shell, not a picture.

**Re-telling the ribbon in JavaScript.** Refused as a fake. The page stands on
what it shows being real; a second implementation of the same rules proves only
that they can be written twice.

`/digitwm/` currently carries a recording of a live session. It stays: nothing
below replaces it.

## What changed since, and why the refusal stands

Checked again in August 2026, and one thing did change.

**v86 still has no 64 bits.** Its README says what it said before: *"Linux
works pretty well. 64-bit kernels are not supported"*, with "64-bit extensions"
among the missing CPU features ([copy/v86](https://github.com/copy/v86)). Our
binary is x86-64, so that road is closed by word size, not by speed.

**WebVM has grown graphics, and that is a real change.** Its README now offers
*"Try out the new Alpine / Xorg / i3 graphical environment"*
([leaningtech/webvm](https://github.com/leaningtech/webvm)). X11 in a browser
exists in 2026 and is not a theory.

And the refusal now rests on that same project, for a different reason than
before — a licensing one. WebVM itself is Apache 2.0, but CheerpX, the engine
under it, is not: *"The public CheerpX deployment is provided as-is and is free
to use for technological exploration, testing and use by individuals"*, while
*"any other use by organizations, including non-profit, academia and the public
sector, requires a license"*, and separately *"downloading a CheerpX build for
the purpose of hosting it elsewhere is not permitted without a commercial
license"*. A product page is use by an organization; so it is either buy a
licence or serve someone else's runtime from someone else's CDN on our own
page. For a project whose licence contour has a gate of its own
(`tools/check-licensing.py`, [NOTICE](../NOTICE)), that is not paperwork.

What we did **not** check, and do not pass off as checked: whether CheerpX
takes 64-bit binaries. Its documentation names no word size either way; until
it does, nothing can be planned on it.

The remote desktop was not reconsidered and is not open to reconsideration for
the same reason as before: it was about security, not about technique.

## What was built: digitwm decides, the page draws

There is no need to emulate a machine, because the ribbon does not need one.
That was already proved for the sake of macOS, and by the same means:
`tools/no-x-build.sh` shows the ribbon's arithmetic takes nothing from X11, and
its entire contract with the outside world is ten functions out of
`nm -u ribbon.o`.

So `ribbon.c` compiles straight to WebAssembly and the page writes the ten:

```sh
sh tools/wasm-layout/build.sh          # build the module
node tools/wasm-layout/check.mjs       # prove it is the same ribbon
```

| What | How much |
|---|---|
| `layout.wasm` | **10 111 bytes** |
| the same, gzipped | 4 699 bytes |
| the whole page over the wire (html + wasm, gzipped) | **7 446 bytes** |
| page open → ribbon laid out, Chrome, 6 runs | best **609 ms**, median ~1 550 ms, worst 4 203 ms |
| of that, bringing the module up | 200–1 310 ms |
| bringing the module up in Node, 15 runs | median 0.54 ms, worst 285 ms |
| building a ribbon of eight windows | 0.3–0.5 ms |
| for comparison: the v86 runtime | 2 450 000 bytes |
| for comparison: v86 to the first frame | 27 600 ms |

The spread is not about the module but about the machine: the load average on
eight cores was 52, the browser started in the same queue as everything else,
and `doc/baseline.md` keeps the same rule — write the load down rather than
hide it behind a median. What actually belongs to the ribbon is the bottom row:
the layout itself is a fraction of a millisecond, and the rest of the time the
browser spends on itself. The v86 figures were taken on another machine by
another method, and are best read as an order of magnitude rather than a race.

Those columns may only be compared while remembering what makes them different,
and that is the point of this document.

**What is real.** `ribbon.c` — the same file that builds into the `cwm` binary,
not one line changed. Every decision — where a window lands, how wide a column
is, where the viewport scrolls, who gets the focus after a close — is made by
it. This is not a claim but a check: `check.mjs` puts the same question to the
binary (`cwm -C 'layout-probe layout ...'`, the way CI does) and to the module,
and compares the numbers. **2000 cases, 17 842 windows, not one mismatch.**

**What is not real.** The windows. A browser has neither applications nor an X
server; a window on that page is a rectangle. The page must say so in its first
line, or it becomes exactly the fake that got the JavaScript re-telling refused.
The wording that holds: **digitwm decides, the page draws.**

The difference between this and the recording is not honesty but what each one
shows. The recording shows real windows of real programs and lets nobody touch
them. The module lets you touch it and shows real decisions about unreal
windows. Neither of the two is "digitwm in a browser", and that is how it has
to be written.

## What it costs to keep

The module is built from the same sources as the binary and therefore cannot
fall behind it quietly: `check.mjs` fails if the answers differ in a single
number. That is the device the FTS models live by, at the same price — one step
in the build.

The module has no libc: `-nostdlib`, and everything the ribbon wants from one is
four functions written in `tools/wasm-layout/shim.c`. The heap is one array
with no freeing, which is enough for a page that lives for minutes, and which is
said there outright.

## What it does not solve

Real windows in a browser stay impossible for the same reasons as before, and
none of them is about the speed of our code. The refusal of the remote desktop
is about security, and it stands. The refusal of emulation is about weight and
about word size.
