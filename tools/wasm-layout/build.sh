#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm - собрать ленту в WebAssembly и назвать её вес
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
# Вопрос «можно ли digitwm в браузере» до сих пор упирался в эмуляцию целой
# машины: 2,45 МБ рантайма, образ от 20 МБ, 27,6 с до первой картинки, 883 МБ
# памяти на гостя - и всё это ради 64-битного двоичного файла, которого
# эмулятор всё равно не берёт.
#
# Здесь другой путь и другая цена.  Эмулировать машину не нужно, потому что
# лента в машине не нуждается: tools/no-x-build.sh уже показал, что она не
# берёт у X11 ни одного имени, а весь договор с оконной системой записан в
# wsi.h - одиннадцать операций.  Значит ribbon.c компилируется прямо в
# WebAssembly, а эти одиннадцать пишет страница (tools/wasm-layout/shim.c).
#
# Что при этом остаётся настоящим и что нет - сказано в шапке shim.c, и это
# главное, что нельзя переврать: считает digitwm, рисует страница.
#
# Заглушки заголовков X11 - те же, что в tools/no-x-build.sh, и по той же
# причине: ribbon.c включает calmwm.h, а тот - Xlib.  Ни одно имя оттуда в
# собранный модуль не попадает, что скрипт и проверяет.
#
# Запуск: sh tools/wasm-layout/build.sh

set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
work=${TMPDIR:-/tmp}/digitwm-wasm.$$
CC=${CC:-clang}

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

command -v "$CC" >/dev/null 2>&1 || { echo "нет $CC" >&2; exit 1; }

mkdir -p "$work/inc/X11/Xft" "$work/inc/X11/extensions" "$work/inc/sys"

cat > "$work/inc/X11/Xlib.h" <<'EOF'
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
#define EnterWindowMask (1L << 4)
extern int XSync(Display *, int);
extern int XCheckMaskEvent(Display *, long, XEvent *);
#endif
EOF
for h in XKBlib.h Xatom.h Xproto.h Xutil.h cursorfont.h keysym.h; do
	echo '#include <X11/Xlib.h>' > "$work/inc/X11/$h"
done
cat > "$work/inc/X11/Xft/Xft.h" <<'EOF'
#include <X11/Xlib.h>
typedef struct { int height, ascent, descent; } XftFont;
typedef struct { int dummy; } XftColor;
typedef struct _XftDraw XftDraw;
EOF
echo '#include <X11/Xlib.h>' > "$work/inc/X11/extensions/Xrandr.h"

# libc здесь нет вовсе, и это не бедность, а условие: всё, что ленте от него
# нужно, - четыре функции, и они написаны в shim.c.  Заголовки - только
# объявления, чтобы calmwm.h и ribbon.c собрались.
: > "$work/inc/sys/param.h"
: > "$work/inc/unistd.h"
: > "$work/inc/errno.h"
cat > "$work/inc/sys/types.h" <<'EOF'
#ifndef FAKE_TYPES_H
#define FAKE_TYPES_H
#include <stddef.h>
#endif
EOF
cat > "$work/inc/stdio.h" <<'EOF'
#ifndef FAKE_STDIO_H
#define FAKE_STDIO_H
#include <stdarg.h>
typedef struct _FILE FILE;
#endif
EOF
cat > "$work/inc/stdlib.h" <<'EOF'
#ifndef FAKE_STDLIB_H
#define FAKE_STDLIB_H
#include <stddef.h>
extern void free(void *);
#endif
EOF
cat > "$work/inc/string.h" <<'EOF'
#ifndef FAKE_STRING_H
#define FAKE_STRING_H
#include <stddef.h>
extern int strcmp(const char *, const char *);
extern size_t strlen(const char *);
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
#endif
EOF
cat > "$work/inc/err.h" <<'EOF'
#ifndef FAKE_ERR_H
#define FAKE_ERR_H
extern void warnx(const char *, ...);
extern void errx(int, const char *, ...);
#endif
EOF

wasmflags="--target=wasm32 -O2 -nostdlib -ffreestanding -fno-stack-protector -fvisibility=hidden -Wall"

printf 'ribbon.c в WebAssembly: '
(cd "$root" && $CC $wasmflags -I"$work/inc" -c ribbon.c -o "$work/ribbon.o")
echo "собралась"

printf 'механика для страницы:   '
(cd "$root" && $CC $wasmflags -I"$work/inc" -I"$root" -c \
    tools/wasm-layout/shim.c -o "$work/shim.o")
echo "собралась"

printf 'сборка модуля:           '
$CC --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-dynamic \
	-Wl,--allow-undefined -Wl,--export-memory -Wl,--initial-memory=2097152 \
	-o "$here/layout.wasm" "$work/ribbon.o" "$work/shim.o"
echo "layout.wasm готов"

size=$(wc -c < "$here/layout.wasm")
gz=$(gzip -9 -c "$here/layout.wasm" | wc -c)
elf=$(wc -c < "$root/cwm" 2>/dev/null || echo 0)

echo
echo "== вес"
printf '  layout.wasm              %8d байт\n' "$size"
printf '  он же, gzip -9           %8d байт\n' "$gz"
printf '  для сравнения, cwm (ELF) %8d байт\n' "$elf"
echo
echo "Сравнивать это с 2,45 МБ рантайма эмулятора и образом от 20 МБ можно"
echo "только помня, чем они отличаются: эмулятор запускал бы НАСТОЯЩИЕ окна"
echo "настоящих программ, а здесь настоящая только арифметика."
