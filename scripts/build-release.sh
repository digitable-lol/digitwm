#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm - собрать выпускной архив маковской цели.
#
#   sh scripts/build-release.sh [версия]
#
# Версия берётся из довода, иначе из DIGITWM_VERSION, иначе из файла VERSION в
# корне дерева. Ведущее «v» снимается: тег v0.1.0 и версия 0.1.0 - одно и то же.
#
# ОДИН ПРОГОН - ОДНА АРХИТЕКТУРА, И ЭТО ВЫБОР, А НЕ ОГРАНИЧЕНИЕ. Сосед по
# организации, digitdisk, собирает все четыре цели одной командой с Linux:
# он на Go, CGO выключен, кросс-сборка бесплатна. Здесь так нельзя - маковская
# половина digitwm написана на Objective-C поверх Accessibility API и Carbon, и
# без macOS SDK не собирается вовсе. Значит собирать надо на маке; а раз уж мы
# на маке, то и запускать собранное надо на нём же. Поэтому каждая из двух
# целей выпуска собирается на бегунке СВОЕЙ архитектуры и там же запускается:
# ни один выпущенный двоичный файл не уезжает наружу, ни разу не исполнившись
# на том железе, для которого собран. Кросс-сборка через `-arch` тоже возможна
# и стоила бы одного прогона вместо двух - но тогда x86-64 никто бы не запустил.
#
# ПОВТОРИМОСТЬ. Три условия, и все три проверяются здесь, а не обещаются:
#
#   1. Дерево чистое. Грязное останавливает сборку: архив, которому не
#      соответствует ни один коммит, нечем проверить.
#   2. Время берётся у коммита (SOURCE_DATE_EPOCH), а не у часов машины, -
#      иначе один и тот же коммит давал бы разные архивы каждую минуту.
#   3. Отладочная информация выключена, пути обрезаны (-ffile-prefix-map),
#      порядок файлов в архиве задан списком, а не обходом каталога, владелец
#      обнулён, метаданные macOS в архив не кладутся.
#
# Цель собирается ДВАЖДЫ в разные каталоги, и отпечатки сверяются. Это и есть
# доказательство: расхождение останавливает выпуск. Отключается доводом
# --skip-repro-check, когда нужна просто быстрая сборка.
#
# ЧЕГО ЗДЕСЬ НЕТ. Контрольных сумм на оба архива и заполненной формулы: они
# сводятся из двух прогонов сразу, и это делает scripts/make-formula.sh -
# отдельным шагом, когда оба архива уже лежат рядом.

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

DIST="$root/dist"
repro_check=1
version_arg=""

for arg in "$@"; do
	case "$arg" in
	--skip-repro-check) repro_check=0 ;;
	-h | --help)
		sed -n '5,12p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	-*)
		echo "build-release: неизвестный ключ $arg" >&2
		exit 2
		;;
	*) version_arg=$arg ;;
	esac
done

# --- где мы и на чём ------------------------------------------------------

case "$(uname -s)" in
Darwin) ;;
*)
	echo "build-release: выпуск digitwm собирается только на macOS." >&2
	echo "               Objective-C поверх Accessibility API и Carbon" >&2
	echo "               требует macOS SDK, а он живёт только в Xcode." >&2
	echo "               На Linux проверяется другое: make macos-check." >&2
	exit 1
	;;
esac

case "$(uname -m)" in
arm64) arch=arm64 goarch=arm64 ;;
x86_64) arch=x86_64 goarch=amd64 ;;
*)
	echo "build-release: неизвестная архитектура $(uname -m)" >&2
	exit 1
	;;
esac

# --- версия, коммит, время ------------------------------------------------

version=${version_arg:-${DIGITWM_VERSION:-}}
if [ -z "$version" ]; then
	[ -f VERSION ] || {
		echo "build-release: нет файла VERSION и версия не задана" >&2
		exit 1
	}
	version=$(tr -d ' \t\n\r' < VERSION)
fi
version=${version#v}
[ -n "$version" ] || {
	echo "build-release: версия пуста" >&2
	exit 1
}

git rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
	echo "build-release: это не клон git - хеш сборки взять неоткуда" >&2
	exit 1
}

if [ -n "$(git status --porcelain)" ]; then
	if [ "${DIGITWM_ALLOW_DIRTY:-0}" = "1" ]; then
		echo "build-release: ВНИМАНИЕ, дерево с правками, DIGITWM_ALLOW_DIRTY=1" >&2
	else
		git status --porcelain >&2
		echo "build-release: дерево с правками - выпуск собирается только из чистого." >&2
		echo "               git status --porcelain должен быть пуст." >&2
		exit 1
	fi
fi

commit=$(git rev-parse HEAD)
commit_date=$(git show -s --format=%cI HEAD)
SOURCE_DATE_EPOCH=$(git show -s --format=%ct HEAD)
export SOURCE_DATE_EPOCH
# ld64 кладёт в архивы .a время сборки; ZERO_AR_DATE его обнуляет. Своих .a
# здесь нет, но переменная стоит копейки и снимает целый класс расхождений.
ZERO_AR_DATE=1
export ZERO_AR_DATE

name="digitwm-$version-darwin-$goarch"
stamp=$(TZ=UTC date -r "$SOURCE_DATE_EPOCH" +%Y%m%d%H%M.%S)

# Нижняя граница macOS. Без неё двоичный файл потребует ту версию системы, что
# стоит на бегунке (сегодня это 15), и не запустится ни на одной машине старше.
# 12.0 - потому что всё, что этот порт зовёт у Apple, старше её: Accessibility
# API - с 10.2, -[NSScreen localizedName] - с 10.15, Carbon Event Manager -
# древнее обоих.
min_macos=12.0

echo "digitwm $version"
echo "  коммит          $commit"
echo "  время коммита   $commit_date (SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH)"
echo "  машина          $(sw_vers -productName) $(sw_vers -productVersion), $(uname -m)"
echo "  компилятор      $(cc --version 2>/dev/null | head -1)"
echo "  цель            darwin/$goarch (-arch $arch, -mmacosx-version-min=$min_macos)"
echo "  сверка повтором $([ "$repro_check" = 1 ] && echo включена || echo выключена)"
echo

# --- сборка ---------------------------------------------------------------
#
# Собирает тот же macos/Makefile, который человек запускает у себя: выпуск не
# имеет права собираться другой командой, чем та, что описана в документации, -
# иначе документация проверяет не то, что выпущено. Меняются только флаги, и
# каждый из них назван выше.

release_cflags="-Wall -Wextra -O2 -arch $arch -mmacosx-version-min=$min_macos -ffile-prefix-map=$root=."

build_one() {
	# build_one <каталог объектных файлов> <куда положить двоичный файл>
	rm -rf "$1"
	make -C "$root/macos" \
	    OBJDIR="$1" \
	    PROG="$2" \
	    CFLAGS="$release_cflags" \
	    >"$1.log" 2>&1 || {
		echo
		echo "build-release: сборка не прошла, вывод make целиком:" >&2
		cat "$1.log" >&2
		exit 1
	}
	# Предупреждения компилятора - не повод падать, но повод показать: это
	# первая сборка этого кода против настоящих заголовков Apple, и молчать
	# о том, что сказал компилятор, здесь дороже всего.
	if grep -q 'warning:' "$1.log"; then
		echo
		echo "  компилятор сказал ($(grep -c 'warning:' "$1.log") предупреждений):"
		grep 'warning:' "$1.log" | sed 's/^/    /'
	fi
}

rm -rf "$DIST"
mkdir -p "$DIST"
stage="$DIST/$name"
mkdir -p "$stage"

printf '%-28s' "сборка darwin/$goarch"
build_one "$DIST/.obj1" "$stage/digitwm"
sum=$(shasum -a 256 "$stage/digitwm" | cut -d' ' -f1)

if [ "$repro_check" = 1 ]; then
	build_one "$DIST/.obj2" "$DIST/.second"
	sum2=$(shasum -a 256 "$DIST/.second" | cut -d' ' -f1)
	if [ "$sum" != "$sum2" ]; then
		echo
		echo "build-release: сборка НЕ повторима: $sum != $sum2" >&2
		exit 1
	fi
	rm -f "$DIST/.second"
	echo "$sum  повторена"
else
	echo "$sum"
fi
rm -rf "$DIST/.obj1" "$DIST/.obj2" "$DIST/.obj1.log" "$DIST/.obj2.log"

# --- что едет в архиве ----------------------------------------------------
#
# То, без чего двоичный файл нельзя ни законно раздать, ни понять, ни
# настроить: три лицензии (наша, унаследованная от cwm, и уведомление о
# границе между ними), обе редакции описания, версия, страница cwmrc(5) - тот
# самый файл настроек, который digitwm читает, - и обе редакции руководства по
# установке на маке, потому что именно к ним отсылает формула, когда система
# отказывает в Accessibility.

cp LICENSE LICENSE.upstream NOTICE README.md README.ru.md VERSION \
    cwmrc.5 "$stage/"
mkdir -p "$stage/doc"
cp doc/macos-install.md doc/macos-install.ru.md "$stage/doc/"

find "$stage" -print0 | xargs -0 env TZ=UTC touch -t "$stamp"
(cd "$DIST" && find "$name" | LC_ALL=C sort > "$DIST/.files")
COPYFILE_DISABLE=1 tar --format ustar --uid 0 --gid 0 --numeric-owner \
    --no-mac-metadata -n -C "$DIST" -T "$DIST/.files" -cf - \
    | gzip -9 -n > "$DIST/$name.tar.gz"
rm -f "$DIST/.files"
rm -rf "$stage"

echo "архив           $DIST/$name.tar.gz ($(wc -c < "$DIST/$name.tar.gz" | tr -d ' ') байт)"
shasum -a 256 "$DIST/$name.tar.gz" | sed "s|$DIST/||" > "$DIST/$name.sha256"
sed 's/^/  /' "$DIST/$name.sha256"

# --- самопроверка ---------------------------------------------------------
#
# Проверяется не «файл собрался», а «файл делает своё дело» - и делает его
# из архива, а не из каталога сборки: битым бывает именно архив.
#
# Двух вопросов из трёх Accessibility не нужно, и это не случайность, а
# устройство точки входа: таблица клавиш и разбор настроек отвечают до того,
# как программа впервые спросит систему о правах. Третий - `-N` - как раз
# спрашивает, и на бегунке ему откажут; его вывод здесь не проверяется, а
# печатается целиком, потому что это первое, что мы вообще узнаём о живом маке.

probe=$(mktemp -d)
trap 'rm -rf "$probe"' EXIT INT TERM
tar -C "$probe" -xzf "$DIST/$name.tar.gz"
bin="$probe/$name/digitwm"

echo
echo "самопроверка (darwin/$goarch, из архива):"

[ -x "$bin" ] || {
	echo "build-release: в архиве нет исполняемого digitwm" >&2
	exit 1
}

# 1. Подпись на месте. Без неё macOS не запустит arm64 вовсе, а разрешение
#    Accessibility не к чему привязать: система помнит его по подписи.
codesign --verify --verbose=1 "$bin" 2>&1 | sed 's/^/  подпись: /'
codesign --verify "$bin" 2>/dev/null || {
	echo "build-release: двоичный файл не подписан - macOS не даст ему прав" >&2
	exit 1
}

# 2. Та ли это архитектура. Архив с чужим срезом внутри поставится и не
#    запустится, и человек узнает об этом от своей машины, а не от нас.
file "$bin" | sed 's/^/  /'
lipo -archs "$bin" | grep -qx "$arch" || {
	echo "build-release: в архиве не $arch, а $(lipo -archs "$bin")" >&2
	exit 1
}

# 3. Info.plist вшит в секцию __TEXT,__info_plist. Это единственная строка
#    macos/Makefile, написанная не по документации Apple (флаг -sectcreate),
#    и если она не сработала, узнать об этом надо здесь.
otool -s __TEXT __info_plist "$bin" >/dev/null 2>&1 &&
    otool -X -s __TEXT __info_plist "$bin" | grep -q . || {
	echo "build-release: секции __TEXT,__info_plist в файле нет" >&2
	exit 1
}
echo "  Info.plist: вшит в __TEXT,__info_plist"

# 4. Таблица клавиш. Разрешения не требует.
out=$("$bin" -k)
echo "$out" | grep -q 'bind-key' || {
	echo "build-release: -k не напечатал таблицу клавиш" >&2
	exit 1
}
echo "  -k: $(echo "$out" | grep -c '  bind-key') сочетаний в таблице"

# 5. Разбор настроек. Тоже без разрешения: файл читается до первого вопроса
#    системе о правах.
printf 'ribbongap 12\n' > "$probe/cwmrc"
out=$("$bin" -n -c "$probe/cwmrc")
echo "$out" | grep -q '1 taken' || {
	echo "build-release: -n не разобрал ribbongap 12" >&2
	echo "$out" >&2
	exit 1
}
echo "  -n: $(echo "$out" | head -1)"

# 6. И страница настроек обязана уехать в архиве: без неё формула поставит
#    двоичный файл, а `man 5 cwmrc` ответит «нет такой страницы».
grep -q '^\.Dt CWMRC 5$' "$probe/$name/cwmrc.5" || {
	echo "build-release: cwmrc.5 в архиве не объявляет себя страницей раздела 5" >&2
	exit 1
}
echo "  cwmrc.5: на месте, раздел 5"

echo
echo "готово: $DIST/$name.tar.gz"
