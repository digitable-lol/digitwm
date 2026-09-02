#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm - собрать проверяемую половину macOS-порта и сверить её с двоичным
# файлом
#
# Copyright (c) 2026 Digitable <https://digitable.life>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Порт на macOS разрезан надвое ровно по одной линии - macos/wsi_platform.h.
# Выше линии (macos/wsi_core.c) обычный C: учёт окон, пометка собственных
# выдач геометрии, отбрасывание собственных извещений - все одиннадцать
# операций wsi.h. Ниже (macos/wsi_ax.m) - Accessibility API, которого на этой
# машине нет.
#
# Этот скрипт проверяет ВСЁ, что выше линии, и делает это тем же приёмом, что
# tools/wasm-layout/check.mjs: один и тот же вопрос задаётся двоичному файлу
# (`cwm -C 'layout-probe layout ...'`) и ленте, собранной поверх macOS-порта,
# и ответы сравниваются по числам. Расхождение хоть в одном - и порт не
# реализует контракт.
#
# Заглушки заголовков X11 - те же, что в tools/no-x-build.sh, и по той же
# причине: ribbon.c включает calmwm.h, а тот - Xlib. Ни одно имя X11 в
# собранный харнесс не попадает, и скрипт это проверяет: если бы попало,
# половина порта, которую мы объявили независимой от оконной системы,
# зависела бы от X11.
#
# Запуск:  sh macos/check.sh [число случаев]
# Выход:   0 - лента над macOS-портом считает то же, что лента над X11.

set -e

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/digitwm-macos.$$
CC=${CC:-cc}
cases=${1:-400}

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

# Заглушка заголовков X11 - не здесь, а в macos/fakex.sh: её же подставляет
# macos/Makefile, собирая порт на маке, где X11 нет вовсе. Один вымысел о
# чужих заголовках, одно место.
sh "$root/macos/fakex.sh" "$work/fakex"

inc="-I$work/fakex -I$root -I$root/macos"
warn="-Wall -Wextra -Werror"

# 1. Лента - та же самая, дословно, и собранная без единой функции X11.
printf 'лента (ribbon.c) без Xlib:      '
(cd "$root" && $CC -Wall -Werror=implicit-function-declaration -O2 -g \
    -D_GNU_SOURCE $inc -c ribbon.c -o "$work/ribbon.o")
echo "собралась"

# 2. Часть (а) macOS-порта. -Werror - потому что это наш код, а не чужой.
printf 'порт: wsi_core.c wsi_fake.c:    '
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/wsi_core.c \
    -o "$work/wsi_core.o")
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/wsi_fake.c \
    -o "$work/wsi_fake.o")
echo "собрались ($warn)"

printf 'харнесс: wsicheck.c:            '
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/wsicheck.c \
    -o "$work/wsicheck.o")
(cd "$root" && $CC -Wall -O2 -g -D_GNU_SOURCE $inc -c strlcpy.c \
    -o "$work/strlcpy.o")
(cd "$root" && $CC -Wall -O2 -g -D_GNU_SOURCE $inc -c strlcat.c \
    -o "$work/strlcat.o")
$CC -o "$root/macos/wsicheck" "$work/ribbon.o" "$work/wsi_core.o" \
    "$work/wsi_fake.o" "$work/wsicheck.o" "$work/strlcpy.o" "$work/strlcat.o"
echo "собрался"

# Часть (б): то, что стоит между лентой и человеком, - последовательность
# запуска, чтение cwmrc, разбор нажатой клавиши. Те же самые файлы, что
# собираются в двоичный файл на маке; клавиатурный слой подделан харнессом,
# потому что настоящий - это Carbon.
printf 'порт: wsi_conf.c wsi_run.c:     '
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c confpath.c \
    -o "$work/confpath.o")
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/wsi_conf.c \
    -o "$work/wsi_conf.o")
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/wsi_run.c \
    -o "$work/wsi_run.o")
echo "собрались ($warn)"

printf 'точка входа: wsi_main.c:        '
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/wsi_main.c \
    -o "$work/wsi_main.o")
echo "собралась ($warn)"

printf 'харнесс: runcheck.c:            '
(cd "$root" && $CC -Wall -O2 -g -D_GNU_SOURCE $inc -c xmalloc.c \
    -o "$work/xmalloc.o")
# reallocarray.c - вместе с xmalloc.c и только с ним: xmalloc.c зовёт
# reallocarray(), и на Linux эту функцию даёт glibc (с 2.26), а на macOS её нет
# ни в libSystem, ни где-либо ещё - оттого и лежит в дереве своя копия из
# OpenBSD, которую macos/Makefile собирает в двоичный файл всегда. Здесь её не
# было, и линковка молча держалась на glibc; первый же прогон этого скрипта на
# маке встал на «Undefined symbols: _reallocarray». Скрипт, который на одной
# системе проверяет, а на другой не собирается, проверяет не то, что обещает.
(cd "$root" && $CC -Wall -O2 -g -D_GNU_SOURCE $inc -c reallocarray.c \
    -o "$work/reallocarray.o")
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -c macos/runcheck.c \
    -o "$work/runcheck.o")
$CC -o "$root/macos/runcheck" "$work/ribbon.o" "$work/wsi_core.o" \
    "$work/wsi_fake.o" "$work/wsi_conf.o" "$work/confpath.o" "$work/wsi_run.o" \
    "$work/runcheck.o" "$work/xmalloc.o" "$work/strlcpy.o" "$work/strlcat.o" \
    "$work/reallocarray.o"
echo "собрался"

# И полная сборка двоичного файла - того самого, с настоящим main(), - со
# всеми объектными файлами, которые собирает macos/Makefile на маке, кроме
# двух: wsi_ax.m и wsi_key.m тут заменены оконной системой из памяти и
# подделкой клавиатуры из харнесса. Это не «собралось на маке» и не выдаёт
# себя за него; это доказательство, что набор объектных файлов полон и
# сходится - то единственное в сборке на маке, что здесь проверить можно.
printf 'digitwm целиком (mac-цель, кроме двух .m):  '
(cd "$root" && $CC $warn -O2 -g -D_GNU_SOURCE $inc -Dmain=runcheck_unused \
    -c macos/runcheck.c -o "$work/runfakes.o")
$CC -o "$work/digitwm-fake" "$work/wsi_main.o" "$work/ribbon.o" \
    "$work/wsi_core.o" "$work/wsi_conf.o" "$work/confpath.o" "$work/wsi_run.o" \
    "$work/wsi_fake.o" "$work/runfakes.o" "$work/xmalloc.o" \
    "$work/strlcpy.o" "$work/strlcat.o" "$work/reallocarray.o"
echo "слинковался"
printf '  и отвечает: '
"$work/digitwm-fake" -k | head -1

# И тем же двоичным файлом - порядок поиска настроек. Скрипт один на две
# сборки: та же таблица случаев прогоняется по ./cwm в tools/ и по mac-цели
# здесь, и если два порядка разойдутся, разойдутся и два прогона одной
# таблицы. Отрицательный контроль скрипта (--selfcheck) гоняется там же, где
# и он сам, - в CI, - а не здесь: тут проверяется двоичный файл, а не скрипт.
echo
sh "$root/tools/check-config-order.sh" "$work/digitwm-fake"

# 3. Ни одного имени X11 во всём, что собрано. Это не украшение: половина
# порта, объявленная независимой от оконной системы, обязана быть независимой
# и от той, из-под которой мы её проверяем.
x_syms=$(nm -u "$work/ribbon.o" "$work/wsi_core.o" "$work/wsi_fake.o" \
    "$work/wsicheck.o" "$work/wsi_conf.o" "$work/confpath.o" "$work/wsi_run.o" \
    "$work/wsi_main.o" "$work/runcheck.o" | awk '{print $NF}' \
    | grep -E '^_?X' | LC_ALL=C sort -u || true)
if [ -n "$x_syms" ]; then
	echo "порт требует имён X11, а должен требовать ноль:" >&2
	echo "$x_syms" | sed 's/^/  /' >&2
	exit 1
fi
echo "  имён X11 во всех объектных файлах: ноль"
echo

# 4. Сама сверка. Двоичный файл нужен настоящий - тот, что собирает Makefile.
if [ ! -x "$root/cwm" ]; then
	echo "нет $root/cwm - соберите его: make" >&2
	exit 1
fi

cd "$root" && ./macos/wsicheck --wm ./cwm --cases "$cases"

# 5. И то, чего в сверке выше нет вовсе: запуск, настройки, клавиши.
echo
./macos/runcheck
