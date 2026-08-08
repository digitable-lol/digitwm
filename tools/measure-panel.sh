#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - что делает с лентой панель, которая появилась, свернулась и вернулась
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
# У ленты одна ось и один вьюпорт, который ездит по горизонтали.  Панель
# отнимает высоту, то есть трогает не одну колонку, а все сразу - и вопрос
# «сворачиваемая» в этой модели значит ровно одно: сколько стоит отдать полосу
# и забрать её обратно.  Сценарий отвечает тремя числами:
#
#   - высота колонок до панели, при панели и после сворачивания;
#   - смещение вьюпорта в тех же трёх состояниях - оно обязано пережить обе
#     перекладки, иначе «свернуть панель» означало бы «потерять место в ленте»;
#   - перерисовки уже открытых окон на каждой перекладке.  Обещание ленты -
#     «соседей сдвигают, но не сжимают» - панель нарушает по определению: она
#     сжимает всех.  Число говорит, во сколько перерисовок это обходится.
#
# Панелью работает tools/strut-probe: она ставит те же два свойства, что и
# настоящая панель (тип dock и _NET_WM_STRUT_PARTIAL), и ничего больше, - так
# что замер про оконный менеджер, а не про чужую программу.
#
#   sh tools/measure-panel.sh [-d :78] [-n 3] [-h 28] [-w 2000]

set -eu

display=":78"
clients=3
height=28
work=2000
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/digitwm-panel.$$

while getopts d:n:h:w: opt; do
	case $opt in
	d) display=$OPTARG ;;
	n) clients=$OPTARG ;;
	h) height=$OPTARG ;;
	w) work=$OPTARG ;;
	*) echo "usage: $0 [-d :78] [-n 3] [-h 28] [-w 2000]" >&2; exit 1 ;;
	esac
done

for tool in Xvfb xdotool xwininfo xprop cc; do
	command -v "$tool" >/dev/null 2>&1 || { echo "нет $tool" >&2; exit 1; }
done

mkdir -p "$out"
trap 'rm -rf "$out"' EXIT

[ -x "$here/cwm" ] || (cd "$here" && make >/dev/null)
cc -O2 -Wall -o "$out/redraw-probe" "$here/tools/redraw-probe.c" \
	$(pkg-config --cflags --libs x11)
cc -O2 -Wall -o "$out/strut-probe" "$here/tools/strut-probe.c" \
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

# Геометрия всех колонок одной строкой: «x,y WxH» через пробел, слева направо.
columns() {
	for w in $(DISPLAY=$display xwininfo -root -children |
	    awk '/^ *0x/ { print $1 }'); do
		DISPLAY=$display xprop -id "$w" WM_NAME 2>/dev/null |
			grep -q 'probe[0-9]' || continue
		DISPLAY=$display xwininfo -id "$w" 2>/dev/null | awk '
			/Absolute upper-left X/ { x = $4 }
			/Absolute upper-left Y/ { y = $4 }
			/^  Width:/ { w = $2 }
			/^  Height:/ { h = $2 }
			END { printf "%s,%s %sx%s\n", x, y, w, h }'
	done | sort -t, -k1 -n | tr '\n' ' '
	echo
}

workarea() {
	DISPLAY=$display xprop -root _NET_WORKAREA |
		sed 's/.*= //' | cut -d, -f1-4 | tr -d ' '
}

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

n=1
while [ $n -le "$clients" ]; do
	DISPLAY=$display "$out/redraw-probe" -n "probe$n" -w "$work" \
		>"$out/client$n.log" 2>&1 &
	sleep 1.2
	n=$((n + 1))
done
sleep 0.5

before=$(columns)
before_area=$(workarea)

mkfifo "$out/panel.in"
# Держатель канала: без него strut-probe увидит конец ввода и уйдёт.
sleep 600 > "$out/panel.in" &
holder=$!
DISPLAY=$display "$out/strut-probe" -h "$height" < "$out/panel.in" \
	>"$out/panel.log" 2>&1 &
t_up=$(now)
sleep 1.5
with=$(columns)
with_area=$(workarea)

echo hide > "$out/panel.in"
t_hide=$(now)
sleep 1.5
folded=$(columns)
folded_area=$(workarea)

echo show > "$out/panel.in"
t_show=$(now)
sleep 1.5
again=$(columns)
again_area=$(workarea)

echo quit > "$out/panel.in"
kill "$holder" 2>/dev/null || true
sleep 0.3
kill "$wm" 2>/dev/null || true
sleep 0.3
kill "$xvfb" 2>/dev/null || true
wait "$xvfb" 2>/dev/null || true

echo "дисплей $display, колонок $clients, панель $height точек по верхнему краю"
echo "сервер: Xvfb 1280x800x24, программный - числа не про GPU"
echo
echo "== геометрия колонок"
printf '  %-22s %s\n' "без панели:" "$before"
printf '  %-22s %s\n' "панель показана:" "$with"
printf '  %-22s %s\n' "панель свёрнута:" "$folded"
printf '  %-22s %s\n' "панель вернулась:" "$again"
echo
echo "== рабочая область (_NET_WORKAREA: x, y, ширина, высота)"
printf '  %-22s %s\n' "без панели:" "$before_area"
printf '  %-22s %s\n' "панель показана:" "$with_area"
printf '  %-22s %s\n' "панель свёрнута:" "$folded_area"
printf '  %-22s %s\n' "панель вернулась:" "$again_area"
echo
echo "== смещение вьюпорта: левый край самой левой колонки"
echo "   (одно и то же число во всех четырёх состояниях означает, что"
echo "    сворачивание панели не сбивает место в ленте)"
printf '  %-22s %s\n' "без панели:" "$(echo "$before" | cut -d, -f1)"
printf '  %-22s %s\n' "панель показана:" "$(echo "$with" | cut -d, -f1)"
printf '  %-22s %s\n' "панель свёрнута:" "$(echo "$folded" | cut -d, -f1)"
printf '  %-22s %s\n' "панель вернулась:" "$(echo "$again" | cut -d, -f1)"
echo
echo "== перерисовки уже открытых окон на каждой перекладке"
for pair in "появление:$t_up" "сворачивание:$t_hide" "разворачивание:$t_show"; do
	what=${pair%%:*}
	t=${pair##*:}
	count=0
	n=1
	while [ $n -le "$clients" ]; do
		c=$(awk -v s="$t" '$1 == "expose" && $2 >= s && $2 < s + 1500' \
			"$out/client$n.log" | wc -l)
		count=$((count + c))
		n=$((n + 1))
	done
	printf '  %-16s %d перерисовок у %d окон\n' "$what:" "$count" "$clients"
done
echo
echo "== задержка перекладки: от команды до готового кадра последнего окна"
for pair in "появление:$t_up" "сворачивание:$t_hide" "разворачивание:$t_show"; do
	what=${pair%%:*}
	t=${pair##*:}
	awk -v s="$t" -v what="$what" '
		$1 == "drawn" && $2 >= s && $2 < s + 1500 {
			d = $2 - s
			if (d > max) max = d
			seen = 1
		}
		END {
			if (seen)
				printf "  %-16s %.0f мс\n", what ":", max
			else
				printf "  %-16s кадров в окне замера нет\n", what ":"
		}
	' "$out"/client*.log
done
