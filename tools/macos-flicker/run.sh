#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - одна команда, чтобы получить число мелькания на маке
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
# Что делает: открывает восемь окон подряд и говорит, сколько миллисекунд
# каждое стояло видимым не на своём месте.  Это то самое число, которого не
# хватает doc/portability.md, чтобы ответить «годится перенос или нет».
#
#   sh tools/macos-flicker/run.sh [окон] [интервал, мс]
#
# Нужно одно право: «Универсальный доступ» для Терминала (или для того, из
# чего запускается axcost).  SIP выключать не требуется.  Скрипт сам скажет,
# если права нет.

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
count=${1:-8}
gap=${2:-1500}
out=${TMPDIR:-/tmp}/digitwm-flicker.$$

[ "$(uname -s)" = "Darwin" ] || {
	echo "Это меряется только на macOS.  Здесь - sh $here/stub-build.sh" >&2
	exit 1
}

mkdir -p "$out"
trap 'rm -rf "$out"' EXIT INT TERM

(cd "$here" && make >/dev/null)

"$here/axcost" trusted || {
	echo "Дайте право «Универсальный доступ» и запустите снова." >&2
	exit 1
}

# Помощник должен появиться раньше наблюдателя: наблюдатель подписывается на
# события конкретного процесса, а значит должен знать его pid.
"$here/flicker" "$count" "$gap" > "$out/helper.log" 2>&1 &
helper=$!
sleep 1
pid=$helper

secs=$(( (count * gap) / 1000 + 6 ))
"$here/axcost" watch "$pid" "$secs" "$count" > "$out/manager.log" 2>&1 || true

wait "$helper" 2>/dev/null || true

echo "== со стороны менеджера: известие о новом окне и выдача геометрии"
cat "$out/manager.log"
echo
echo "== со стороны приложения: сколько окно было видно не на месте"
cat "$out/helper.log"
echo
echo "Складывать так: «мелькание» из второй половины - это то, что видит глаз,"
echo "а первая говорит, из чего оно состоит.  Разница между ними - время от"
echo "появления окна на экране до kAXWindowCreated, то самое, которого нет на"
echo "X11: там менеджер получает запрос ВМЕСТО системы и отвечает до показа."
