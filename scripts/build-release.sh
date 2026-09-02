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
appname="digitwm-app-$version-darwin-$goarch"
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
    digitwm.1 cwmrc.5 "$stage/"
mkdir -p "$stage/doc"
cp doc/macos-install.md doc/macos-install.ru.md "$stage/doc/"

# pack <имя каталога в dist> - собрать из него .tar.gz, повторимо
pack() {
	find "$DIST/$1" -print0 | xargs -0 env TZ=UTC touch -t "$stamp"
	(cd "$DIST" && find "$1" | LC_ALL=C sort > "$DIST/.files")
	COPYFILE_DISABLE=1 tar --format ustar --uid 0 --gid 0 --numeric-owner \
	    --no-mac-metadata -n -C "$DIST" -T "$DIST/.files" -cf - \
	    | gzip -9 -n > "$DIST/$1.tar.gz"
	rm -f "$DIST/.files"
	rm -rf "$DIST/$1"
	echo "архив           $DIST/$1.tar.gz ($(wc -c < "$DIST/$1.tar.gz" | tr -d ' ') байт)"
	shasum -a 256 "$DIST/$1.tar.gz" | sed "s|$DIST/||" > "$DIST/$1.sha256"
	sed 's/^/  /' "$DIST/$1.sha256"
}

# --- второй архив: тот же файл в пакете .app ------------------------------
#
# Пакет собирается ПОСЛЕ повторимой сборки и из неё же: внутри лежит тот самый
# двоичный файл, байт в байт. Отдельным архивом, а не в том же, потому что это
# два разных способа поставить одну программу, и человек выбирает один.
#
# Зачем он вообще: запущенный из терминала digitwm разрешения Accessibility на
# себя не получает - система приписывает его тому, кто запустил. Владелец
# наступил на это первым же запуском на настоящем маке 2 сентября 2026.
# Приложение из /Applications - само себе хозяин.

appstage="$DIST/$appname"
mkdir -p "$appstage"
printf '%-28s' "пакет .app"
make -C "$root/macos" \
    OBJDIR="$DIST/.objapp" \
    PROG="$stage/digitwm" \
    APPDIR="$appstage/digitwm.app" \
    CFLAGS="$release_cflags" \
    app >"$DIST/.app.log" 2>&1 || {
	echo
	echo "build-release: пакет не собрался:" >&2
	cat "$DIST/.app.log" >&2
	exit 1
}
rm -rf "$DIST/.objapp" "$DIST/.app.log"
echo "собран, подпись проверена --deep --strict"
cp LICENSE LICENSE.upstream NOTICE VERSION digitwm.1 cwmrc.5 "$appstage/"
mkdir -p "$appstage/doc"
cp doc/macos-install.md doc/macos-install.ru.md "$appstage/doc/"

# ОТПЕЧАТОК ПОДПИСИ - число, ради которого стоит смотреть. Разрешение
# Accessibility система помнит по нему; одинаковый отпечаток у двух сборок
# одного коммита значит, что переустановка той же версии разрешения не теряет.
echo
echo "отпечаток подписи (cdhash) - по нему macOS помнит разрешение:"
codesign -d --verbose=4 "$stage/digitwm" 2>&1 | grep -i 'CDHash\|Signature=' | sed 's/^/  /'

pack "$name"
echo
pack "$appname"

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
# Проверяется КОПИЯ в пустом каталоге, а не файл на месте: рядом с ним лежат
# лицензии и страница руководства, а codesign от вшитого Info.plist начинает
# разговаривать про пакет и его печать. Вопрос, на который надо ответить,
# другой: переживает ли подпись переезд? Формула именно переезд и делает.
lone=$(mktemp -d)
cp "$bin" "$lone/digitwm"
codesign -dv "$lone/digitwm" 2>&1 | sed 's/^/  подпись: /'
codesign -dv "$lone/digitwm" 2>&1 | grep -q 'Signature=adhoc\|Authority=' || {
	echo "build-release: двоичный файл не подписан - macOS не даст ему прав" >&2
	exit 1
}
codesign --verify --verbose=1 "$lone/digitwm" 2>&1 | sed 's/^/  сверка:  /' || true
rm -rf "$lone"

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
# 6. И обе страницы руководства обязаны уехать в архиве: без них формула
#    поставит двоичный файл, а `man digitwm` ответит «нет такой страницы».
#    Страниц две, потому что вопросов два: digitwm(1) - про сам инструмент,
#    его ключи и разрешение Accessibility; cwmrc(5) - про файл настроек.
grep -q '^\.Dt CWMRC 5$' "$probe/$name/cwmrc.5" || {
	echo "build-release: cwmrc.5 в архиве не объявляет себя страницей раздела 5" >&2
	exit 1
}
grep -q '^\.Dt DIGITWM 1$' "$probe/$name/digitwm.1" || {
	echo "build-release: digitwm.1 в архиве не объявляет себя страницей раздела 1" >&2
	exit 1
}
echo "  руководство: digitwm.1 (раздел 1) и cwmrc.5 (раздел 5) на месте"

# 7. И пакет: тот же двоичный файл внутри отвечает так же, подпись пакета
#    цела, LSUIElement на месте - без него в Dock завелась бы иконка
#    программы, у которой нет ни одного своего окна.
tar -C "$probe" -xzf "$DIST/$appname.tar.gz"
appbin="$probe/$appname/digitwm.app/Contents/MacOS/digitwm"
[ -x "$appbin" ] || {
	echo "build-release: в пакете нет исполняемого файла" >&2
	exit 1
}
codesign --verify --deep --strict "$probe/$appname/digitwm.app" || {
	echo "build-release: подпись пакета не пережила упаковку" >&2
	exit 1
}
/usr/libexec/PlistBuddy -c "Print :LSUIElement" \
    "$probe/$appname/digitwm.app/Contents/Info.plist" | grep -qx true || {
	echo "build-release: в пакете нет LSUIElement - он заведёт иконку в Dock" >&2
	exit 1
}
"$appbin" -k | grep -q 'bind-key' || {
	echo "build-release: файл в пакете не отвечает на -k" >&2
	exit 1
}
# ТОТ ЖЕ ЛИ ЭТО ФАЙЛ. Побайтно - НЕТ, и это не поломка: подписывая пакет,
# codesign переподписывает лежащий внутри Mach-O, и в его подпись входит хеш
# Info.plist пакета и печать ресурсов. Значит различаются подписи, а программа
# обязана быть та же. Сверяются копии со СНЯТОЙ подписью - это и есть вопрос
# «одна ли это программа», заданный точно.
#
# Следствие, которое стоит знать заранее: cdhash у файла и у пакета РАЗНЫЕ, а
# разрешение Accessibility система помнит по cdhash. То есть разрешение,
# выданное digitwm рядом с вами, и разрешение, выданное digitwm.app, - два
# разных разрешения, и одно не считается за другое.
# Сверяется СЕКЦИЯ КОДА, а не файл целиком. Первая попытка сверяла файлы со
# снятой подписью, и на x86-64 они разошлись при одинаковом размере:
# `codesign --remove-signature` оставляет место подписи, а не байты, и добивка
# у подписанного и у неподписанного разная. Секция __TEXT,__text - это сами
# команды процессора, и подпись их не трогает ни на одной архитектуре.
code_a=$(otool -X -s __TEXT __text "$bin" | shasum -a 256 | cut -d' ' -f1)
code_b=$(otool -X -s __TEXT __text "$appbin" | shasum -a 256 | cut -d' ' -f1)
if [ "$code_a" != "$code_b" ]; then
	echo "build-release: в пакете ДРУГАЯ программа, а не другая подпись:" >&2
	echo "  файл  __TEXT,__text $code_a" >&2
	echo "  пакет __TEXT,__text $code_b" >&2
	exit 1
fi
echo "  пакет: подпись цела, LSUIElement=true, код тот же (__TEXT,__text $code_a)"
echo "  (cdhash файла и пакета разные - это два разных разрешения в системе)"

echo
echo "готово: $DIST/$name.tar.gz"
echo "        $DIST/$appname.tar.gz"
