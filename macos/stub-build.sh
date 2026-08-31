#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - согласовать слой Accessibility API там, где его нет
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
# То же, что делает tools/macos-flicker/stub-build.sh для axcost.c, и по той
# же причине: мака нет, «оно соберётся» проверить нечем, а согласованность
# кода с тем, ЧЕМ МЫ СЧИТАЕМ Accessibility API, - проверить можно.
# Компилятор сверяет macos/wsi_ax.m с заглушкой: опечатка в имени,
# перепутанный аргумент, чужая подпись у callback, CFTypeRef вместо
# AXUIElementRef - всё это падает здесь.
#
# Отличие от соседнего скрипта одно, и оно важное: заглушка Accessibility API
# НЕ переписывается заново, а ВЫРЕЗАЕТСЯ из tools/macos-flicker/stub-build.sh.
# Две копии одного представления о чужом API разошлись бы молча, и тогда
# «согласовано» перестало бы что-либо значить. Скрипт проверяет, что вырезал
# именно её, и падает, если соседний файл переписали.
#
# Сверх вырезанного объявляется ровно то, чего в нём нет, - и каждое такое
# имя названо ниже поимённо, с указанием, откуда взято. Список повторён в
# шапке macos/wsi_ax.m и в doc/macos.md, чтобы владельцу было что проверять
# на первом же маке.
#
# Чего это НЕ доказывает: что заглушка верна. Она - наше представление о
# чужом API, и врать может она, а не код. На маке заглушка не нужна: там
# `make -C macos` собирает то же самое против настоящих заголовков.
#
# Запуск: sh macos/stub-build.sh

set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
work=${TMPDIR:-/tmp}/digitwm-axport.$$
CC=${CC:-clang}

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

command -v "$CC" >/dev/null 2>&1 || {
	echo "нет $CC: Objective-C сверяет только clang" >&2
	exit 1
}

mkdir -p "$work/stub/ApplicationServices" "$work/stub/AppKit"

# 1. Вырезать заглушку Accessibility API из соседнего скрипта - ту самую, с
# которой axcost.c согласуется с тех пор, как был написан.
src="$root/tools/macos-flicker/stub-build.sh"
[ -f "$src" ] || { echo "нет $src" >&2; exit 1; }

awk '
/^cat > "\$work\/stub\/ApplicationServices\/ApplicationServices.h" <<.EOF.$/ {
	inside = 1; next
}
inside && /^EOF$/ { exit }
inside { print }
' "$src" > "$work/ax-common.h"

grep -q 'AXUIElementCreateApplication' "$work/ax-common.h" || {
	echo "не удалось вырезать заглушку из $src" >&2
	echo "Скорее всего его переписали; поправьте выборку здесь." >&2
	exit 1
}
common=$(wc -l < "$work/ax-common.h")
echo "заглушка Accessibility API взята из tools/macos-flicker/stub-build.sh:"
echo "  $common строк, не копия - вырезка"

# 2. Снять закрывающий #endif и дописать то, чего в общей заглушке нет.
sed '$ { /^#endif$/d; }' "$work/ax-common.h" \
    > "$work/stub/ApplicationServices/ApplicationServices.h"

cat >> "$work/stub/ApplicationServices/ApplicationServices.h" <<'EOF'

/*
 * Сверх общей заглушки. Каждое имя - с источником; тот же список в шапке
 * macos/wsi_ax.m.
 *
 * Класс [2] - имя и место названы в doc/portability.md, список аргументов
 * НЕТ; подпись ниже - наша догадка, и на маке она проверяется компилятором
 * первой же сборкой:
 */
extern AXError	 AXUIElementIsAttributeSettable(AXUIElementRef, CFStringRef,
		     Boolean *);			/* AXUIElement.h:204 */
extern AXError	 AXUIElementPerformAction(AXUIElementRef, CFStringRef);
					/* yabai src/window_manager.c:1324 */
extern const CFStringRef kAXRaiseAction;	/* там же */
/* AXNotificationConstants.h:57,113,123,133,194 - имена, не значения: */
extern const CFStringRef kAXUIElementDestroyedNotification;
extern const CFStringRef kAXWindowMovedNotification;
extern const CFStringRef kAXWindowResizedNotification;
extern const CFStringRef kAXFocusedWindowChangedNotification;

/*
 * Класс [3] - в дереве не подтверждено ничем; написано по общему знанию API,
 * то есть ровно то, что имеет право оказаться неверным:
 */
extern Boolean	 CFEqual(CFTypeRef, CFTypeRef);
extern AXError	 AXObserverRemoveNotification(AXObserverRef, AXUIElementRef,
		     CFStringRef);
extern const CFStringRef kAXMainAttribute;
extern const CFStringRef kAXFrontmostAttribute;
#endif
EOF

# 3. Заглушка AppKit. Здесь класса [1] нет вовсе: доказательная база AppKit в
# дереве - две строки doc/portability.md (visibleFrame, NSEvent.mouseLocation)
# и ссылка на yabai для NSWorkspace. Всё остальное - класс [3].
cat > "$work/stub/AppKit/AppKit.h" <<'EOF'
#ifndef STUB_APPKIT_H
#define STUB_APPKIT_H
/*
 * Не Apple SDK. Написано ради одной проверки: согласован ли macos/wsi_ax.m с
 * тем, чем мы считаем AppKit.
 */
#include <ApplicationServices/ApplicationServices.h>
#include <stddef.h>
#include <sys/types.h>

typedef unsigned long	 NSUInteger;
typedef long		 NSInteger;
typedef CGPoint		 NSPoint;
typedef CGSize		 NSSize;
typedef struct { NSPoint origin; NSSize size; } NSRect;

@interface NSObject
@end

@interface NSString : NSObject
- (const char *)UTF8String;
@end

@interface NSArray : NSObject
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)index;
@end

/* doc/portability.md: visibleFrame - «Apple documentation». Остальное - [3]. */
@interface NSScreen : NSObject
+ (NSArray *)screens;
- (NSRect)frame;
- (NSRect)visibleFrame;
- (NSString *)localizedName;
@end

/* doc/portability.md:95 и doc/macos.md:76. */
@interface NSEvent : NSObject
+ (NSPoint)mouseLocation;
@end

typedef NSInteger NSApplicationActivationPolicy;
enum { NSApplicationActivationPolicyRegular = 0 };

@interface NSRunningApplication : NSObject
- (pid_t)processIdentifier;
- (NSApplicationActivationPolicy)activationPolicy;
@end

/* yabai src/workspace.m:157-180 - способ, но не селекторы: [3]. */
@interface NSWorkspace : NSObject
+ (NSWorkspace *)sharedWorkspace;
- (NSArray *)runningApplications;
@end
#endif
EOF

# 4. Сама сверка. -Wno-objc-root-class - потому что в заглушке NSObject
# объявлен без базового класса: настоящий тоже корневой, но настоящий - в
# SDK, которого здесь нет.
echo
echo "macos/wsi_ax.m против заглушек ApplicationServices и AppKit:"
$CC -fsyntax-only -x objective-c -std=c99 -D_POSIX_C_SOURCE=200809L \
	-Wall -Wextra -Werror -Wno-objc-root-class \
	-I"$work/stub" -I"$root/macos" "$here/wsi_ax.m"
echo "  согласован (-Wall -Wextra -Werror)"
echo
echo "Что это значит: код согласован с нашим представлением об API - имена,"
echo "типы, порядок аргументов, подпись AXObserverCallback."
echo "Что это НЕ значит: что представление верно. Классы [2] и [3] в шапке"
echo "macos/wsi_ax.m - это и есть список того, что проверяется только на маке."
echo
echo "На маке:  cc -Wall -Wextra -fobjc-arc -c macos/wsi_ax.m \\"
echo "              -framework ApplicationServices -framework AppKit"
