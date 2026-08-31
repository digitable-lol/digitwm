#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm - собрать замерщик macOS там, где macOS нет
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
# Мака у нас нет, и «оно соберётся» - утверждение, которое без мака проверить
# нельзя.  Проверить можно другое, и это не то же самое: что axcost.c
# согласован с тем, чем мы считаем Accessibility API.  Заглушка ниже
# переписана из заголовков SDK - имена, порядок и типы аргументов, что
# возвращается.  Компилятор сверяет с ней наш код: опечатка в имени,
# перепутанный аргумент, чужая подпись у callback - всё это здесь падает.
#
# Чего это НЕ доказывает: что заглушка верна.  Она - наше представление о
# чужом API, и врать может она, а не код.  На маке заглушка не нужна - там
# та же команда собирается против настоящих заголовков и отвечает уже на
# первый вопрос; в этом и смысл: проверка одна, а не две.
#
# Запуск: sh tools/macos-flicker/stub-build.sh

set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=${TMPDIR:-/tmp}/digitwm-axstub.$$
CC=${CC:-cc}

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

mkdir -p "$work/stub/ApplicationServices"

cat > "$work/stub/ApplicationServices/ApplicationServices.h" <<'EOF'
#ifndef STUB_APPLICATIONSERVICES_H
#define STUB_APPLICATIONSERVICES_H
/*
 * Не Apple SDK.  Переписано из CoreFoundation.h, AXUIElement.h, AXValue.h,
 * AXAttributeConstants.h, AXNotificationConstants.h ради одной проверки:
 * согласован ли axcost.c с тем, чем мы считаем это API.
 */
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef unsigned char		 Boolean;
typedef long			 CFIndex;
typedef double			 CFTimeInterval;
typedef int32_t			 SInt32;
typedef double			 CGFloat;
typedef unsigned long		 CFTypeID;
typedef unsigned int		 CFStringEncoding;

typedef const void		*CFTypeRef;
typedef const struct __CFString	*CFStringRef;
typedef const struct __CFArray	*CFArrayRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFBoolean *CFBooleanRef;
typedef struct __CFAllocator	*CFAllocatorRef;
typedef struct __CFRunLoop	*CFRunLoopRef;
typedef struct __CFRunLoopSource *CFRunLoopSourceRef;

typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;

extern void		 CFRelease(CFTypeRef);
extern CFTypeRef	 CFRetain(CFTypeRef);
extern CFTypeID		 CFGetTypeID(CFTypeRef);
extern CFTypeID		 CFStringGetTypeID(void);
extern CFTypeID		 CFArrayGetTypeID(void);
extern CFIndex		 CFArrayGetCount(CFArrayRef);
extern const void	*CFArrayGetValueAtIndex(CFArrayRef, CFIndex);
extern Boolean		 CFStringGetCString(CFStringRef, char *, CFIndex,
			     CFStringEncoding);
#define kCFStringEncodingUTF8 0x08000100

struct CFDictionaryKeyCallBacks { CFIndex version; void *pad[6]; };
struct CFDictionaryValueCallBacks { CFIndex version; void *pad[4]; };
typedef struct CFDictionaryKeyCallBacks CFDictionaryKeyCallBacks;
typedef struct CFDictionaryValueCallBacks CFDictionaryValueCallBacks;
extern const CFDictionaryKeyCallBacks	 kCFTypeDictionaryKeyCallBacks;
extern const CFDictionaryValueCallBacks	 kCFTypeDictionaryValueCallBacks;
extern CFDictionaryRef	 CFDictionaryCreate(CFAllocatorRef, const void **,
			     const void **, CFIndex,
			     const CFDictionaryKeyCallBacks *,
			     const CFDictionaryValueCallBacks *);
extern const CFBooleanRef kCFBooleanTrue;

extern CFRunLoopRef	 CFRunLoopGetCurrent(void);
extern void		 CFRunLoopAddSource(CFRunLoopRef, CFRunLoopSourceRef,
			     CFStringRef);
extern SInt32		 CFRunLoopRunInMode(CFStringRef, CFTimeInterval,
			     Boolean);
extern const CFStringRef kCFRunLoopDefaultMode;

typedef struct __AXUIElement	*AXUIElementRef;
typedef struct __AXValue		*AXValueRef;
typedef struct __AXObserver	*AXObserverRef;

typedef enum { kAXErrorSuccess = 0, kAXErrorFailure = -25200 } AXError;
typedef enum {
	kAXValueTypeCGPoint = 1,
	kAXValueTypeCGSize = 2,
	kAXValueTypeCGRect = 3
} AXValueType;

typedef void (*AXObserverCallback)(AXObserverRef, AXUIElementRef, CFStringRef,
    void *);

extern AXUIElementRef	 AXUIElementCreateApplication(pid_t);
extern AXError		 AXUIElementCopyAttributeValue(AXUIElementRef,
			     CFStringRef, CFTypeRef *);
extern AXError		 AXUIElementSetAttributeValue(AXUIElementRef,
			     CFStringRef, CFTypeRef);
extern AXError		 AXUIElementSetMessagingTimeout(AXUIElementRef, float);
extern AXValueRef	 AXValueCreate(AXValueType, const void *);
extern Boolean		 AXValueGetValue(AXValueRef, AXValueType, void *);
extern AXError		 AXObserverCreate(pid_t, AXObserverCallback,
			     AXObserverRef *);
extern AXError		 AXObserverAddNotification(AXObserverRef,
			     AXUIElementRef, CFStringRef, void *);
extern CFRunLoopSourceRef AXObserverGetRunLoopSource(AXObserverRef);
extern Boolean		 AXIsProcessTrustedWithOptions(CFDictionaryRef);
extern const CFStringRef kAXTrustedCheckOptionPrompt;

extern const CFStringRef kAXPositionAttribute;
extern const CFStringRef kAXSizeAttribute;
extern const CFStringRef kAXTitleAttribute;
extern const CFStringRef kAXWindowsAttribute;
extern const CFStringRef kAXFocusedWindowAttribute;
extern const CFStringRef kAXWindowCreatedNotification;
#endif
EOF

echo "axcost.c против заглушки Accessibility API:"
${CC} -fsyntax-only -Wall -Wextra -Werror -std=c99 -D_POSIX_C_SOURCE=200809L \
	-I"$work/stub" "$here/axcost.c"
echo "  собралась (-Wall -Wextra -Werror), заглушка полная: ни одного имени"
echo "  сверх перечисленных в ней axcost.c не берёт"
echo
echo "Что это значит: код согласован с нашим представлением об API."
echo "Что это НЕ значит: что представление верно.  Это проверяется на маке -"
echo "той же командой без -I заглушки, и там же появляются числа."
