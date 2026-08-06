#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - что происходит с уже открытыми окнами, когда открывают ещё одно
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
# DGT-WM-01 требовал контрольного образца - papersway поверх i3 - и трёх
# чисел: мерцание, задержка вставки, поведение скрытых окон.  Образца здесь
# нет и взяться ему неоткуда (ни i3, ни papersway на машине нет, и ставить их
# ради замера - решение не сценария).  Что сценарий может дать - это те же три
# числа для самого digitwm, чтобы сравнивать было с чем, когда образец
# появится.
#
# Что печатается:
#   - задержка вставки: от запуска клиента до готового первого кадра;
#   - мерцание: сколько раз перерисовались УЖЕ ОТКРЫТЫЕ окна, пока
#     открывалось новое.  Обещание ленты - «соседей сдвигают, но не сжимают»
#     - на экране означает ноль их перерисовок при вставке новой колонки;
#   - скрытые окна: сколько окон вышло за край вьюпорта и что с ними стало.
#
#   sh tools/measure-insert.sh [-d :77] [-n 8] [-w 2000]

set -eu

display=":77"
clients=8
work=2000
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/digitwm-insert.$$

while getopts d:n:w: opt; do
	case $opt in
	d) display=$OPTARG ;;
	n) clients=$OPTARG ;;
	w) work=$OPTARG ;;
	*) echo "usage: $0 [-d :77] [-n 8] [-w 2000]" >&2; exit 1 ;;
	esac
done

for tool in Xvfb xdotool cc; do
	command -v "$tool" >/dev/null 2>&1 || { echo "нет $tool" >&2; exit 1; }
done

mkdir -p "$out"
trap 'rm -rf "$out"' EXIT

[ -x "$here/cwm" ] || (cd "$here" && make >/dev/null)
cc -O2 -Wall -o "$out/redraw-probe" "$here/tools/redraw-probe.c" \
	$(pkg-config --cflags --libs x11)

cat > "$out/cwmrc" <<'EOF'
ribbon yes
ribbonhide no
ribbongap 8
ribbonminwidth 120
ribbonminheight 60
ribbonwidths 33 50 67 100
EOF

now() { date +%s%3N; }

Xvfb "$display" -screen 0 1280x800x24 -nolisten tcp >"$out/xvfb.log" 2>&1 &
xvfb=$!
i=0
while [ $i -lt 50 ]; do
	DISPLAY=$display xdotool getdisplaygeometry >/dev/null 2>&1 && break
	i=$((i + 1))
	sleep 0.1
done

DISPLAY=$display "$here/cwm" -c "$out/cwmrc" >"$out/wm.log" 2>&1 &
wm=$!
sleep 1

: > "$out/opened"
n=1
while [ $n -le "$clients" ]; do
	t=$(now)
	DISPLAY=$display "$out/redraw-probe" -n "probe$n" -w "$work" \
		>"$out/client$n.log" 2>&1 &
	echo "$n $t" >> "$out/opened"
	sleep 1.2
	n=$((n + 1))
done
sleep 1

kill "$wm" 2>/dev/null || true
sleep 0.3
kill "$xvfb" 2>/dev/null || true
wait "$xvfb" 2>/dev/null || true

echo "дисплей $display, клиентов $clients, отрезков на кадр $work"
echo "сервер: Xvfb 1280x800x24, программный - числа не про GPU"
echo

# Две границы, потому что вопросов два.  От запуска процесса меряется всё
# вместе - fork, exec, соединение с сервером, - и это не про оконный менеджер.
# От «ready», то есть от XMapWindow, до первого готового кадра меряется ровно
# его доля: принять MapRequest, выбрать колонку, выдать геометрию, отобразить.
echo "== задержка вставки"
printf '  %-6s %12s %12s\n' окно "от запуска" "от XMapWindow"
n=1
while [ $n -le "$clients" ]; do
	start=$(awk -v k="$n" '$1 == k { print $2 }' "$out/opened")
	awk -v k="$n" -v start="$start" '
		$1 == "ready" { ready = $2 }
		$1 == "drawn" && !seen {
			printf "  %-6d %9.0f мс %9.0f мс\n", k, $2 - start, $2 - ready
			seen = 1
		}
	' "$out/client$n.log"
	n=$((n + 1))
done

echo
echo "== мерцание: перерисовки уже открытых окон во время вставки"
echo "   (окно считается уже открытым, если оно нарисовало свой первый кадр"
echo "    раньше, чем запустили следующее)"
n=2
total=0
while [ $n -le "$clients" ]; do
	start=$(awk -v k="$n" '$1 == k { print $2 }' "$out/opened")
	count=0
	who=""
	m=1
	while [ $m -lt $n ]; do
		c=$(awk -v s="$start" '$1 == "expose" && $2 >= s && $2 < s + 1000' \
			"$out/client$m.log" | wc -l)
		count=$((count + c))
		[ "$c" -eq 0 ] || who="$who $m"
		m=$((m + 1))
	done
	echo "  вставка $n-го окна: $count перерисовок у $((n - 1)) уже открытых${who:+, у окон:$who}"
	total=$((total + count))
	n=$((n + 1))
done
echo "  всего: $total"

echo
echo "== скрытые окна: что стало с теми, кто уехал за край"
hidden=$(cat "$out"/client*.log | grep -c '^unmap' || true)
echo "  событий unmap у клиентов: $hidden (при ribbonhide no ожидается 0)"
echo "  окон в сеансе: $clients, вьюпорт 1280 точек,"
echo "  колонка по умолчанию 50 % - за краем оказывается большинство"
