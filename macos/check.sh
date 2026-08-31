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

mkdir -p "$work/fakex/X11/Xft" "$work/fakex/X11/extensions"

cat > "$work/fakex/X11/Xlib.h" <<'EOF'
#ifndef FAKE_XLIB_H
#define FAKE_XLIB_H
typedef unsigned long Window, Colormap, Atom, Cursor, Time, Pixmap, KeySym, XID;
typedef struct _Display Display;
typedef struct { int dummy; } Visual;
typedef union _XEvent { int type; long pad[24]; } XEvent;
typedef struct { char *res_name, *res_class; } XClassHint;
typedef struct { long flags; int x, y, width, height, min_width, min_height,
    max_width, max_height, width_inc, height_inc, base_width, base_height,
    win_gravity; struct { int x, y; } min_aspect, max_aspect; } XSizeHints;
#define False 0
#define True 1
#endif
EOF
for h in XKBlib.h Xatom.h Xproto.h Xutil.h cursorfont.h keysym.h; do
	echo '#include <X11/Xlib.h>' > "$work/fakex/X11/$h"
done
cat > "$work/fakex/X11/Xft/Xft.h" <<'EOF'
#include <X11/Xlib.h>
typedef struct { int height, ascent, descent; } XftFont;
typedef struct { int dummy; } XftColor;
typedef struct _XftDraw XftDraw;
EOF
echo '#include <X11/Xlib.h>' > "$work/fakex/X11/extensions/Xrandr.h"

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

# 3. Ни одного имени X11 во всём, что собрано. Это не украшение: половина
# порта, объявленная независимой от оконной системы, обязана быть независимой
# и от той, из-под которой мы её проверяем.
x_syms=$(nm -u "$work/ribbon.o" "$work/wsi_core.o" "$work/wsi_fake.o" \
    "$work/wsicheck.o" | awk '{print $NF}' | grep -E '^_?X' | LC_ALL=C sort -u \
    || true)
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
