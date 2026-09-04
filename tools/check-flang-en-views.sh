#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Две поверхности одной спеки говорят одно и то же.
#
# У `.fts` поверхностей было две ФАЙЛАМИ - `column-width.fts` и
# `column-width.en.fts`, - и что это одна модель, проверял `surfaces.mjs`:
# компилировал обе и сравнивал скелеты канонических документов. У flang файл
# один, и вторая поверхность - вид обещания с меткой `en:` рядом с основным.
# Сравнивать два документа больше нечего, и `surfaces.mjs` вместе с ними ушёл.
#
# ЧЕГО КОМПИЛЯТОР НЕ ДЕЛАЕТ, И ЭТО ЗАМЕРЕНО. Шапки спек говорят «цель у вида
# сверяется с целью основного знак в знак». Компилятор 0.7.10 так НЕ делает:
# метка `en:` для него - часть ИМЕНИ обещания, и вид, ослабленный до
# `результат не меньше -5` там, где основное требует `не меньше 0`, проходит
# `flang check` молча. Красным становится только вид, нарушенный ЗНАЧЕНИЯМИ
# примеров.
#
# Поэтому сверяет здесь этот сторож: у каждого обещания ровно два вида -
# основной и `en:`, - они стоят парой и цель у них знак в знак одна.
#
# Запуск:  sh tools/check-flang-en-views.sh
#          sh tools/check-flang-en-views.sh --selfcheck
# Выход:   0 - все пары сошлись; 1 - названы файл, строка и обе цели.

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root" || exit 1

# Один файл за раз: END вместо ENDFILE, потому что ENDFILE знает только gawk,
# а на раннере сборки `awk` - это mawk.
check_file() {
	awk '
	function goal(line,   p) {
		# имя обещания стоит в ёлочках; цель - всё, что после закрывающей.
		p = index(line, "»")
		return substr(line, p + 3)
	}
	function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
	/^[ \t]*обеспечивает / {
		is_en = (index($0, "обеспечивает «en:") > 0)
		g = trim(goal($0))
		if (want_en == 0) {
			if (is_en) {
				printf "FAIL  %s:%d вид «en:» без основного обещания\n", FILENAME, FNR
				bad++
				next
			}
			want_en = 1; prev_goal = g; prev_line = FNR
			next
		}
		if (!is_en) {
			printf "FAIL  %s:%d обещание без вида «en:»\n", FILENAME, prev_line
			bad++
			want_en = 1; prev_goal = g; prev_line = FNR
			next
		}
		if (g != prev_goal) {
			printf "FAIL  %s:%d поверхности разошлись\n      основное: %s\n      en:       %s\n", FILENAME, FNR, prev_goal, g
			bad++
		} else {
			pairs++
		}
		want_en = 0
		next
	}
	END {
		if (want_en == 1) { printf "FAIL  %s:%d обещание без вида «en:»\n", FILENAME, prev_line; bad++ }
		if (bad > 0) { exit 1 }
		print pairs + 0
	}
	' "$1"
}

if [ "${1:-}" = "--selfcheck" ]; then
	# Отрицательный контроль: сторож обязан краснеть на подделке.
	work=$(mktemp -d "${TMPDIR:-/tmp}/digitwm-enview.XXXXXXXX") || exit 1
	trap 'rm -rf "$work"' EXIT INT TERM
	sed 's/обеспечивает «en: offset is not negative» результат не меньше 0/обеспечивает «en: offset is not negative» результат не меньше -5/' \
		fts/flang/output-change.flang > "$work/mutant.flang" || exit 1
	if grep -q 'не меньше -5' "$work/mutant.flang"; then
		:
	else
		echo "самопроверка не подставила подделку - текст спеки изменился" >&2
		exit 1
	fi
	if check_file "$work/mutant.flang" >/dev/null 2>&1; then
		echo "САМОПРОВЕРКА ПРОВАЛЕНА: ослабленный вид «en:» прошёл сторожа" >&2
		exit 1
	fi
	echo "самопроверка: ослабленный вид «en:» замечен."
	exit 0
fi

fails=0
pairs=0
for model in fts/flang/*.flang; do
	got=$(check_file "$model") || { fails=$((fails + 1)); echo "$got" >&2; continue; }
	pairs=$((pairs + got))
done

if [ "$fails" -gt 0 ]; then
	echo "расхождений поверхностей в $fails файлах; правка обещания идёт в оба вида одной строкой" >&2
	exit 1
fi
echo "$pairs пар обещаний, обе поверхности говорят одно и то же."
