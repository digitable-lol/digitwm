#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm - свести два архива в контрольные суммы и заполненную формулу.
#
#   sh scripts/make-formula.sh [версия]
#
# Ждёт, что в dist/ уже лежат ОБА архива выпуска - digitwm-<версия>-darwin-arm64
# .tar.gz и -darwin-amd64.tar.gz. Их кладут туда два прогона
# scripts/build-release.sh на двух бегунках: этот скрипт ничего не собирает, он
# только сводит.
#
# Пишет:
#   dist/SHA256SUMS         - отпечатки обоих архивов
#   dist/homebrew/digitwm.rb - формула с подставленными версией и отпечатками
#
# ОТПЕЧАТОК НЕ ВПИСЫВАЕТСЯ РУКАМИ. Он снимается с тех самых байтов, которые
# уезжают в выпуск, - здесь, а не в build-release.sh, потому что там архив ещё
# не проехал через выгрузку и загрузку артефакта. Если по дороге что-то
# испортилось, отпечаток в формуле будет отпечатком испорченного, и brew
# поставит его молча; поэтому сумма снимается с последней доступной копии.
#
# Незаполненное место останавливает выпуск: `grep PLACEHOLDER` в конце.

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

DIST="$root/dist"
version=${1:-${DIGITWM_VERSION:-}}
if [ -z "$version" ]; then
	[ -f VERSION ] || {
		echo "make-formula: нет файла VERSION и версия не задана" >&2
		exit 1
	}
	version=$(tr -d ' \t\n\r' < VERSION)
fi
version=${version#v}

# sha256 <файл> - одна и та же сумма на macOS и на Linux
sha256() {
	if command -v sha256sum >/dev/null 2>&1; then
		sha256sum "$1" | cut -d' ' -f1
	else
		shasum -a 256 "$1" | cut -d' ' -f1
	fi
}

arm="digitwm-$version-darwin-arm64.tar.gz"
amd="digitwm-$version-darwin-amd64.tar.gz"

for f in "$arm" "$amd"; do
	[ -f "$DIST/$f" ] || {
		echo "make-formula: нет $DIST/$f" >&2
		echo "              выпуск - это ОБЕ архитектуры; одна из них не собралась" >&2
		echo "              или её артефакт не доехал." >&2
		exit 1
	}
done

sha_arm=$(sha256 "$DIST/$arm")
sha_amd=$(sha256 "$DIST/$amd")

{
	echo "$sha_arm  $arm"
	echo "$sha_amd  $amd"
} > "$DIST/SHA256SUMS"

echo "контрольные суммы:"
sed 's/^/  /' "$DIST/SHA256SUMS"
echo
echo "размеры:"
for f in "$arm" "$amd"; do
	echo "  $f  $(wc -c < "$DIST/$f" | tr -d ' ') байт"
done

mkdir -p "$DIST/homebrew"
sed -e "s/VERSION_PLACEHOLDER/$version/g" \
    -e "s/SHA256_MACOS_ARM64_PLACEHOLDER/$sha_arm/" \
    -e "s/SHA256_MACOS_AMD64_PLACEHOLDER/$sha_amd/" \
    macos/digitwm.rb > "$DIST/homebrew/digitwm.rb"

if grep -q PLACEHOLDER "$DIST/homebrew/digitwm.rb"; then
	echo "make-formula: в формуле остались незаполненные места:" >&2
	grep -n PLACEHOLDER "$DIST/homebrew/digitwm.rb" >&2
	exit 1
fi

# Синтаксис - обязательно, и здесь, а не на чужой машине: ruby есть и на
# маковском бегунке, и на линуксовом, а формула с опечаткой ломается только у
# того, кто её уже поставил.
ruby -c "$DIST/homebrew/digitwm.rb" >/dev/null || {
	echo "make-formula: формула не разбирается ruby" >&2
	exit 1
}

echo
echo "формула:  dist/homebrew/digitwm.rb  -> Formula/digitwm.rb в digitable-lol/homebrew-tap"
echo "          версия $version, синтаксис проверен ruby -c"
