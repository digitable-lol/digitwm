# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Собирается только на macOS: тут два фреймворка, которых больше нигде нет.
# На Linux проверяется другое и другим - sh stub-build.sh.

CC?=		cc
CFLAGS?=	-Wall -Wextra -O2

all: axcost flicker

axcost: axcost.c
	${CC} ${CFLAGS} -o axcost axcost.c \
		-framework ApplicationServices -framework CoreFoundation

flicker: flicker.m
	${CC} ${CFLAGS} -fobjc-arc -o flicker flicker.m -framework Cocoa

clean:
	rm -f axcost flicker

.PHONY: all clean
