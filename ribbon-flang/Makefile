# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Арифметика ленты на flang: печать библиотеки flang-ribbon в C для digitwm.
#
# ── ЧТО ЗДЕСЬ ЛЕЖИТ ─────────────────────────────────────────────────────────
# flang-ribbon/ - ПОДМОДУЛЬ, клон github.com/digitable-lol/flang-ribbon на
#                 закреплённом отпечатке. Исходник библиотеки НЕ скопирован
#                 сюда, а подключён: `git submodule status` называет и адрес,
#                 и отпечаток, а `git -C flang-ribbon log` показывает, откуда
#                 взялась каждая строка.
# out-c/        - ПЕЧАТАЕТСЯ, а не правится руками, и при этом коммитится.
#                 Довод целиком - в README.md рядом; коротко: digitwm
#                 собирается компилятором C, yacc и тремя библиотеками X, и
#                 добавить к этому списку компилятор flang значило бы отрезать
#                 NetBSD и всякого, кто просто клонировал дерево.
#
# ── ПРАВКА ЗДЕСЬ - ДЕФЕКТ, А НЕ ПРАВКА ──────────────────────────────────────
# `make emit` начинается с `rm -rf out-c` и сотрёт её молча. Меняется
# поведение ленты - меняется библиотека в своём репозитории, потом здесь
# переставляется отпечаток подмодуля, потом печать, и всё это идёт одним
# коммитом. Что дерево не разошлось с подмодулем, проверяет `make verify`:
# перепечатывает во временный каталог и сравнивает побайтно.
#
# ── ПОЧЕМУ РАНТАЙМ БЕРЁТСЯ У geometry ───────────────────────────────────────
# `flang emit` кладёт рантайм рядом с КАЖДЫМ модулем, и четыре копии
# различаются ровно двумя вещами: именем модуля в шапке-комментарии и
# `#define FL_MAX_ARGS` - шириной самого широкого вызова модуля. Рантайм в
# сборке один на всех, и FL_MAX_ARGS обязан покрывать самый широкий вызов
# из четырёх: это «Ширина колонки по пресету» с восемью доводами, то есть
# geometry. Тот же выбор и по той же причине делает сверка библиотеки
# (flang-ribbon/tools/compare.sh).
#
# ── КАК ОБНОВИТЬ БИБЛИОТЕКУ ─────────────────────────────────────────────────
#   git -C ribbon-flang/flang-ribbon fetch
#   git -C ribbon-flang/flang-ribbon checkout <отпечаток>
#   make -C ribbon-flang            # проверка и печать
#   sh tools/check-ribbon-flang.sh  # сверка с эталоном: побайтно
#   git add ribbon-flang            # новый отпечаток и новая печать вместе
#
#   make -C ribbon-flang check      # flang check + flang test по модулям
#   make -C ribbon-flang emit       # печать в C и сборка напечатанного
#   make -C ribbon-flang verify     # перепечатать во временное и сравнить
#   make -C ribbon-flang clean      # убрать напечатанное

ifdef FLANG_HOME
FLANG ?= $(FLANG_HOME)/bootstrap/flang
else
FLANG ?= flang
endif

CC ?= cc
LIB = flang-ribbon
MODULES = viewport geometry placement strut

# Модуль, у которого берётся общий рантайм. См. шапку.
RUNTIME_FROM = geometry

.PHONY: all check emit verify clean

all: check emit

# `check` - разбор, типы, завершаемость и постусловия; `test` - примеры.
# Порознь, потому что порознь и падают: у них разные причины.
check:
	@for m in $(MODULES); do \
	  echo "== $$m"; \
	  $(FLANG) check $(LIB)/flang/$$m.flang || exit 1; \
	  $(FLANG) test  $(LIB)/flang/$$m.flang || exit 1; \
	done

emit:
	rm -rf out-c .emit
	@mkdir -p out-c
	@for m in $(MODULES); do \
	  echo "== печать $$m"; \
	  $(FLANG) emit $(LIB)/flang/$$m.flang --target c --out .emit/$$m || exit 1; \
	  cp .emit/$$m/$$m.c .emit/$$m/$$m.h out-c/ || exit 1; \
	done
	cp .emit/$(RUNTIME_FROM)/flang_runtime.c .emit/$(RUNTIME_FROM)/flang_runtime.h out-c/
	rm -rf .emit
	@echo "== сборка напечатанного"
	@for f in out-c/*.c; do \
	  $(CC) -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iout-c -c "$$f" -o /dev/null || exit 1; \
	done
	@echo "напечатано и собрано: $(words $(MODULES)) модуля плюс общий рантайм"

# Расхождение значит, что кто-то правил вывод компилятора руками - или что
# отпечаток подмодуля переставили, а печать забыли.
verify:
	@rm -rf .verify && mkdir -p .verify/out-c
	@for m in $(MODULES); do \
	  $(FLANG) emit $(LIB)/flang/$$m.flang --target c --out .verify/$$m || exit 1; \
	  cp .verify/$$m/$$m.c .verify/$$m/$$m.h .verify/out-c/ || exit 1; \
	done
	@cp .verify/$(RUNTIME_FROM)/flang_runtime.c .verify/$(RUNTIME_FROM)/flang_runtime.h .verify/out-c/
	@diff -r out-c .verify/out-c || { rm -rf .verify; exit 1; }
	@rm -rf .verify
	@echo "печать совпадает с деревом"

clean:
	rm -rf out-c .emit .verify
