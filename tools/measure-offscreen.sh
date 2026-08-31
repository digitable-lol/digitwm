#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm - сколько стоит прокрутка при двух политиках невидимых окон
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
# DGT-WM-07: `ribbonhide yes` гасит окна, которые вьюпорт не показывает,
# `ribbonhide no` оставляет их отрисованными за краем экрана.  Сценарий
# прогоняет одну и ту же прокрутку при обеих политиках и печатает время от
# нажатия до последнего готового кадра.
#
# Мерится на Xvfb - программном сервере без ускорения.  Это названо прямо в
# doc/offscreen.md: числа отсюда ограничивают стоимость протокола и
# перерисовки у клиента, но ничего не говорят о композиторе и GPU.
#
#   sh tools/measure-offscreen.sh [-d :77] [-n 6] [-r 24] [-w 2000]

set -eu

display=":77"
clients=6
rounds=24
work=2000
here=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/digitwm-offscreen.$$

while getopts d:n:r:w: opt; do
	case $opt in
	d) display=$OPTARG ;;
	n) clients=$OPTARG ;;
	r) rounds=$OPTARG ;;
	w) work=$OPTARG ;;
	*) echo "usage: $0 [-d :77] [-n 6] [-r 24] [-w 2000]" >&2; exit 1 ;;
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

now() { date +%s%3N; }

# Один прогон при одной политике.  Печатает по строке на прокрутку:
# «<время от нажатия до последнего готового кадра> <expose> <unmap> <map>».
run() {
	mode=$1
	hide=$2
	log="$out/$mode"
	mkdir -p "$log"

	cat > "$log/cwmrc" <<EOF
ribbon yes
ribbonhide $hide
ribbongap 8
ribbonminwidth 120
ribbonminheight 60
ribbonwidths 33 50 67 100
EOF

	Xvfb "$display" -screen 0 1280x800x24 -nolisten tcp >"$log/xvfb.log" 2>&1 &
	xvfb=$!
	i=0
	while [ $i -lt 50 ]; do
		DISPLAY=$display xdotool getdisplaygeometry >/dev/null 2>&1 && break
		i=$((i + 1))
		sleep 0.1
	done

	DISPLAY=$display "$here/cwm" -c "$log/cwmrc" >"$log/wm.log" 2>&1 &
	wm=$!
	sleep 1

	n=1
	while [ $n -le "$clients" ]; do
		DISPLAY=$display "$out/redraw-probe" -n "probe$n" -w "$work" \
			>"$log/client$n.log" 2>&1 &
		n=$((n + 1))
		sleep 0.35
	done
	sleep 1

	# Прокрутка влево до края, потом туда-сюда: каждая колонка успевает
	# уехать за край и вернуться.
	n=1
	while [ $n -lt "$clients" ]; do
		DISPLAY=$display xdotool key --clearmodifiers super+h
		n=$((n + 1))
		sleep 0.3
	done

	# Ход ленты: до правого края и обратно.  Прокрутка, упёршаяся в край,
	# ничего не показывает и в выборку не попадает - поэтому не «туда-сюда»,
	# а полный проход, где каждая колонка успевает уехать и вернуться.
	: > "$log/keys"
	r=0
	step=1
	while [ $r -lt "$rounds" ]; do
		if [ $step -lt "$clients" ]; then key=super+l; else key=super+h; fi
		t=$(now)
		DISPLAY=$display xdotool key --clearmodifiers $key
		echo "$t" >> "$log/keys"
		sleep 0.6
		r=$((r + 1))
		step=$((step + 1))
		[ $step -ge $((clients * 2 - 1)) ] && step=1
	done

	kill "$wm" 2>/dev/null || true
	sleep 0.3
	kill "$xvfb" 2>/dev/null || true
	wait "$xvfb" 2>/dev/null || true

	cat "$log"/client*.log > "$log/events"
	awk -v keysfile="$log/keys" '
	BEGIN {
		nk = 0
		while ((getline line < keysfile) > 0) keys[++nk] = line + 0
	}
	# Событие относим к последнему нажатию, которое было не позже него и
	# не дальше секунды назад: за это время всё, что должно перерисоваться,
	# перерисовывается, а следующее нажатие ещё не пришло.
	function bucket(t,   i, found) {
		found = 0
		for (i = 1; i <= nk; i++)
			if (t >= keys[i] && t - keys[i] < 1000) found = i
		return found
	}
	$1 == "drawn"   { i = bucket($2 + 0); if (i) { d = $2 - keys[i]; if (d > last[i]) last[i] = d; drawn[i]++ } }
	$1 == "expose"  { i = bucket($2 + 0); if (i) expose[i]++ }
	$1 == "unmap"   { i = bucket($2 + 0); if (i) unmapped[i]++ }
	$1 == "map"     { i = bucket($2 + 0); if (i) mapped[i]++ }
	END {
		for (i = 1; i <= nk; i++)
			if (drawn[i] > 0)
				printf "%.1f %d %d %d\n", last[i], expose[i], unmapped[i] + 0, mapped[i] + 0
	}' "$log/events" > "$log/samples"

	echo "== $mode (ribbonhide $hide)"
	printf "  всего за прогон: Expose %s, unmap %s, map %s\n" \
		"$(grep -c '^expose' "$log/events" || true)" \
		"$(grep -c '^unmap' "$log/events" || true)" \
		"$(grep -c '^map' "$log/events" || true)"
	sort -n "$log/samples" | awk -v mode="$mode" '
	{ v[NR] = $1; e += $2; u += $3; m += $4 }
	END {
		if (NR == 0) { print "  ни одной перерисовки после прокрутки - сценарий не сработал"; exit 1 }
		printf "  прокруток с перерисовкой: %d\n", NR
		printf "  до последнего кадра, мс: медиана %.1f, 90-й %.1f, худшая %.1f\n", \
			v[int((NR + 1) / 2)], v[int(NR * 0.9) ? int(NR * 0.9) : 1], v[NR]
		printf "  на прокрутку: Expose %.1f, unmap %.1f, map %.1f\n", e / NR, u / NR, m / NR
	}'
}

echo "дисплей $display, клиентов $clients, прокруток $rounds, отрезков на кадр $work"
echo "сервер: Xvfb 1280x800x24, программный - числа не про GPU"
echo
run mapped no
echo
run hidden yes
