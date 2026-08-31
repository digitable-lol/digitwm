#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - прыгает ли указатель за фокусом ленты, и держится ли фокус, когда
# он не прыгает
#
# `ribbonwarp` - единственная настройка ленты, у которой обе стороны видны
# только на живом сервере: модель про указатель ничего не знает, а probe его
# не двигает.  Проверяются ровно два утверждения, оба на одном и том же
# сценарии из трёх окон:
#
#   ribbonwarp yes - указатель ПЕРЕезжает в окно, получившее фокус (так было
#                    всегда, и это поведение по умолчанию);
#   ribbonwarp no  - указатель НЕ сдвигается ни на точку, а фокус всё равно
#                    переходит.  Второе - половина, ради которой настройка и
#                    существует: без неё «не прыгать» означало бы «колонка
#                    уехала, а клавиатура осталась у соседа».
#
# Фокус лента переводит в двух разных местах, и меряются оба: команда
# ribbon-focus-left/right (ribbon_activate) и открытие окна (обработчик
# MapRequest).  Второе - самый частый прыжок за день работы, и настройка,
# которая его не покрывает, выключает не то, на что жалуются.
#
# Клавиатура видна снаружи как _NET_ACTIVE_WINDOW на корне (client.c, в
# client_set_active), указатель - как `xdotool getmouselocation`.
#
#   sh tools/measure-warp.sh [-d :95] [-n 3]
#
#   -d  дисплей Xvfb (по умолчанию первый свободный из :95..:120)
#   -n  сколько окон открыть
#
# Скрипт поднимает свой Xvfb, свой cwm и свои xterm'ы и убивает только
# собственные PID.  Код возврата: 0 - оба утверждения держатся, 1 - нарушены,
# 2 - проверить не удалось.

set -u

display=
clients=3

while [ $# -gt 0 ]; do
	case "$1" in
	-d) display=$2; shift 2 ;;
	-n) clients=$2; shift 2 ;;
	*) echo "usage: measure-warp.sh [-d :95] [-n 3]" >&2; exit 2 ;;
	esac
done

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/digitwm-warp.$$
pids=

for tool in Xvfb xdotool xprop xdpyinfo xterm; do
	command -v "$tool" >/dev/null 2>&1 || { echo "нет $tool" >&2; exit 2; }
done
[ -x "$here/cwm" ] || (cd "$here" && make >/dev/null) || exit 2

mkdir -p "$out" || exit 2
cleanup() {
	for p in $pids; do kill "$p" 2>/dev/null; done
	sleep 0.5
	for p in $pids; do kill -9 "$p" 2>/dev/null; done
	rm -rf "$out"
}
trap cleanup EXIT INT TERM

if [ -z "$display" ]; then
	n=95
	while [ $n -le 120 ]; do
		[ ! -e "/tmp/.X$n-lock" ] && [ ! -e "/tmp/.X11-unix/X$n" ] &&
			{ display=":$n"; break; }
		n=$((n + 1))
	done
fi
[ -n "$display" ] || { echo "нет свободного дисплея" >&2; exit 2; }

# "x y" указателя и id активного окна - два свидетеля, снятые одинаково в
# обоих прогонах.
pointer() {
	xdotool getmouselocation --shell 2>/dev/null |
		sed -n 's/^X=\(.*\)/\1/p;s/^Y=\(.*\)/\1/p' | tr '\n' ' '
}
active() {
	win=$(xprop -root _NET_ACTIVE_WINDOW 2>/dev/null | sed -n 's/.*# *//p')
	echo "${win:-нет}"
}

# Один прогон: поднять сервер и менеджер, открыть окна, поставить указатель в
# центр, перевести фокус влево и снова вправо.  Пять строк вида
# "метка x y активное_окно": «пусто» - до окон, «окна» - после всех вставок,
# «старт» - после того как указатель поставили руками, дальше две команды.
run() {
	warp=$1
	pids=

	Xvfb "$display" -screen 0 1280x800x24 -nolisten tcp \
		>"$out/xvfb.log" 2>&1 &
	pids="$pids $!"
	i=0
	while [ $i -lt 40 ]; do
		DISPLAY=$display xdpyinfo >/dev/null 2>&1 && break
		i=$((i + 1)); sleep 0.25
	done
	[ $i -lt 40 ] || { echo "Xvfb $display не поднялся" >&2; return 2; }

	printf 'ribbon yes\nribbonwarp %s\n' "$warp" > "$out/cwmrc"
	DISPLAY=$display "$here/cwm" -c "$out/cwmrc" >"$out/cwm.log" 2>&1 &
	pids="$pids $!"
	i=0
	while [ $i -lt 40 ]; do
		DISPLAY=$display xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null |
			grep -q 'window id' && break
		i=$((i + 1)); sleep 0.25
	done
	[ $i -lt 40 ] || { echo "менеджер не встал" >&2; return 2; }

	export DISPLAY=$display
	echo "пусто  $(pointer)$(active)"

	n=1
	while [ $n -le "$clients" ]; do
		xterm -title "warp$n" -e sleep 600 >>"$out/xterm.log" 2>&1 &
		pids="$pids $!"
		sleep 1
		n=$((n + 1))
	done
	sleep 1
	echo "окна   $(pointer)$(active)"

	xdotool mousemove 640 400
	sleep 0.5
	echo "старт  $(pointer)$(active)"
	xdotool key --clearmodifiers super+h
	sleep 1
	echo "4-h    $(pointer)$(active)"
	xdotool key --clearmodifiers super+l
	sleep 1
	echo "4-l    $(pointer)$(active)"
	unset DISPLAY

	for p in $pids; do kill "$p" 2>/dev/null; done
	sleep 1
	for p in $pids; do kill -9 "$p" 2>/dev/null; done
	pids=
	sleep 1
}

rc=0
for warp in yes no; do
	echo "== ribbonwarp $warp"
	run "$warp" > "$out/log.$warp" || exit 2
	cat "$out/log.$warp"

	# «до» - строка, с которой сравнивается движение: для вставки это пустой
	# экран, для команд - положение, поставленное руками.
	at() { awk -v l="$1" '$1 == l {print $2, $3}' "$out/log.$warp"; }
	win_at() { awk -v l="$1" '$1 == l {print $4}' "$out/log.$warp"; }

	insert=0
	command=0
	focus=0
	[ "$(at окна)" = "$(at пусто)" ] || insert=1
	[ "$(at 4-h)" = "$(at старт)" ] && [ "$(at 4-l)" = "$(at старт)" ] ||
		command=1
	[ "$(win_at 4-h)" = "$(win_at старт)" ] || focus=1

	if [ "$focus" = 0 ]; then
		echo "ПРОВАЛ: фокус не перешёл по 4-h - сценарий ничего не проверил"
		rc=1
	elif [ "$warp" = yes ] && { [ "$insert" = 0 ] || [ "$command" = 0 ]; }; then
		echo "ПРОВАЛ: при ribbonwarp yes указатель не сдвинулся" \
		     "(вставка $insert, команда $command)"
		rc=1
	elif [ "$warp" = no ] && { [ "$insert" = 1 ] || [ "$command" = 1 ]; }; then
		echo "ПРОВАЛ: при ribbonwarp no указатель всё-таки уехал" \
		     "(вставка $insert, команда $command)"
		rc=1
	else
		echo "держится"
	fi
done

exit $rc
