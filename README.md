# digitwm

A scrollable-tiling window manager for X11, forked from
[cwm](https://github.com/leahneukirchen/cwm) — the calm window manager from the
OpenBSD base system.

**Русская версия: [README.ru.md](README.ru.md).**

## What it is

Windows live on an endless horizontal ribbon of columns. The screen is a
viewport onto that ribbon: opening a window pushes the ribbon, it never squeezes
the neighbours into unreadable slivers. Focus moves the viewport; the ribbon
keeps its shape.

If that sounds like [niri](https://github.com/YaLTeR/niri), it is the same idea.
niri is a Wayland compositor. digitwm brings the layout model to X11, where it
can run on Linux, FreeBSD and NetBSD alike.

## What it is not

**It is not a compositor**, so it does not animate. Animation needs a process
that owns the frame, and an X11 window manager does not. Windows move in one
step. If you want fades and slides, run a compositor beside it.

**It is not a fork of sdorfehs or of papersway.** Those are GPL; digitwm is ISC,
and it stays that way so patches can flow back to cwm and so the code can be
used without conditions. See [NOTICE](NOTICE).

## Why cwm as the base

cwm is small (7 883 lines of C), depends on three libraries (`x11`, `xft`,
`xrandr`), has been maintained in OpenBSD base for two decades, and — the part
that matters here — **has no tiling model to tear out**. It is a floating window
manager, so the ribbon is added onto clean ground rather than grafted over
someone else's frame tree.

## Layout rules are executable specifications

The numbers that drive the layout — how far the ribbon shifts after a focus
change, how wide a column is, where a new window is inserted, what gets focus
when a window closes — are not buried in C. They are written as
[FTS](https://github.com/the-homeless-god/fts) models under `fts/`, in Russian
and in English, with worked examples on the boundaries.

The loop over columns stays in C, because a loop is mechanical. The numbers the
loop substitutes are policy, and policy belongs somewhere it can be read and
tested.

**FTS never runs inside the window manager.** It runs in CI: the same vectors go
through the generated code and through the live window manager, and a mismatch
fails the build. No runtime dependency on Node, which is what keeps NetBSD
reachable.

## Status

Early. The fork is in place with upstream history preserved; the ribbon is being
implemented. Nothing here is ready for daily use yet.

## Licence

ISC, inherited from cwm and applied to new code as well. Full text in
[LICENSE](LICENSE); provenance and the one BSD-3 exception in [NOTICE](NOTICE).

Part of [Digitable](https://digitable.life).
