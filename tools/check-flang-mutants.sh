#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Зелёная сверка ничего не значит, пока не показано, что она бывает красной.
#
# `fts/flang/conformance.flang` и `fts/flang/layout.flang` спрашивают живой
# оконный менеджер и сверяют его ответ с ответом спеки. Здесь ответ оконного
# менеджера портится - и требуется, чтобы сверка это заметила и назвала, где.
#
# ПОЧЕМУ ПОРТИТСЯ ИМЕННО ОТВЕТ C, А НЕ СПЕКА. Прежний `selftest.mjs` портил
# модель: там модель и векторы жили в разных файлах, и порча модели была
# единственным способом расшевелить сверку. У flang примеры спеки И ЕСТЬ
# векторы, поэтому всякая порча спеки валится раньше - на `flang test`, на её
# собственных примерах, - и о самой сверке не говорит ничего. Сверка стоит
# ровно за тем, чего примеры не ловят: за расхождением, пришедшим СО СТОРОНЫ C.
# Его и надо подделать.
#
# Подделка не требует пересборки: `cwm` подменяется оболочкой, которая зовёт
# настоящий и правит одно число в ответе. Спека при этом не тронута ни одной
# буквой.
#
# Запуск:  sh tools/check-flang-mutants.sh
# Выход:   0 - каждая порча замечена и названа; 1 - названа порча, которую
#          сверка проспала.

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root" || exit 1

flang=${FLANG:-flang}
if ! command -v "$flang" >/dev/null 2>&1; then
	echo "нет компилятора flang: положите его на PATH или задайте FLANG=" >&2
	exit 1
fi
[ -x "$root/cwm" ] || { echo "нет $root/cwm - сначала make" >&2; exit 1; }

work=
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT INT TERM
work=$(mktemp -d "${TMPDIR:-/tmp}/digitwm-mutant.XXXXXXXX") || exit 1

# План зовёт `../../cwm` от каталога СВОЕГО файла, поэтому копия дерева
# повторяет два уровня: подменённый оконный менеджер ложится в корень копии.
mkdir -p "$work/fts/flang" || exit 1
cp fts/flang/*.flang "$work/fts/flang/" || exit 1

fails=0
oks=0

# mutant <имя> <план> <программа awk> <что обязано быть в жалобе>
mutant() {
	name=$1 plan=$2 program=$3 want=$4
	{
		echo "#!/bin/sh"
		echo "\"$root/cwm\" \"\$@\" | awk '$program'"
	} > "$work/cwm"
	chmod +x "$work/cwm"

	out=$("$flang" io "$work/fts/flang/$plan" 2>&1)
	code=$?
	if [ "$code" -eq 0 ]; then
		fails=$((fails + 1))
		echo "НЕ ЗАМЕЧЕНО: $name - $plan прошёл на испорченном ответе C" >&2
		return
	fi
	if ! printf '%s' "$out" | grep -q -- "$want"; then
		fails=$((fails + 1))
		echo "НЕ НАЗВАНО: $name - $plan упал, но в жалобе нет «$want»" >&2
		return
	fi
	oks=$((oks + 1))
	echo "ok  $name - замечено, и названо «$want»"
}

# 1. Скаляры. Одной порчей и одним прогоном проверяются все десять спек сразу:
#    каждая отвечает на своих векторах, и жалоба обязана назвать каждую по
#    имени её вектора. Утилита, чьи векторы до сверки не доехали, тем и выдаст
#    себя, что в жалобе её не окажется.
{
	echo "#!/bin/sh"
	echo "\"$root/cwm\" \"\$@\" | awk '{ if (\$1 == \"ok\" && \$2 != \"layout\") { \$3 = \$3 + 1 }; print }'"
} > "$work/cwm"
chmod +x "$work/cwm"

scalars=$("$flang" io "$work/fts/flang/conformance.flang" 2>&1)
if [ $? -eq 0 ]; then
	fails=$((fails + 1))
	echo "НЕ ЗАМЕЧЕНО: ответ C на единицу больше - conformance.flang прошёл на испорченном ответе" >&2
	scalars=
fi
for util in scroll-offset stack-offset column-width window-height insertion \
	focus-after-close output-change strut-span strut-reserve strut-pair
do
	if printf '%s' "$scalars" | grep -q "$util «"; then
		oks=$((oks + 1))
		echo "ok  ответ C на единицу больше - названа спека «$util»"
	else
		fails=$((fails + 1))
		echo "НЕ НАЗВАНО: векторы «$util» до сверки не доехали - в жалобе её нет" >&2
	fi
done

# 2. Целые сценарии. Портится по одному числу на каждый род строк вывода,
#    иначе «сверяются два потока байт» стоит на честном слове: строку, которой
#    сверка не касается, никто бы не хватился.
mutant "ширина колонки на единицу больше" layout.flang \
	'{ if ($1 == "column") { $6 = $6 + 1 }; print }' \
	'строка 7: спека «column'
mutant "смещение ленты на единицу больше" layout.flang \
	'{ if ($1 == "ribbon") { $5 = $5 + 1 }; print }' \
	'строка 6: спека «ribbon length'
mutant "окно на ленте съехало" layout.flang \
	'{ if ($1 == "window") { $6 = $6 + 1 }; print }' \
	'строка 8: спека «window'
mutant "окно на экране съехало" layout.flang \
	'{ if ($1 == "window") { $11 = $11 + 1 }; print }' \
	'строка 8: спека «window'
mutant "высота полотна на единицу больше" layout.flang \
	'{ if ($1 == "ribbon") { $11 = $11 + 1 }; print }' \
	'строка 6: спека «ribbon length'
mutant "вьюпорт другой" layout.flang \
	'{ if ($1 == "viewport") { $4 = $4 + 1 }; print }' \
	'строка 3: спека «viewport'

echo
if [ "$fails" -gt 0 ]; then
	echo "порч без реакции: $fails; сверка не доказала, что умеет падать" >&2
	exit 1
fi
echo "$oks порч - все замечены и названы."
