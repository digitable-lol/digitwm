#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# Сколько X11 в арифметике ленты - проверка сборкой, а не чтением.
#
# Вопрос «можно ли перенести digitwm на систему без X11» распадается на два, и
# только второй интересен:
#
#   1. сколько кода зовёт Xlib напрямую - это видно грепом;
#   2. сколько кода *не соберётся* без Xlib - а это видно только компилятору.
#
# Скрипт отвечает на второй. Он подсовывает вместо заголовков X11 заглушку,
# в которой перечислено всё, что ribbon.c имеет право оттуда взять, и собирает
# ribbon.c против неё. Если сборка проходит - зависимость ленты от X11 равна
# ровно списку в заглушке. Если кто-то добавит в ленту вызов Xlib, которого в
# заглушке нет, скрипт упадёт на неизвестном имени и назовёт строку.
#
# Второй проход отделяет десять политик - чистую арифметику - в собственную
# единицу трансляции и требует, чтобы она собралась с -Werror и не имела ни
# одного неопределённого имени, кроме Conf. Это и есть та часть, которую любой
# порт унесёт с собой дословно.
#
# Запуск:  sh tools/no-x-build.sh
# Выход:   0 - зависимость та же, что записана здесь; 1 - изменилась.

set -e

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/digitwm-nox.$$
CC=${CC:-cc}

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

mkdir -p "$work/fakex/X11/Xft" "$work/fakex/X11/extensions"

# Заглушка вместо Xlib. Каждое имя здесь - это имя, которое лента действительно
# берёт у X11; список короткий, и в этом всё утверждение.
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
#define EnterWindowMask (1L << 4)
extern int XSync(Display *, int);
extern int XCheckMaskEvent(Display *, long, XEvent *);
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

# 1. Вся лента - против заглушки.
printf 'ribbon.c без Xlib: '
(cd "$root" && $CC -Wall -Werror=implicit-function-declaration -O2 -D_GNU_SOURCE \
    -I"$work/fakex" -c ribbon.c -o "$work/ribbon.o") || {
	echo "НЕ СОБРАЛАСЬ"
	echo "Лента взяла у X11 имя, которого нет в заглушке выше." >&2
	echo "Либо верните вызов в механику, либо впишите имя сюда - осознанно." >&2
	exit 1
}
echo "собралась"

x_syms=$(nm -u "$work/ribbon.o" | awk '{print $NF}' | grep -E '^(X[A-Z]|X_Dpy)' | sort || true)
expect_x="XCheckMaskEvent
XSync
X_Dpy"
if [ "$x_syms" != "$expect_x" ]; then
	echo "Имена X11, которых требует ribbon.o, изменились:" >&2
	echo "$x_syms" | sed 's/^/  /' >&2
	echo "Ожидались три, все три - внутри ribbon_settle():" >&2
	echo "$expect_x" | sed 's/^/  /' >&2
	exit 1
fi
echo "  имена X11 в ribbon.o: XSync, XCheckMaskEvent, X_Dpy - все в ribbon_settle()"

# 2. Девять политик - отдельно, с -Werror и без единого заголовка X11.
cat > "$work/shim.h" <<'EOF'
#ifndef SHIM_H
#define SHIM_H
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define RIBBON_NPRESET		4
#define RIBBON_PLACE_COLUMN	0
#define RIBBON_PLACE_STACK	1
#define RIBBON_PLACE_FLOAT	2
#define RIBBON_PLACE_FULL	3
#define RIBBON_RULE_NONE	0
#define RIBBON_RULE_STACK	1
#define RIBBON_RULE_FLOAT	2
struct conf_shim { int ribbonwidth[RIBBON_NPRESET]; };
extern struct conf_shim Conf;
int ribbon_policy_offset(int, int, int, int, int, int);
int ribbon_policy_voffset(int, int, int, int, int, int);
int ribbon_policy_width(int, int, int, int);
int ribbon_policy_height(int, int, int, int, int);
int ribbon_policy_insert(int, int, int, int, int, int);
int ribbon_policy_close(int, int, int, int);
int ribbon_policy_output(int, int, int);
int ribbon_policy_span(int, int, int, int);
int ribbon_policy_reserve(int, int, int, int, int);
#endif
EOF

# Выкусываем ровно функции ribbon_policy_* вместе со строкой типа над именем.
echo '#include "shim.h"' > "$work/policy.c"
awk '
/^ribbon_policy_[a-z]*\(/ { emit = 1; print prev }
emit { print }
/^\}/ { emit = 0 }
{ prev = $0 }
' "$root/ribbon.c" >> "$work/policy.c"

nfn=$(grep -c '^ribbon_policy_[a-z]*(' "$work/policy.c")
[ "$nfn" -eq 10 ] || {
	echo "выкушено $nfn политик вместо десяти - изменился набор ribbon_policy_*" >&2
	exit 1
}

printf 'десять политик отдельной единицей: '
(cd "$work" && $CC -Wall -Wextra -Werror -O2 -c policy.c -o policy.o) || {
	echo "НЕ СОБРАЛИСЬ"
	exit 1
}
echo "собрались (-Wall -Wextra -Werror, ни одного заголовка X11)"

undef=$(nm -u "$work/policy.o" | awk '{print $NF}' | sort | tr '\n' ' ')
undef=${undef% }
if [ "$undef" != "Conf" ]; then
	echo "Политики перестали быть чистой арифметикой." >&2
	echo "Неопределённые имена: $undef (ожидалось только Conf)" >&2
	exit 1
fi
echo "  неопределённых имён: Conf - и больше ничего"

echo
echo "Арифметика ленты не зависит от X11; механика зависит целиком."
echo "Цена переноса - вторая, а не первая. См. doc/portability.ru.md."
