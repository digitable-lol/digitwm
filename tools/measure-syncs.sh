#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - сколько раз одна вставка трогает чужие окна
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
# doc/portability.md называет цену переноса на macOS одним числом, которого до
# сих пор никто не мерил: сколько выдач геометрии приходится на одну вставку.
# На X11 это не видно - выдача уходит в буфер и не ждёт ответа.  На macOS
# каждая такая выдача - синхронный круг в чужой процесс, и число кругов
# решает, годится перенос или нет.
#
# Скрипт считает не по коду, а по проводу: заглушка tools/count-geom.so
# подставляется под LD_PRELOAD и записывает каждый вызов, ушедший в libX11.
# Столбец «кругов AX» - это тот же список, пересчитанный по правилу
# «XMoveResizeWindow = две записи (AXPosition и AXSize), XMoveWindow = одна,
# синтетический ConfigureNotify и рамка = ноль, соответствия нет».
#
#   sh tools/measure-syncs.sh [-d :78] [-n 9] [-w 500]

set -eu

display=":78"
clients=9
work=500
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/digitwm-syncs.$$

while getopts d:n:w: opt; do
	case $opt in
	d) display=$OPTARG ;;
	n) clients=$OPTARG ;;
	w) work=$OPTARG ;;
	*) echo "usage: $0 [-d :78] [-n 9] [-w 500]" >&2; exit 1 ;;
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
cc -shared -fPIC -O2 -Wall -o "$out/count-geom.so" "$here/tools/count-geom.c" \
	$(pkg-config --cflags x11) -ldl

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

: > "$out/geom.log"
DISPLAY=$display DIGITWM_GEOM_LOG="$out/geom.log" \
	LD_PRELOAD="$out/count-geom.so" "$here/cwm" -c "$out/cwmrc" \
	>"$out/wm.log" 2>&1 &
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
end=$(now)

kill "$wm" 2>/dev/null || true
sleep 0.3
kill "$xvfb" 2>/dev/null || true
wait "$xvfb" 2>/dev/null || true

echo "дисплей $display, клиентов $clients, отрезков на кадр $work"
echo "считает провод, а не код: LD_PRELOAD поверх libX11"
echo

# Один проход помечает каждую выдачу словом «зря», если она повторяет ту
# геометрию, которая у этого окна уже стоит.  Сравнение сквозное по сеансу, а
# не внутри вставки: окно, не сдвинувшееся с прошлого раза, получает свою
# геометрию заново - и на macOS платит за это кругом.
awk '
	$2 == "moveresize" || $2 == "move" || $2 == "resize" {
		split($3, a, "=")
		key = $5 " " $6 " " $7 " " $8
		dup = (key == last[$4]) ? 1 : 0
		last[$4] = key
		print $1, $2, a[2] + 0, $4, dup
	}
' "$out/geom.log" > "$out/geom.ann"

echo "== выдачи геометрии на вставку"
printf '  %-9s %6s %8s %11s %10s\n' \
	вставка окон выдач "из них зря" "кругов AX"
n=1
while [ $n -le "$clients" ]; do
	start=$(awk -v k="$n" '$1 == k { print $2 }' "$out/opened")
	if [ "$n" -lt "$clients" ]; then
		stop=$(awk -v k="$((n + 1))" '$1 == k { print $2 }' "$out/opened")
	else
		stop=$end
	fi
	awk -v k="$n" -v start="$start" -v stop="$stop" -v open="$n" '
		$1 >= start && $1 < stop {
			pushes++
			rounds += $3
			same += $5
		}
		END {
			printf "  %-9d %6d %8d %11d %10d\n",
			    k, open, pushes, same, rounds
		}
	' "$out/geom.ann"
	n=$((n + 1))
done

echo
echo "== итого за сеанс"
awk '{ call[$2]++ } END {
	printf "  синтетических Configure: %d\n", call["config"]
	printf "  работы с рамкой:         %d\n",
	    call["border"] + call["borderwidth"]
}' "$out/geom.log"
awk '
	{ pushes++; rounds += $3; same += $5 }
	END {
		printf "  выдач геометрии:         %d\n", pushes
		printf "  из них та же геометрия:  %d (%.0f %%)\n",
		    same, pushes ? same * 100.0 / pushes : 0
		printf "  кругов AX на macOS:      %d\n", rounds
	}
' "$out/geom.ann"
echo
echo "«та же геометрия» - выдача, повторяющая последнюю выданную этому же окну."
echo "На X11 она стоит буфер, на macOS - круг в чужой цикл событий."
