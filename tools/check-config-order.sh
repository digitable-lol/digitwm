#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Порядок поиска файла настроек - прогоном, а не чтением кода.
#
# Файл настроек ищется в четыре шага: -c, потом $DIGITWMRC, потом
# ~/.digitable/digitwm/digitwmrc, потом cwm'ский ~/.cwmrc (и только про
# последний говорится вслух). Порядок написан один раз - в confpath.c, - и обе
# сборки компилируют ИМЕННО ЕГО. Но «обе сборки берут один файл» - утверждение
# о собранных двоичных файлах, а не об исходнике: линковка могла не взять
# объектный файл, порт мог оставить свою старую дорожку, а документ - обещать
# третье. Поэтому спрашиваются двоичные файлы, и одной и той же таблицей:
#
#   sh tools/check-config-order.sh ./cwm              сборка X11
#   sh tools/check-config-order.sh <mac-цель>         сборка macOS
#                                                     (её зовёт macos/check.sh)
#
# Разойдись два порядка - разойдутся и два прогона одной таблицы, и это видно
# в CI, а не через полгода на чужой машине.
#
# Как читается ответ: обе сборки в режиме -n печатают ПЕРВОЙ СТРОКОЙ файл,
# который они выбрали (X11 - его одного, mac-цель - его и разбор через
# двоеточие). Всё до первого двоеточия и есть путь; временный каталог здесь
# двоеточий не содержит.
#
# Отрицательный контроль:
#
#   sh tools/check-config-order.sh --selfcheck
#
# Таблица прогоняется по подставному «менеджеру», у которого порядок нарушен
# ровно так, как это случилось бы при небрежной правке: cwm'ский файл сильнее
# нашего, и вслух ничего не говорится. Сторож обязан покраснеть; если он
# зелёный - краснеть он не умеет вовсе, и его зелёный цвет ничего не стоит.
#
# Выход: 0 - порядок тот, что обещан (или подлог замечен); 1 - назван каждый
#        случай: что ожидалось, что вышло и в каком мире.

set -u

selfcheck=0
bin=

for arg in "$@"; do
	case "$arg" in
	--selfcheck)	selfcheck=1 ;;
	-*)		echo "неизвестный ключ: $arg" >&2; exit 2 ;;
	*)		bin=$arg ;;
	esac
done

if [ "$selfcheck" -eq 0 ] && [ -z "$bin" ]; then
	echo "укажите двоичный файл: sh tools/check-config-order.sh ./cwm" >&2
	exit 2
fi

# Свой каталог с именем от ядра, а не угаданное имя в общем /tmp: см. шапку
# tools/no-x-build.sh - предсказуемое имя плюс mkdir -p однажды стёрло чужой
# каталог целиком.
work=
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT INT TERM
work=$(mktemp -d "${TMPDIR:-/tmp}/digitwm-confpath.XXXXXXXX") || {
	echo "не удалось создать временный каталог под ${TMPDIR:-/tmp}" >&2
	exit 1
}

if [ "$selfcheck" -eq 1 ]; then
	# Подставной «менеджер» с нарушенным порядком. Настоящего он не
	# трогает: это отдельный файл, и живёт он ровно до конца прогона.
	bin="$work/wrong-wm"
	cat > "$bin" <<'EOF'
#!/bin/sh
# ПОДЛОГ, а не сборка: cwm'ский файл сильнее нашего, и вслух не говорится
# ничего. Ровно та ошибка, ради которой этот сторож написан.
set -u
file=
while [ $# -gt 0 ]; do
	case "$1" in
	-c)	file=$2; shift 2 ;;
	*)	shift ;;
	esac
done
if [ -n "$file" ]; then echo "$file"; exit 0; fi
if [ -f "$HOME/.cwmrc" ]; then echo "$HOME/.cwmrc"; exit 0; fi
if [ -n "${DIGITWMRC:-}" ]; then echo "$DIGITWMRC"; exit 0; fi
echo "$HOME/.digitable/digitwm/digitwmrc"
EOF
	chmod +x "$bin"
fi

case "$bin" in
/*)	;;
*)	bin=$(CDPATH= cd -- "$(dirname -- "$bin")" && pwd)/$(basename -- "$bin") ;;
esac

if [ ! -x "$bin" ]; then
	echo "нет $bin - соберите его: make (X11) или sh macos/check.sh (mac-цель)" >&2
	exit 1
fi

home="$work/home"
own="$home/.digitable/digitwm/digitwmrc"
legacy="$home/.cwmrc"
named="$work/named-with-c"

fails=0
oks=0

# Мир: чистый домашний каталог, и в нём только то, что просит случай.
# «own» и «legacy» - наш файл и cwm'ский; «named» лежит вне дома, потому что
# -c ни от какого дома не зависит.
world() {
	rm -rf "$home"
	mkdir -p "$home/.digitable/digitwm"
	for what in "$@"; do
		case "$what" in
		own)	echo 'ribbongap 12' > "$own" ;;
		legacy)	echo 'ribbongap 13' > "$legacy" ;;
		named)	echo 'ribbongap 14' > "$named" ;;
		esac
	done
}

# case <название> <ожидаемый путь> <вслух: yes|no> [-c файл] -- с $DIGITWMRC в
# переменной окружения ниже.
check() {
	name=$1; want=$2; loud=$3; shift 3

	out=$("$bin" -n "$@" 2>"$work/err" </dev/null)
	got=$(printf '%s\n' "$out" | sed -n '1{s/:.*//;p;}')
	said=no
	if grep -q "^digitwm: reading .*\.cwmrc" "$work/err" 2>/dev/null; then
		said=yes
	fi

	if [ "$got" != "$want" ]; then
		fails=$((fails + 1))
		echo "FAIL  $name" >&2
		echo "      ожидался файл: $want" >&2
		echo "      прочитан:      ${got:-<ничего не напечатано>}" >&2
		return
	fi
	if [ "$said" != "$loud" ]; then
		fails=$((fails + 1))
		echo "FAIL  $name" >&2
		if [ "$loud" = yes ]; then
			echo "      файл взят от cwm, а вслух об этом не сказано" >&2
		else
			echo "      сказано про cwm'ский файл там, где читается не он" >&2
		fi
		return
	fi
	oks=$((oks + 1))
	printf 'ok    %-52s %s\n' "$name" "${want#$home/}"
}

echo "порядок поиска настроек: $bin"
echo

# 1. -c сильнее всего: и переменной, и обоих файлов.
world own legacy named
DIGITWMRC="$work/never-read" HOME="$home" \
	check "-c сильнее переменной и обоих файлов" "$named" no -c "$named"

# 2. $DIGITWMRC сильнее обоих файлов.
world own legacy named
DIGITWMRC="$named" HOME="$home" \
	check "\$DIGITWMRC сильнее обоих файлов" "$named" no

# 3. Пустая переменная - всё равно что незаданная.
world own legacy
DIGITWMRC="" HOME="$home" \
	check "пустой \$DIGITWMRC не считается заданным" "$own" no

# 4. Свой файл, когда он один.
world own
HOME="$home" check "свой файл, когда он один" "$own" no

# 5. ГЛАВНЫЙ СЛУЧАЙ. Лежат оба - читается наш, и молча: про cwm'ский путь
# говорится тогда и только тогда, когда по нему живут.
world own legacy
HOME="$home" check "лежат оба - читается наш, и молча" "$own" no

# 6. Совместимость: пришедший из cwm продолжает работать - и слышит об этом.
world legacy
HOME="$home" check "один cwm'ский - читается он, и сказано вслух" "$legacy" yes

# 7. Нет ничего: назван наш путь, и вслух ничего - читать нечего.
world
HOME="$home" check "нет ни одного файла - назван наш путь" "$own" no

echo
if [ "$selfcheck" -eq 1 ]; then
	if [ "$fails" -gt 0 ]; then
		echo "отрицательный контроль: подлог замечен, $fails случая(ев) из $((fails + oks)) покраснели."
		exit 0
	fi
	echo "отрицательный контроль НЕ СРАБОТАЛ: порядок нарушен, а сторож зелёный." >&2
	exit 1
fi

if [ "$fails" -gt 0 ]; then
	echo "$fails случая(ев) из $((fails + oks)): порядок не тот, что обещан confpath.h." >&2
	exit 1
fi
echo "$oks случая(ев), порядок тот же, что в confpath.h."
