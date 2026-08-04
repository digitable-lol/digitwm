# Сборка, установка, запуск

**English version: [build.md](build.md).**

## Что нужно

Компилятор C, `make` (годятся и BSD, и GNU), `yacc` или `bison`, `pkg-config` и
заголовки трёх библиотек: `x11`, `xft`, `xrandr`. Это весь список, и он тот же,
что у cwm: лента не добавила ни одной зависимости — именно это оставляет
достижимым NetBSD.

**Node.js в списке нет.** Модели раскладки из `fts/` проверяются в CI и внутри
оконного менеджера не исполняются никогда.

## Руками

```sh
make
make install PREFIX=$HOME/.local    # bin/cwm, man1/cwm.1, man5/cwmrc.5
```

`PREFIX`, `DESTDIR` и `MANPREFIX` уважаются. Двоичный файл называется `cwm`, а
не `digitwm`: так `cwmrc` и всё написанное про cwm переносится без правок — чего
это стоит, разобрано в [pkgsrc/README.ru.md](../pkgsrc/README.ru.md).

## Одной командой, вместе с окружением

```sh
sh bootstrap.sh --plan     # посмотреть, что будет сделано, ничего не меняя
sh bootstrap.sh            # зависимости, сборка, установка, потом сессия
session/verify.sh          # проверить, что получилось
```

`bootstrap.sh` ставит то, *чем* digitwm собирается — компилятор, yacc, три
набора заголовков X, — и передаёт работу `session/install.sh`, который
раскладывает окружение: редактор, терминал, оболочку, мультиплексор, темы
Digitable Focus и агента Digit. Разделение намеренное: сборочные зависимости
ставятся от root пакетным менеджером, а сессия раскладывается в домашнем
каталоге и root ей не нужен вовсе.

| Система | Пакетный менеджер | Что ставится |
|---|---|---|
| Arch | `pacman` | `base-devel libx11 libxft libxrandr bison pkgconf` |
| Debian, Ubuntu | `apt` | `build-essential libx11-dev libxft-dev libxrandr-dev bison pkg-config` |
| Fedora | `dnf` | `gcc make libX11-devel libXft-devel libXrandr-devel bison pkgconf-pkg-config` |
| openSUSE | `zypper` | те же имена, как их пишет openSUSE |
| FreeBSD | `pkg` | `libX11 libXft libXrandr bison pkgconf` |
| NetBSD | `pkgin` | `bison pkgconf` — X11 уже в базовой системе |
| OpenBSD | — | ничего: X11 и yacc в базе |
| macOS | — | отказ с объяснением: оконному менеджеру X11 там нечем управлять |

`--no-session` останавливается на оконном менеджере; `--no-packages` не трогает
пакетный менеджер вовсе; всё остальное уходит в `session/install.sh` как есть
(`--palette`, `--skip-install`, `--no-rc`, `--yes`).

Инструментарий уже стоит? `bootstrap.sh` спрашивает `pkg-config`, а не базу
пакетов, и пропускает установку, когда заголовки на месте, — чтобы не просить
пароль там, где он не нужен.

**Что прогнано на самом деле:** план и путь «собрать и установить», на
Debian/Ubuntu, в пустой префикс. Установка пакетов на Arch, FreeBSD и NetBSD
написана по их именам пакетов и здесь не исполнялась; `--plan` печатает точную
команду, чтобы её можно было прочитать до запуска.

## Пакетом

`pkgsrc/wm/digitwm` — пакет pkgsrc для одного оконного менеджера; в каком он
состоянии и чего в нём не хватает до первого релизного тега, написано в
[pkgsrc/README.ru.md](../pkgsrc/README.ru.md).

## Запуск

```sh
exec digitwm-session          # то, что session/install.sh кладёт в ~/.xinitrc
exec cwm                      # только оконный менеджер
```

`digitwm.desktop` ставится в `~/.local/share/xsessions`, поэтому менеджер
входа предложит сессию по имени.

Конфигурация лежит в `~/.config/digitwm/cwmrc` (туда её кладёт сессия и
запускает `cwm -c` с ней) либо в `~/.cwmrc`. Все настройки описаны в
`cwmrc(5)`; собственные настройки ленты собраны в [ribbon.ru.md](ribbon.ru.md).

## Как проверить правку

```sh
make                                    # сначала оно обязано собраться

git clone https://github.com/digitable-lol/fts ../fts
(cd ../fts && npm ci && npm run build)

for m in fts/*.fts; do node ../fts/dist/src/cli.js check "$m" >/dev/null; done
for m in fts/*.fts; do node ../fts/dist/src/cli.js test  "$m" >/dev/null; done

node fts/harness/surfaces.mjs    --fts ../fts
node fts/harness/conformance.mjs --fts ../fts --wm ./cwm
node fts/harness/selftest.mjs    --fts ../fts --wm ./cwm
node fts/harness/invariants.mjs  --wm ./cwm
node fts/harness/invariants.mjs  --wm ./cwm --selfcheck
node fts/harness/hotplug.mjs     --wm ./cwm
node fts/harness/hotplug.mjs     --wm ./cwm --selfcheck
```

Ровно это и в том же порядке гоняет
`.github/workflows/fts-conformance.yml`. Что доказывает каждая проверка —
в [`fts/README.ru.md`](../fts/README.ru.md); в каком порядке вносится правка
в раскладку — в [CONTRIBUTING.ru.md](../CONTRIBUTING.ru.md).

`tools/measure-offscreen.sh` в этот набор не входит: ему нужны `Xvfb` и
`xdotool`, он идёт минуты, а не секунды, и отвечает на вопрос о времени, а не о
правильности ([offscreen.ru.md](offscreen.ru.md)).
