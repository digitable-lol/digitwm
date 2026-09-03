#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Лента считается ТЕМ ЖЕ, чем считалась - проверка прогоном, а не чтением.
#
# Арифметика ленты больше не написана в ribbon.c: десять ribbon_policy_*
# теперь тонкие обёртки над напечатанным из flang кодом (ribbon-flang/, довод
# в его README.md). Вопрос, на который отвечает этот скрипт, ровно один:
# ОТВЕЧАЕТ ЛИ digitwm ТЕПЕРЬ ТЕМИ ЖЕ ЧИСЛАМИ, что отвечал до подмены.
#
# «Проверки прошли» - не ответ. Ответ - два потока байт и cmp(1).
#
#   слева   ref_policy_* - десять функций, выкушенных из ribbon.c
#           ПРИШПИЛЕННОГО коммита, где они ещё были рукописной арифметикой.
#           Пин и sha256 ниже: эталон, который поехал, эталоном не был бы.
#   справа  ribbon_policy_* из ribbon.o ЭТОГО дерева - того самого объектного
#           файла, который линкуется в оконный менеджер. Не копии обёрток, не
#           пересборки «как в дереве»: собирается ribbon.c как есть.
#
# Сетка входов - сетка библиотеки (flang-ribbon/tools/compare.sh), значение в
# значение: 526871 вход. Так две сверки складываются, а не просто рифмуются.
# Библиотека доказала на ней «напечатанное = рукописный C»; здесь доказывается
# «digitwm через напечатанное = тот же рукописный C».
#
# Отрицательный контроль в конце: проверку, которую нечем уронить, проверкой
# не считают. Портится КОПИЯ напечатанного, и требуется красное.
#
# Сети не нужно: эталон берётся из истории этого же репозитория.
# Компилятор flang не нужен: напечатанное лежит в дереве.
#
# Запуск:  sh tools/check-ribbon-flang.sh
# Выход:   0 - расхождений ноль; 1 - расхождение или сломанный эталон.

set -e

# Коммит, в котором десять политик ещё были рукописной арифметикой. Это же
# состояние ribbon.c пришпилено у библиотеки (flang-ribbon/tools/compare.sh),
# и sha256 обязан совпадать с тем, который она называет: иначе «сверено с
# библиотекой» и «сверено здесь» - про разные файлы.
PIN=bfd5d85ef0417c38ff24ab0224844696b06fab06
PIN_RIBBON_SHA256=2a12726ccd8296fb1ea6f6ec3ba9df28f32ca59983e8ea24bb1aa4e1768768eb

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}

err() { printf '%s\n' "$*" >&2; }

work=
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT INT TERM
work=$(mktemp -d "${TMPDIR:-/tmp}/digitwm-ribbon-flang.XXXXXXXX")

# ── 1. Эталон: ribbon.c пришпиленного коммита ───────────────────────────────
git -C "$root" show "$PIN:ribbon.c" > "$work/ribbon-ref.c" 2>/dev/null || {
	err "в истории нет $PIN:ribbon.c - мелкий клон?"
	err "нужен полный клон: git fetch --unshallow"
	exit 1
}
have=$(sha256sum "$work/ribbon-ref.c" | awk '{print $1}')
if [ "$have" != "$PIN_RIBBON_SHA256" ]; then
	err "ribbon.c пришпиленного коммита не тот:"
	err "  ждали  $PIN_RIBBON_SHA256"
	err "  видим  $have"
	exit 1
fi
echo "эталон: коммит $PIN, ribbon.c sha256 $have"

# Отпечаток напечатанного до прогона. Порча в отрицательном контроле живёт в
# копии, и это утверждение проверяется в конце, а не подразумевается.
sha256sum "$root/ribbon-flang/out-c"/* > "$work/emitted-before"

# ── 2. Выкусить десять политик и переименовать ──────────────────────────────
# Приём дословно тот же, что в tools/no-x-build.sh (проход 4) и в сверке самой
# библиотеки: функция ribbon_policy_* вместе со строкой типа над именем.
# Переименование - потому что имена столкнулись бы: справа те же десять имён,
# и они-то и проверяются.
{
	echo '#include "calmwm.h"'
	awk '
	/^ribbon_policy_[a-z]*\(/ { emit = 1; print prev }
	emit { print }
	/^\}/ { emit = 0 }
	{ prev = $0 }
	' "$work/ribbon-ref.c" | sed 's/\bribbon_policy_/ref_policy_/g'
} > "$work/policy_ref.c"

nfn=$(grep -c '^ref_policy_[a-z]*(' "$work/policy_ref.c")
[ "$nfn" -eq 10 ] || { err "выкушено $nfn политик вместо десяти"; exit 1; }
echo "  выкушено политик: $nfn"

cat > "$work/ribbon-flang-shim.h" <<'EOF'
#ifndef RIBBON_FLANG_SHIM_H
#define RIBBON_FLANG_SHIM_H
/*
 * Обе стороны видят НАСТОЯЩИЙ struct conf, а не свой вымысел о нём: политику
 * ширины колонки читают из Conf.ribbonwidth обе, и разойдись раскладка
 * структуры - сверка сравнивала бы разные поля и молча зеленела.
 */
#include "calmwm.h"
int ref_policy_offset(int, int, int, int, int, int);
int ref_policy_voffset(int, int, int, int, int, int);
int ref_policy_width(int, int, int, int);
int ref_policy_height(int, int, int, int, int);
int ref_policy_insert(int, int, int, int, int, int);
int ref_policy_close(int, int, int, int);
int ref_policy_output(int, int, int);
int ref_policy_span(int, int, int, int);
int ref_policy_reserve(int, int, int, int, int);
int ref_policy_pair(int, int, int, int);
#endif
EOF

# ── 3. Заглушка оконной системы ─────────────────────────────────────────────
# ribbon.c берётся как есть, поэтому ему нужно то, что он зовёт: одиннадцать
# операций контракта wsi.h плюс конфигурация и память. Ни одна не вызывается -
# сверяются политики, - но линковщику нужны все. Заглушка заголовков X11 -
# та же самая, что у macos/Makefile и macos/check.sh; копии нет.
sh "$root/macos/fakex.sh" "$work/fakex" >/dev/null
cat > "$work/stubs.c" <<'EOF'
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "calmwm.h"

struct conf Conf;

/*
 * Ни одна из них не зовётся: проверяются политики, а не механика. Подписи -
 * дословно из wsi.h, потому что заглушка с другой подписью не собралась бы,
 * и это единственное, что здесь держит их в согласии с контрактом.
 */
static void nope(void) { abort(); }

int client_geom_current(struct client_ctx *c) { (void)c; nope(); return 0; }
void client_resize(struct client_ctx *c, int r) { (void)c; (void)r; nope(); }
void client_hide(struct client_ctx *c) { (void)c; nope(); }
void client_show(struct client_ctx *c) { (void)c; nope(); }
void client_raise(struct client_ctx *c) { (void)c; nope(); }
void client_set_active(struct client_ctx *c) { (void)c; nope(); }
struct client_ctx *client_current(struct screen_ctx *sc) { (void)sc; nope(); return NULL; }
void client_ptr_save(struct client_ctx *c) { (void)c; nope(); }
void client_ptr_warp(struct client_ctx *c) { (void)c; nope(); }
struct region_ctx *region_pointer(struct screen_ctx *sc) { (void)sc; nope(); return NULL; }
void wsi_settle(void) { nope(); }

int conf_ribbonrule_match(struct client_ctx *c) { (void)c; nope(); return 0; }
void *xcalloc(size_t n, size_t s) { (void)n; (void)s; nope(); return NULL; }
char *xstrdup(const char *s) { (void)s; nope(); return NULL; }
EOF

# ── 4. Собрать обе стороны и прогонщик ──────────────────────────────────────
inc="-I$work/fakex -I$root -I$work"
warn="-Wall -Werror=implicit-function-declaration"

build() {
	out=$1; shift
	rm -rf "$work/build"; mkdir -p "$work/build"
	cp "$root/ribbon-flang/out-c"/*.c "$root/ribbon-flang/out-c"/*.h "$work/build/"
	# Одна правка, если её просили: имя файла и подстановка. Требуется,
	# чтобы правка ДЕЙСТВИТЕЛЬНО что-то изменила: подстановка, промахнувшаяся
	# мимо перепечатанного кода, оставила бы отрицательный контроль зелёным
	# по той же причине, по которой он и заводится, - потому что ничего не
	# сломалось. Молчаливый промах здесь хуже отсутствия контроля.
	if [ -n "${SPOIL_FILE:-}" ]; then
		cp "$work/build/$SPOIL_FILE" "$work/before-spoil"
		sed -i "$SPOIL_EXPR" "$work/build/$SPOIL_FILE"
		if cmp -s "$work/before-spoil" "$work/build/$SPOIL_FILE"; then
			err "порча не подействовала: подстановка не нашла, что править"
			err "  файл:  $SPOIL_FILE"
			err "  строка: $SPOIL_EXPR"
			err "Напечатанное изменилось - поправьте подстановку."
			exit 1
		fi
	fi
	(cd "$root" && $CC $warn -O2 -D_GNU_SOURCE $inc \
	    -I"$work/build" -c ribbon.c -o "$work/ribbon.o")
	# Заголовки напечатанного ribbon.c берёт относительным путём от себя,
	# поэтому испорченную копию подставляем не через -I, а подменой файла.
	for f in "$work"/build/*.c; do
		$CC -std=c99 -Wall -O2 -c "$f" -o "${f%.c}.o"
	done
	(cd "$root" && $CC $warn -O2 -D_GNU_SOURCE $inc -c \
	    tools/ribbon-flang-probe.c -o "$work/probe.o")
	(cd "$root" && $CC $warn -O2 -D_GNU_SOURCE $inc -c \
	    "$work/policy_ref.c" -o "$work/policy_ref.o")
	(cd "$root" && $CC $warn -O2 -D_GNU_SOURCE $inc -c \
	    "$work/stubs.c" -o "$work/stubs.o")
	$CC -o "$out" "$work/probe.o" "$work/policy_ref.o" "$work/ribbon.o" \
	    "$work/stubs.o" "$work"/build/*.o -lm -lpthread
}

build "$work/probe"
echo "собрано: слева выкушенный эталон, справа ribbon.o этого дерева"

# ── 5. Прогон ───────────────────────────────────────────────────────────────
set +e
out=$("$work/probe" "$work/ref.txt" "$work/tree.txt" 2>"$work/diffs.txt")
code=$?
set -e
total=$(echo "$out" | awk '{print $1}')
bad=$(echo "$out" | awk '{print $2}')

echo
echo "сверено входов: $total"
echo "расхождений:    $bad"

if [ "$code" != "0" ] || [ "${bad:-1}" != "0" ]; then
	err "СВЕРКА НЕ СОШЛАСЬ. Первые расхождения:"
	head -20 "$work/diffs.txt" >&2
	exit 1
fi

# Второй способ, независимый от арифметики самого прогонщика.
if ! cmp -s "$work/ref.txt" "$work/tree.txt"; then
	err "потоки разошлись побайтно, хотя прогонщик насчитал ноль:"
	diff "$work/ref.txt" "$work/tree.txt" | head -20 >&2
	exit 1
fi
lines=$(wc -l < "$work/ref.txt")
bytes=$(wc -c < "$work/ref.txt")
echo "потоки ответов совпали побайтно: строк $lines, байт $bytes (cmp)"

# Ни одна обёртка не позвала запасной ответ: напечатанное ответило числом на
# каждом из входов, а не бедой FLANG_PROPERTY.
if [ -s "$work/diffs.txt" ]; then
	err "прогонщик что-то сказал в stderr, а не должен был:"
	head -10 "$work/diffs.txt" >&2
	exit 1
fi
echo "запасной ответ не понадобился ни разу: ни одного отказа напечатанного"

# ── 6. Отрицательный контроль ───────────────────────────────────────────────
# Портим КОПИЮ напечатанного - зазор в арифметике смещения на единицу - и
# требуем красного. Дерево не трогается: правка живёт в $work/build.
echo
echo "отрицательный контроль: портим копию напечатанного"
# Сколько панель отнимает у ближнего края: было `панель - положение`, стало
# на единицу больше. Ровно та же порча, что в отрицательном контроле самой
# библиотеки, - арифметика зазора на единицу.
SPOIL_FILE=strut.c
SPOIL_EXPR='s/panel\.as\.number - polozhenie\.as\.number/panel.as.number - polozhenie.as.number + 1.0/g'
export SPOIL_FILE SPOIL_EXPR
build "$work/probe-spoilt"
unset SPOIL_FILE SPOIL_EXPR
set +e
spoilt=$("$work/probe-spoilt" "$work/ref2.txt" "$work/tree2.txt" 2>/dev/null)
scode=$?
set -e
sbad=$(echo "$spoilt" | awk '{print $2}')
if [ "$scode" = "0" ] || [ "${sbad:-0}" = "0" ]; then
	err "отрицательный контроль НЕ СРАБОТАЛ: порченое напечатанное признано"
	err "совпавшим. Сверка не умеет краснеть - значит, она ничего не значит."
	exit 1
fi
if cmp -s "$work/ref2.txt" "$work/tree2.txt"; then
	err "отрицательный контроль: cmp не заметил порченого потока"
	exit 1
fi
echo "  расхождений на порченом: $sbad из $total - сверка умеет краснеть"

sha256sum "$root/ribbon-flang/out-c"/* > "$work/emitted-after"
if ! cmp -s "$work/emitted-before" "$work/emitted-after"; then
	err "проверка наследила: напечатанное в дереве изменилось за прогон."
	err "Порча живёт в копии под \$TMPDIR и до ribbon-flang/out-c доходить"
	err "не должна ни при каких обстоятельствах."
	diff "$work/emitted-before" "$work/emitted-after" >&2
	exit 1
fi
echo "дерево не тронуто: напечатанное в ribbon-flang/out-c то же, что было"

echo
echo "Лента считается напечатанным из flang кодом и отвечает теми же числами,"
echo "что рукописная арифметика коммита $PIN: $total входов, ноль расхождений."
