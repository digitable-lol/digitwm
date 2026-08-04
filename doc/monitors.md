# More than one monitor

**Русская версия: [monitors.ru.md](monitors.ru.md).**

Every RandR output gets **its own ribbon**, with its own columns, its own
length and its own offset. Nothing is ever moved from one ribbon to another, so
two monitors never end up sharing a row of windows.

## Bound by name, not by number

A ribbon is bound to the **name** of the output — `HDMI-1`, `DP-2`, `eDP-1` —
because the name is what survives a cable being pulled. The CRTC index is not:
unplug the middle monitor of three and the numbers behind the remaining two can
change.

`screen_update_geometry()` names each region after its output, falling back to
`crtc<N>` only when RandR gives no name at all; `ribbon_screen_relayout()` looks
its ribbon up by that name and makes a new one only when the name is new.

## A monitor that went away

Its ribbon is marked detached (`active = 0`) and **keeps every column it had**.
The windows on it are parked — hidden with `client_hide()` and flagged
`CLIENT_RIBBON_PARKED`, so the ribbon knows which windows are its own to bring
back and does not un-hide a window that some group hid.

Plug the monitor back in and the ribbon comes back as it was: same columns,
same widths, same stacks, same offset. Nothing had to be saved, because nothing
was thrown away.

If the monitor comes back **at a different resolution**, the columns are
re-measured against the new viewport — their widths are presets, a percentage
of the viewport, not pixels — and the offset is pulled back inside the new
ribbon length by `ribbon_policy_output`, the same policy the `output-change`
model describes.

## What is proved, and how

`fts/harness/hotplug.mjs` drives seven scenarios through the window manager's
own code and checks three things: a detached ribbon loses no columns, no window
crosses from one ribbon to another, and a monitor that returns unchanged
restores its ribbon to the pixel.

```sh
node fts/harness/hotplug.mjs --wm ./cwm
node fts/harness/hotplug.mjs --wm ./cwm --selfcheck
```

The event is fed in where the `RRScreenChangeNotify` handler feeds it:
`ribbon_screen_update()` was split in two, and everything that is arithmetic —
which ribbons are attached, what became of their viewports and offsets — lives
in `ribbon_screen_relayout()`, which touches no X function. `layout-probe
outputs` sets the outputs up the way `xrandr` prints them and replays a hotplug
through it:

```sh
./cwm -C 'layout-probe outputs \
    outputs=HDMI-1:1920x1080+0+0,DP-1:1280x800+1920+0 \
    columns=HDMI-1:2.1.3,DP-1:1.1 focus=HDMI-1:2,DP-1:1 \
    then=HDMI-1:1920x1080+0+0 \
    after=HDMI-1:1920x1080+0+0,DP-1:1280x800+1920+0'
```

`--selfcheck` breaks four things in an answer the window manager actually gave
— columns lost from a detached ribbon, a window crossing over, a ribbon coming
back with a different offset, a detached ribbon still calling itself attached —
and requires each to be caught by the check it was aimed at.

## What is not proved

**A real hotplug has not been run.** It takes a second physical output or an X
server that can bring one and take it away; Xvfb cannot, and there is no second
monitor on the machine this was written on. What is checked is exactly what the
ribbon computes when the event arrives — not what the X server does around it,
and not that the event arrives at all on your hardware.

The path from the event to the arithmetic is three lines of `xev_handle_randr`
(`XRRUpdateConfiguration`, `screen_update_geometry`, `ribbon_screen_update`),
inherited from cwm and unchanged. That is a reason to expect it to work, not
evidence that it does.
