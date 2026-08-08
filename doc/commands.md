# Every command, and what the ribbon did to it

**Русская версия: [commands.ru.md](commands.ru.md).**

`DGT-WM-09` asked for an inventory of the roughly forty frame commands of
`actions.c` and for a verdict on virtual screens, written down one command at a
time. The premise changed under it, and this document says how before it
answers.

## The premise that changed

The first edition of the specification proposed forking **sdorfehs**, and
`actions.c` is a file of that project. digitwm is not that fork: the base was
changed to **cwm** before any code was taken (`fd9c10a`, `c19566e`), because
sdorfehs and ratpoison are GPL-2.0-or-later and one borrowed function would
have relicensed the whole tree — see [NOTICE](../NOTICE).

So there is no `actions.c` here and never was, and **the sdorfehs command list
is not reproduced in this document either**. Reading someone else's code to
understand an idea is fine; copying their strings into our tree is the thing the
licence contour exists to prevent, and a table of forty names is a copy of the
part that was worth copying.

What is inventoried instead is the surface digitwm actually has: **120 commands**
in the table in `conf.c`, every one of them either inherited from cwm, added by
the ribbon, or left standing while the ribbon changed what it means. The
question `DGT-WM-09` really asks — *which commands still make sense once windows
live on a ribbon* — is answered for each group below.

## Added by the ribbon — 11

| Command | What it does |
|---|---|
| `ribbon-focus-left`, `ribbon-focus-right` | move focus to the neighbouring column; the viewport follows |
| `ribbon-focus-up`, `ribbon-focus-down` | move focus inside the column's stack; the canvas scrolls to the window |
| `ribbon-move-left`, `ribbon-move-right` | carry the window to the neighbouring column, making one at the edge if there is none |
| `ribbon-width-cycle`, `ribbon-width-grow`, `ribbon-width-shrink` | step the column through the four width presets |
| `ribbon-center` | put the focus in the middle of the viewport, on both axes |
| `ribbon-float-toggle` | drop a window out of the ribbon, or pick a floating one up into it |

Default bindings are on `Mod4` (`4-h`, `4-l`, `4-k`, `4-j`, `4S-h`, `4S-l`,
`4-r`, `4-equal`, `4-minus`, `4-c`), which is free in upstream cwm — nothing
inherited was rebound.

## Inherited and untouched — 84

They mean on a ribbon exactly what they mean in cwm, and are listed by family
rather than one by one because nothing was decided about them:

- **groups** (24): `group-toggle-N`, `group-only-N`, `group-close-N`,
  `group-cycle`, `group-rcycle`, `group-last`, `group-toggle-all`,
  `window-movetogroup-N`, `window-group`, `window-stick`;
- **cycling** (6): `window-cycle`, `window-rcycle` and their `-ingroup` and
  `-inclass` forms;
- **menus** (7): `menu-cmd`, `menu-group`, `menu-ssh`, `menu-window`,
  `menu-window-hidden`, `menu-exec`, `menu-exec-wm`;
- **the pointer** (8): `pointer-move-*` and their `-big` forms;
- **stacking and life** (6): `window-lower`, `window-raise`, `window-hide`,
  `window-close`, `window-delete`, `window-menu-label`;
- **the session** (4): `terminal`, `lock`, `restart`, `quit`.

## Left standing, with a changed meaning — 25

These are the ones a tiling layout usually deletes. digitwm keeps them, because
the ribbon is not a mode you are locked into: a window can leave it with
`ribbon-float-toggle` and every one of these commands then means what it always
did. **On a window that is on the ribbon** they behave as follows, and this is a
decision, not an oversight:

| Family | Commands | On the ribbon |
|---|---|---|
| move | `window-move`, `window-move-{up,down,left,right}`, and the four `-big` | The window moves, and the next event that touches the ribbon puts it back. Geometry on the ribbon belongs to the ribbon: `ribbon_place()` writes it, `client_config()` answers a client's own `ConfigureRequest` with what it actually has, as ICCCM requires of a denied request. |
| resize | `window-resize`, `window-resize-{up,down,left,right}`, and the four `-big` | The same. A column's width is a preset, and the command for it is `ribbon-width-grow`. |
| snap | `window-snap-center` and the eight directions | The same. Snapping to a screen edge is a floating-layout idea; on the ribbon the answer is `ribbon-center`. |
| tile | `window-htile`, `window-vtile` | The same. cwm's tiling arranges a window's group around the screen; the ribbon arranges columns. Two layouts do not compose, and the ribbon wins because it wrote last. |
| maximise | `window-maximize`, `window-vmaximize`, `window-hmaximize` | The same, with one exception below. |
| freeze | `window-freeze` | **The escape hatch, and the exception.** A frozen window keeps the geometry it was given and holds its slot in the column, so the stack does not shuffle under it (`ribbon_place()` checks `CLIENT_FREEZE`). Freeze first, then move or maximise, and it sticks. |
| fullscreen | `window-fullscreen` | Honoured. A fullscreen window keeps its slot and its geometry the same way a frozen one does; EWMH fullscreen is a request from the client, not a layout choice, and refusing it would break video players and browsers. |

**Nothing was removed.** Removing a command would have been the smaller change
to write and the larger one to defend: every one of them is reachable the moment
a window is floated, and a window manager that deletes cwm's commands is no
longer a cwm your configuration file understands.

## Virtual screens

sdorfehs and ratpoison have virtual screens. cwm has **groups**, nine of them,
and digitwm keeps groups and does not add virtual screens. The reasons are in
the code rather than in taste:

- a group is a set of windows shown or hidden together; a ribbon is a row bound
  to a **RandR output**. They are different axes, and both work at once: a
  window belongs to a group and stands in a column, and hiding its group does
  not disturb the column;
- the ribbon never un-hides what it did not hide. `ribbon_sync_one()` marks a
  window it parked with `CLIENT_RIBBON_PARKED` and only ever brings back windows
  carrying that mark, so a window hidden because its group is hidden stays
  hidden. Without that flag the two mechanisms would fight over `client_hide()`;
- a second ribbon per output already exists (see [monitors.md](monitors.md)).
  Virtual screens would be a third axis over the same windows, and the ribbon
  itself — endless, scrolled, cheap to extend — is what removes the need for
  them: on a ribbon you make a new column, you do not switch to another desktop.

**Verdict: no virtual screens.** Groups stay as cwm wrote them.

## Configuration files of sdorfehs

**Not compatible, and deliberately so.** digitwm reads `cwmrc`; its grammar,
its command names and its binding syntax are cwm's. `migrate-config.pl` in this
tree is upstream's migrator for *old cwm* configuration files (`bind` →
`bind-key`, `move` → `window-move`), inherited with the fork; it has nothing to
do with sdorfehs and was not extended to it.

Reading a foreign configuration language would mean carrying that project's
vocabulary — and its ideas about frames — into a tree that has neither. The
migration path from sdorfehs is the documented set of commands above, not a
translation layer.
