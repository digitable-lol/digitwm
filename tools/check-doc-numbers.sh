#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Числа переносимости - проверкой, а не глазами.
#
# `doc/portability.md` и `doc/portability.ru.md` устроены так, что почти в
# каждой строке с числом рядом названа команда, которой это число снимается:
# «1542 | `wc -l ribbon.c`». Это хорошее свойство и совершенно бесполезное,
# пока команду никто не запускает. Дерево растёт каждую неделю, а документ -
# нет; за одну только историю этого файла числа расходились дважды, и оба раза
# расхождение находил человек, случайно решивший проверить.
#
# Здесь эти же команды выполняются, и заявленное сверяется с настоящим. Правило
# отбора жёсткое: сюда попадает число, которое дерево выводит ОДНОЗНАЧНО - у
# него есть команда, и у команды один ответ. Замеры (миллисекунды, выдачи
# геометрии, окна в случайном прогоне) и числа из чужих проектов сюда не
# попадают: их не проверить чтением дерева, и делать вид, что проверено, хуже,
# чем не проверять. Что осталось за границей и почему - перечислено в конце
# файла.
#
# Оба перевода проверяются порознь и одним и тем же числом из дерева: документ,
# который разошёлся сам с собой, - тоже расхождение.
#
# Запуск:  sh tools/check-doc-numbers.sh
# Выход:   0 - документ говорит то же, что дерево; 1 - назван каждый разошедшийся
#          пункт: что стоит в документе, что в дереве и чем это снято.

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root" || exit 1

work=
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT INT TERM
work=$(mktemp -d "${TMPDIR:-/tmp}/digitwm-docnum.XXXXXXXX") || {
	echo "не удалось создать временный каталог" >&2
	exit 1
}

EN=doc/portability.md
RU=doc/portability.ru.md
for f in "$EN" "$RU"; do
	[ -f "$f" ] || { echo "нет файла $f" >&2; exit 1; }
done

# Проза в этих документах переносится по строкам, и число сплошь и рядом стоит
# на одной строке, а слово после него - на следующей. Поэтому сверяется не файл,
# а его развёрнутая в одну строку копия: тогда якорь можно писать так, как
# фраза читается.
tr '\n' ' ' < "$EN" | tr -s ' ' > "$work/en"
tr '\n' ' ' < "$RU" | tr -s ' ' > "$work/ru"

fails=0
oks=0

# grab <файл> <BRE с одной группой> - что документ заявляет в этом месте
grab() {
	sed -n "s/$2/\1/p" "$1" | head -1
}

# claim <документ> <что именно> <ожидание из дерева> <BRE> <чем снято>
claim() {
	doc=$1 what=$2 want=$3 pat=$4 how=$5
	got=$(grab "$work/$doc" "$pat")
	if [ -z "$got" ]; then
		fails=$((fails + 1))
		echo "FAIL  $doc: $what - место не найдено, шаблон устарел вместе с текстом" >&2
	elif [ "$got" = "$want" ]; then
		oks=$((oks + 1))
		printf 'ok    %-3s %-46s %s\n' "$doc" "$what" "$want"
	else
		fails=$((fails + 1))
		echo "FAIL  $doc: $what - в документе $got, в дереве $want ($how)" >&2
	fi
}

# both <что> <ожидание> <BRE для en> <BRE для ru> <чем снято>
both() {
	claim en "$1" "$2" "$3" "$5"
	claim ru "$1" "$2" "$4" "$5"
}

# say <что> <ожидание> <фактическое> <чем снято> - сверка без документа,
# для величин, которые дерево должно подтверждать само себе.
say() {
	if [ "$2" = "$3" ]; then
		oks=$((oks + 1))
		printf 'ok    --  %-46s %s\n' "$1" "$2"
	else
		fails=$((fails + 1))
		echo "FAIL  $1 - ожидалось $2, получено $3 ($4)" >&2
	fi
}

# --------------------------------------------------------------- размеры файлов

ribbon_lines=$(wc -l < ribbon.c | tr -d ' ')
probe_lines=$(wc -l < probe.c | tr -d ' ')
xutil_lines=$(wc -l < xutil.c | tr -d ' ')

both "строк в ribbon.c (таблица)" "$ribbon_lines" \
	'.*| lines in `ribbon\.c` | \([0-9][0-9]*\) |.*' \
	'.*| строк в `ribbon\.c` | \([0-9][0-9]*\) |.*' \
	'wc -l ribbon.c'
both "строк в ribbon.c (ответ)" "$ribbon_lines" \
	'.*but \([0-9][0-9]*\) lines of `ribbon\.c`.*' \
	'.*а \([0-9][0-9]*\) строки `ribbon\.c`.*' \
	'wc -l ribbon.c'
both "строк в probe.c (харнесс)" "$probe_lines" \
	'.*`probe\.c` (\([0-9][0-9]*\) lines,.*' \
	'.*`probe\.c` (\([0-9][0-9]*\) строки,.*' \
	'wc -l probe.c'
both "строк в probe.c (ответ)" "$probe_lines" \
	'.*and \([0-9][0-9]*\) lines of `probe\.c`.*' \
	'.*и \([0-9][0-9]*\) строки `probe\.c`.*' \
	'wc -l probe.c'
both "строк в xutil.c (весь EWMH)" "$xutil_lines" \
	'.*`xutil\.c` (\([0-9][0-9]*\), all EWMH).*' \
	'.*`xutil\.c` (\([0-9][0-9]*\), весь EWMH).*' \
	'wc -l xutil.c'

# Механика вне ribbon.c - ровно те восемь файлов, что перечислены в самой
# строке таблицы. Список берётся отсюда, а не из документа: если он изменится,
# менять надо обе стороны сразу, и это заметно.
mech='client.c xutil.c xevents.c screen.c kbfunc.c menu.c group.c calmwm.c'
# shellcheck disable=SC2086
mech_lines=$(wc -l $mech | awk '$2 == "total" { print $1 }')
both "строк механики вне ribbon.c (таблица)" "$mech_lines" \
	'.*| mechanics outside `ribbon\.c` | \*\*\([0-9][0-9]*\)\*\* |.*' \
	'.*| механика вне `ribbon\.c` | \*\*\([0-9][0-9]*\)\*\* |.*' \
	"wc -l $mech"
both "строк механики вне ribbon.c (цена)" "$mech_lines" \
	'.*| \([0-9][0-9]*\) lines of X11 mechanics.*' \
	'.*| \([0-9][0-9]*\) строк механики X11.*' \
	"wc -l $mech"

# ------------------------------------------------------------------ шов и контракт

wsi_ops=$(sed -n 's/^[a-z].*[ 	*]\([a-z_][a-z_0-9]*\)(.*);$/\1/p' wsi.h | LC_ALL=C sort -u)
wsi_n=$(printf '%s\n' "$wsi_ops" | grep -c .)
both "операций оконной системы (таблица)" "$wsi_n" \
	'.*\*\*\([0-9][0-9]*\) operations\*\*, every one declared.*' \
	'.*\*\*\([0-9][0-9]*\) операций\*\*, все объявлены.*' \
	'grep по объявлениям wsi.h'
both "операций оконной системы (договор)" "$wsi_n" \
	'.*\*\*\([0-9][0-9]*\) window-system operations\*\*.*' \
	'.*\*\*\([0-9][0-9]*\) операций оконной системы\*\*.*' \
	'grep по объявлениям wsi.h'

# Числа мало: договор назван поимённо, и каждое имя обязано быть в обоих
# переводах. Двенадцатая операция, дописанная в wsi.h и не дописанная в
# документ, - это ровно тот случай, ради которого документ и пишется.
for sym in $wsi_ops; do
	for doc in en ru; do
		if grep -q "\`$sym\`" "$work/$doc"; then
			oks=$((oks + 1))
		else
			fails=$((fails + 1))
			echo "FAIL  $doc: операция \`$sym\` объявлена в wsi.h, но в документе не названа" >&2
		fi
	done
done
printf 'ok    --  %-46s %s\n' "все операции wsi.h названы в обоих переводах" "$wsi_n x 2"

policies=$(grep -c '^ribbon_policy_[a-z]*(' ribbon.c)
say "политик ribbon_policy_* в ribbon.c" 10 "$policies" \
	"grep -c '^ribbon_policy_[a-z]*(' ribbon.c - документ везде говорит «десять политик»"

# Заглушка вместо Xlib - тот самый here-document внутри no-x-build.sh.
stub_lines=$(sed -n "/Xlib\.h\" <<'EOF'/,/^EOF\$/p" tools/no-x-build.sh | sed '1d;$d' | wc -l | tr -d ' ')
both "строк в заглушке вместо Xlib.h" "$stub_lines" \
	'.*puts a \([0-9][0-9]*\)-line stub.*' \
	'.*заглушку на \([0-9][0-9]*\) строк.*' \
	'here-document Xlib.h в tools/no-x-build.sh'

# Единственное оставшееся протекание названо номерами строк. Номера едут при
# любой правке выше по файлу, и проверить их глазами нельзя вовсе.
bw1=$(grep -n 'cc->geom\.w = MAX(1, col->w - (cc->bwidth \* 2));' ribbon.c | cut -d: -f1 | head -1)
bw2=$(grep -n 'cc->geom\.h = MAX(1, h - (cc->bwidth \* 2));' ribbon.c | cut -d: -f1 | head -1)
if [ -z "$bw1" ] || [ -z "$bw2" ]; then
	fails=$((fails + 1))
	echo "FAIL  строки с bwidth в ribbon.c не найдены - протекание #1 переписано, документ надо читать целиком" >&2
else
	both "строки протекания bwidth в ribbon.c" "$bw1-$bw2" \
		'.*`ribbon_place()`, `ribbon\.c:\([0-9-]*\)`.*' \
		'.*`ribbon_place()`, `ribbon\.c:\([0-9-]*\)`.*' \
		'grep -n по обеим строкам с cc->bwidth'
fi

# ----------------------------------------------------------------------- векторы

if command -v jq >/dev/null 2>&1; then
	vec_total=$(jq -s 'map(length)|add' fts/vectors/*.json)
	vec_layout=$(jq 'length' fts/vectors/layout.json)
	vec_pair=$(jq 'length' fts/vectors/strut-pair.json)
else
	vec_total=$(python3 -c 'import json,glob,sys; print(sum(len(json.load(open(f))) for f in sorted(glob.glob("fts/vectors/*.json"))))')
	vec_layout=$(python3 -c 'import json; print(len(json.load(open("fts/vectors/layout.json"))))')
	vec_pair=$(python3 -c 'import json; print(len(json.load(open("fts/vectors/strut-pair.json"))))')
fi
vec_scalar=$((vec_total - vec_layout))

# Моделей столько же, сколько политик: пара .fts на каждую, плюс перевод.
models=$(ls fts/*.fts | grep -vc '\.en\.fts$')
say "моделей fts/*.fts (без переводов)" "$policies" "$models" \
	'по модели на политику - иначе «10 сверок полей» в документе ни на чём не стоит'

# 448 в документе разложено формулой; здесь считается та же формула из чисел
# дерева, а не переписывается результат.
checks=$((2 * (models + vec_scalar) + 2 * vec_layout))

both "векторов в fts/vectors" "$vec_total" \
	'.*gives \*\*\([0-9][0-9]*\)\*\*, of which.*' \
	'.*даёт \*\*\([0-9][0-9]*\)\*\*, из них.*' \
	"jq -s 'map(length)|add' fts/vectors/*.json"
both "векторов в fts/vectors (ответ)" "$vec_total" \
	'.*\*\*\([0-9][0-9]*\) vectors\*\* of `fts\/vectors\/`.*' \
	'.*\*\*\([0-9][0-9]*\) векторах\*\* `fts\/vectors\/`.*' \
	"jq -s 'map(length)|add' fts/vectors/*.json"
both "сценариев в layout.json" "$vec_layout" \
	'.*gives \*\*\([0-9][0-9]*\)\*\* scenarios.*' \
	'.*даёт \*\*\([0-9][0-9]*\)\*\* сценариев.*' \
	"jq 'length' fts/vectors/layout.json"
both "скалярных векторов" "$vec_scalar" \
	'.*remaining \*\*\([0-9][0-9]*\)\*\* are scalar.*' \
	'.*остальные \*\*\([0-9][0-9]*\)\*\*.*' \
	'214 минус 13, обе половины сняты jq'
both "проверок конформанса" "$checks" \
	'.*prints `проверок: \([0-9][0-9]*\)`.*' \
	'.*печатает `проверок: \([0-9][0-9]*\)`.*' \
	'2 x (моделей + скалярных) + 2 x сценариев'
both "проверок конформанса (ответ)" "$checks" \
	'.*[^0-9]\([0-9][0-9]*\) checks, `conformance\.mjs`.*' \
	'.*это \([0-9][0-9]*\) проверок `conformance\.mjs`.*' \
	'2 x (моделей + скалярных) + 2 x сценариев'
both "векторов в strut-pair.json" "$vec_pair" \
	'.*10 examples, \([0-9][0-9]*\) vectors.*' \
	'.*10 примеров, \([0-9][0-9]*\) векторов.*' \
	"jq 'length' fts/vectors/strut-pair.json"

pair_examples=$(grep -cE '^[[:space:]]*пример «' fts/strut-pair.fts)
both "примеров в fts/strut-pair.fts" "$pair_examples" \
	'.*: \([0-9][0-9]*\) examples, [0-9][0-9]* vectors.*' \
	'.*: \([0-9][0-9]*\) примеров, [0-9][0-9]* векторов.*' \
	"grep -c 'пример' fts/strut-pair.fts"

# --------------------------------------------------------------- прочее из дерева

widths=$(sed -n 's/.*c->ribbonwidth\[[0-3]\] = \([0-9][0-9]*\);.*/\1/p' conf.c | paste -sd/ -)
both "проценты пресетов из conf_init()" "$widths" \
	'.*keeps \([0-9/]*\) as constants.*' \
	'.*держит \([0-9/]*\) константами.*' \
	'sed по c->ribbonwidth[] в conf.c'

for h in conformance selftest surfaces invariants hotplug; do
	if [ -f "fts/harness/$h.mjs" ]; then
		oks=$((oks + 1))
	else
		fails=$((fails + 1))
		echo "FAIL  документ называет харнесс fts/harness/$h.mjs, которого в дереве нет" >&2
	fi
done
printf 'ok    --  %-46s %s\n' "пять харнессов, названных в документе, на месте" "5"

# ---------------------------------------------------------------------- итог

echo
if [ "$fails" -gt 0 ]; then
	echo "$fails расхождений документа с деревом, $oks сошлось." >&2
	echo "Каждое из них - число, которое документ обещает снять командой." >&2
	exit 1
fi
echo "$oks сверок, 0 расхождений."

# ЧТО СЮДА НЕ ВОШЛО И ПОЧЕМУ - чтобы следующий не искал заново.
#
#  * 142 / 256 / 359 строк по слоям («тела функций без комментариев и заголовка
#    файла»). Команды нет, и определения тоже: где кончается «модель ленты» и
#    начинается «механика внутри ribbon.c» - решение человека, а не разметка в
#    файле. Пока в ribbon.c нет метки границы, эти три числа проверяются только
#    пересчётом вручную.
#  * 500 случаев / 4386 окон. Выводится, но не чтением: нужен собранный с Xlib
#    cwm и сборка ribbon.c в WebAssembly. Прогон засеян (seed 20260809 в
#    tools/wasm-layout/check.mjs), то есть воспроизводим - это работа для CI,
#    а не для сторожа, который обязан отработать за секунду на любом дереве.
#  * миллисекунды и выдачи геометрии (0,06 / 0,51 / 19 / 9 / 44 / 99 / 887).
#    Это замеры на машине, а не свойства дерева; их сторож проверить не может
#    и не притворяется.
#  * числа чужих проектов (Silica 2434, AeroSpace 928, yabai ~2400, 7,1 мс
#    alt-tab) и номера строк в чужих исходниках. Их источник - ссылка, и
#    проверяются они по ссылке.
