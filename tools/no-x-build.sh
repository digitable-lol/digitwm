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
# Скрипт отвечает на второй, и с тех пор как шов проведён, ответ обязан быть
# «нисколько». Заглушка вместо заголовков X11 не объявляет НИ ОДНОЙ функции
# X11: если лента позовёт хоть одну, сборка встанет на неизвестном имени и
# назовёт строку. Всё, что лента просит у оконной системы, перечислено в
# wsi.h - и третья проверка требует, чтобы список в заголовке и список в
# объектном файле совпадали, иначе контракт молча оброс бы вызовами, о
# которых порт не знает.
#
# Последний проход отделяет десять политик - чистую арифметику - в собственную
# единицу трансляции и требует, чтобы она собралась с -Werror и не имела ни
# одного неопределённого имени, кроме Conf. Это и есть та часть, которую любой
# порт унесёт с собой дословно.
#
# Запуск:  sh tools/no-x-build.sh
# Выход:   0 - шов цел; 1 - лента снова знает про оконную систему больше, чем
#          записано в wsi.h.

set -e

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/digitwm-nox.$$
CC=${CC:-cc}

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

mkdir -p "$work/fakex/X11/Xft" "$work/fakex/X11/extensions"

# Заглушка вместо Xlib. Типы - потому что через calmwm.h их видит любой файл
# дерева; функций нет ни одной, и в этом всё утверждение. Раньше здесь стояли
# XSync() и XCheckMaskEvent(): их звал ribbon_settle(), теперь это wsi_settle()
# в xutil.c, по ту сторону шва.
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

# 1. Вся лента - против заглушки, в которой нет ни одной функции X11.
printf 'ribbon.c без Xlib: '
(cd "$root" && $CC -Wall -Werror=implicit-function-declaration -O2 -D_GNU_SOURCE \
    -I"$work/fakex" -c ribbon.c -o "$work/ribbon.o") || {
	echo "НЕ СОБРАЛАСЬ"
	echo "Лента позвала функцию X11 - строка названа выше." >&2
	echo "Шов проведён в wsi.h: вызов принадлежит механике (xutil.c)," >&2
	echo "а лента зовёт операцию контракта." >&2
	exit 1
}
echo "собралась"

# 2. Ни одного имени X11 в объектном файле. Ноль - не «мало», а условие:
# лента, которой нужен хоть один X-символ, не соберётся на macOS вообще.
# Шаблон шире, чем X[A-Z]: Xft*, XRR* и внутренние _X* тоже имена X11.
x_syms=$(nm -u "$work/ribbon.o" | awk '{print $NF}' | grep -E '^_?X' | LC_ALL=C sort || true)
if [ -n "$x_syms" ]; then
	echo "ribbon.o требует имён X11, а должен требовать ноль:" >&2
	echo "$x_syms" | sed 's/^/  /' >&2
	echo "Вынесите вызов за шов - в xutil.c - и объявите операцию в wsi.h." >&2
	exit 1
fi
echo "  имён X11 в ribbon.o: ноль"

# 3. Всё, что лента берёт у оконной системы, объявлено в wsi.h - и наоборот.
#
# Без этой проверки шов держался бы на одном слове «X11»: новый вызов
# client_*, добавленный в ленту мимо контракта, не дал бы ни одного имени X11
# и прошёл бы молча, а порт на macOS узнал бы о нём на этапе компоновки.
#
# LC_ALL=C у sort - не украшение, а условие сравнения: в русской локали
# подчёркивание сортируется иначе, списки перестают совпадать, и скрипт
# объявляет поломку на дереве, где не изменилось ничего. В CI локаль C,
# поэтому отказ ловился бы только на машине разработчика.
wsi_decl=$(sed -n 's/^[a-z].*[ 	*]\([a-z_][a-z_0-9]*\)(.*);$/\1/p' "$root/wsi.h" \
    | LC_ALL=C sort -u)
[ -n "$wsi_decl" ] || { echo "в wsi.h не найдено ни одного объявления" >&2; exit 1; }

ws_syms=$(nm -u "$work/ribbon.o" | awk '{print $NF}' \
    | grep -E '^(client_|region_|screen_|xu_|wsi_)' | LC_ALL=C sort -u || true)
missing=
for sym in $ws_syms; do
	echo "$wsi_decl" | grep -qx "$sym" || missing="$missing $sym"
done
if [ -n "$missing" ]; then
	echo "Лента зовёт оконную систему мимо контракта:" >&2
	for sym in $missing; do echo "  $sym - нет в wsi.h" >&2; done
	echo "Либо впишите операцию в wsi.h - с обещанием и с тем, чего от неё" >&2
	echo "НЕ требуется, - либо не зовите её из ленты." >&2
	exit 1
fi
nws=$(echo "$ws_syms" | wc -w)
echo "  операций оконной системы: $nws, все объявлены в wsi.h"

unused=
for sym in $wsi_decl; do
	echo "$ws_syms" | grep -qx "$sym" || unused="$unused $sym"
done
[ -n "$unused" ] && {
	echo "  ВНИМАНИЕ: в wsi.h объявлено, но лентой не зовётся:$unused" >&2
	echo "  контракт шире, чем нужно - порт заплатит за лишнее." >&2
}

# 4. Десять политик - отдельно, с -Werror и без единого заголовка X11.
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

undef=$(nm -u "$work/policy.o" | awk '{print $NF}' | LC_ALL=C sort | tr '\n' ' ')
undef=${undef% }
if [ "$undef" != "Conf" ]; then
	echo "Политики перестали быть чистой арифметикой." >&2
	echo "Неопределённые имена: $undef (ожидалось только Conf)" >&2
	exit 1
fi
echo "  неопределённых имён: Conf - и больше ничего"

echo
echo "Лента не знает про X11 ничего; всё, что она просит у оконной системы,"
echo "записано в wsi.h. Цена переноса - механика, а не арифметика."
echo "См. doc/portability.ru.md."
