#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - заглушка заголовков X11, одна на всех, кому она нужна
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
# calmwm.h включает Xlib ради ЧЕТЫРЁХ ТИПОВ в описаниях структур - Window,
# Colormap, Visual, XftDraw - и ни ради одного вызова.  Разрезать calmwm.h -
# отдельная работа (doc/macos.md, «два места, где контракт и macOS не
# встречаются», пункт 2), и до неё всякий, кто собирает ленту или
# macos/wsi_core.c без X11, подставляет вместо заголовков эти двадцать строк.
#
# Таких мест стало три: macos/check.sh (проверка здесь), macos/Makefile
# (сборка на маке, где X11 нет вовсе) и tools/no-x-build.sh (свой набор, чуть
# уже).  Две копии одного вымысла о чужих заголовках разошлись бы молча -
# поэтому копий нет, есть этот скрипт, и оба первых его зовут.
#
# Запуск:  sh macos/fakex.sh <каталог>
# Выход:   <каталог>/X11/... - подставляется через -I<каталог>

set -e

dir=${1:?укажите каталог, куда положить заглушку}

mkdir -p "$dir/X11/Xft" "$dir/X11/extensions"

cat > "$dir/X11/Xlib.h" <<'EOF'
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
	echo '#include <X11/Xlib.h>' > "$dir/X11/$h"
done

cat > "$dir/X11/Xft/Xft.h" <<'EOF'
#include <X11/Xlib.h>
typedef struct { int height, ascent, descent; } XftFont;
typedef struct { int dummy; } XftColor;
typedef struct _XftDraw XftDraw;
EOF

echo '#include <X11/Xlib.h>' > "$dir/X11/extensions/Xrandr.h"
