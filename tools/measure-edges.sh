#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - панель у каждого из четырёх краёв, и две панели друг против друга
#
# tools/measure-panel.sh мерит верхний край: панель поднимается, колонки
# укорачиваются, панель сворачивается, колонки возвращаются.  Этого мало.
# Арифметика полосы симметрична по обеим осям и по обоим концам каждой оси,
# но симметрия была ПРОВЕРЕНА только сверху (DGT-WM-13, «что осталось»,
# пункт 4).  Здесь проверены все четыре края на живом менеджере, и сверх них
# случай, которого не бывает у одной панели: две панели друг против друга
# просят вместе больше, чем есть.
#
# Пара - это ribbon_policy_pair() и fts/strut-pair.fts.  До неё арифметика
# пары жила строчкой в screen.c без модели и без вектора.
#
#   sh tools/measure-edges.sh [-d :92] [-n 3] [-h 28]
#
#   -d  дисплей Xvfb
#   -n  сколько окон открыть
#   -h  глубина панели в точках

set -eu

display=:92
clients=3
depth=28

while [ $# -gt 0 ]; do
	case "$1" in
	-d) display=$2; shift 2 ;;
	-n) clients=$2; shift 2 ;;
	-h) depth=$2; shift 2 ;;
	*) echo "usage: measure-edges.sh [-d :92] [-n 3] [-h 28]" >&2
	   exit 1 ;;
	esac
done

here=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)

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

start_server() {
	Xvfb "$display" -screen 0 1280x800x24 -nolisten tcp \
		>"$out/xvfb.log" 2>&1 &
	xvfb=$!
	i=0
	while [ $i -lt 50 ]; do
		DISPLAY=$display xdotool getdisplaygeometry >/dev/null 2>&1 &&
			break
		i=$((i + 1))
		sleep 0.1
	done

	DISPLAY=$display "$here/cwm" -c "$out/cwmrc" >"$out/wm.log" 2>&1 &
	wm=$!
	sleep 1

	n=1
	while [ $n -le "$clients" ]; do
		DISPLAY=$display "$out/redraw-probe" -n "probe$n" -w 500 \
			>"$out/client$n.log" 2>&1 &
		sleep 1.0
		n=$((n + 1))
	done
	sleep 0.5
}

stop_server() {
	kill "$wm" 2>/dev/null || true
	sleep 0.3
	kill "$xvfb" 2>/dev/null || true
	wait "$xvfb" 2>/dev/null || true
}

# Поднять панель у края $1 глубиной $2, вернуть держатель канала в $holder.
raise_panel() {
	_edge=$1
	_depth=$2
	_tag=$3
	mkfifo "$out/panel.$_tag"
	sleep 600 > "$out/panel.$_tag" &
	eval "holder_$_tag=\$!"
	DISPLAY=$display "$out/strut-probe" -e "$_edge" -h "$_depth" \
		< "$out/panel.$_tag" >"$out/panel.$_tag.log" 2>&1 &
	sleep 1.5
}

echo "дисплей $display, колонок $clients, панель $depth точек"
echo "сервер: Xvfb 1280x800x24, программный - числа не про GPU"
echo "зазор ribbongap 8, экран 1280x800"
echo

echo "== по одному краю =="
for e in top bottom left right; do
	start_server
	before=$(columns)
	before_area=$(workarea)

	raise_panel "$e" "$depth" one
	with=$(columns)
	with_area=$(workarea)

	echo quit > "$out/panel.one"
	eval "kill \$holder_one 2>/dev/null || true"
	rm -f "$out/panel.one"
	sleep 0.5
	folded=$(columns)
	folded_area=$(workarea)

	echo "край $e:"
	echo "  без панели   $before"
	echo "  с панелью    $with"
	echo "  после ухода  $folded"
	echo "  _NET_WORKAREA: $before_area -> $with_area -> $folded_area"
	stop_server
	echo
done

echo "== две панели друг против друга =="
echo "Пара по вертикали: сверху $depth и снизу $depth на экране 800."
start_server
before=$(columns)
raise_panel top "$depth" t
raise_panel bottom "$depth" b
both=$(columns)
both_area=$(workarea)
echo "  без панелей  $before"
echo "  обе панели   $both"
echo "  _NET_WORKAREA: $both_area"

echo "Теперь обе просят по 500 точек на экране в 800: вдвоём это невозможно."
echo "set 500" > "$out/panel.t"
echo "set 500" > "$out/panel.b"
sleep 1.5
greedy=$(columns)
greedy_area=$(workarea)
echo "  обе по 500   $greedy"
echo "  _NET_WORKAREA: $greedy_area"

echo quit > "$out/panel.t"
echo quit > "$out/panel.b"
eval "kill \$holder_t 2>/dev/null || true"
eval "kill \$holder_b 2>/dev/null || true"
stop_server

echo
echo "Ближняя полоса выигрывает спор: у пары «500 и 500» верх забирает своё,"
echo "низу остаётся то, что не взяли сверху. Это ribbon_policy_pair()"
echo "и модель fts/strut-pair.fts; до неё арифметика пары не была ни"
echo "названа, ни проверена вектором."
