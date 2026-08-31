# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Собирается только на macOS: здесь два фреймворка, которых больше нигде нет.
# На Linux проверяется другое и другим:
#
#   sh macos/stub-build.sh   - согласован ли wsi_ax.m с Accessibility API
#   sh macos/check.sh        - та же ли раскладка у ленты поверх порта
#
# Собирается ровно один файл, и это не скромность, а граница. wsi_ax.m лежит
# НИЖЕ macos/wsi_platform.h и потому не включает calmwm.h - ему нечего знать
# ни о ленте, ни о client_ctx, ни об X11. Всё, что выше линии
# (macos/wsi_core.c), включает calmwm.h, а тот включает Xlib ради типов
# Window, Colormap, Visual и XftDraw в описаниях структур - имён X11 в
# объектном файле от этого не появляется (tools/no-x-build.sh это и
# проверяет), но заголовки нужны. Пока calmwm.h не разрезан, wsi_core.c
# собирается против семнадцати строк заглушки - той же, что в
# tools/no-x-build.sh и в macos/check.sh. Разрез calmwm.h - следующая работа,
# и она не здесь.

CC?=		cc
CFLAGS?=	-Wall -Wextra -O2

all: wsi_ax.o

wsi_ax.o: wsi_ax.m wsi_platform.h
	${CC} ${CFLAGS} -fobjc-arc -c wsi_ax.m \
		-framework ApplicationServices -framework AppKit

clean:
	rm -f wsi_ax.o wsicheck

.PHONY: all clean
