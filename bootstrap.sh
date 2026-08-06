#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# digitwm - одна команда от голой системы до работающей среды
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
# Здесь ровно то, чего не делает session/install.sh: ставит то, чем digitwm
# собирается - компилятор, yacc и заголовки трёх библиотек X, - и после этого
# передаёт ему работу. Разделение не косметическое: сборочные зависимости
# ставятся от root и знают про пакетный менеджер, а сессия раскладывается от
# пользователя и про него знать не обязана.
#
#   sh bootstrap.sh --plan          показать, что будет сделано, и выйти
#   sh bootstrap.sh                 поставить всё
#   sh bootstrap.sh --no-session    только оконный менеджер, без окружения
#   sh bootstrap.sh --prefix ~/.local --palette signal
#
# Всё, что не разобрано здесь, уходит в session/install.sh как есть.
#
# Совместимость: POSIX sh. Это не эстетика - на NetBSD и FreeBSD bash не
# входит в базовую систему, а bootstrap обязан работать до того, как что-либо
# поставлено.

set -eu

REPO_ROOT=$(cd -- "$(dirname -- "$0")" && pwd)
PLAN_ONLY=0
NO_SESSION=0
NO_PACKAGES=0
PREFIX="$HOME/.local"
SESSION_ARGS=""
SUDO_BIN=""

say() { printf '%s\n' "$*"; }
die() { printf 'digitwm: %s\n' "$*" >&2; exit 1; }

while [ $# -gt 0 ]; do
	case "$1" in
	--plan) PLAN_ONLY=1; SESSION_ARGS="$SESSION_ARGS --plan" ;;
	--no-session) NO_SESSION=1 ;;
	--prefix) [ $# -ge 2 ] || die "--prefix требует значение"
		PREFIX="$2"; SESSION_ARGS="$SESSION_ARGS --prefix $2"; shift ;;
	--prefix=*) PREFIX="${1#--prefix=}"; SESSION_ARGS="$SESSION_ARGS $1" ;;
	--no-packages) NO_PACKAGES=1 ;;
	-h|--help)
		cat <<'USAGE'
digitwm bootstrap - от голой системы до работающей среды одной командой.

  sh bootstrap.sh --plan          показать, что будет сделано, и выйти
  sh bootstrap.sh                 поставить всё
  sh bootstrap.sh --no-session    только оконный менеджер, без окружения
  sh bootstrap.sh --no-packages   не трогать пакетный менеджер
  sh bootstrap.sh --prefix ~/.local --palette signal

Всё, что не разобрано здесь, уходит в session/install.sh как есть:
--palette, --skip-install, --no-rc, --yes и остальные его ключи.
USAGE
		exit 0 ;;
	*) SESSION_ARGS="$SESSION_ARGS $1" ;;
	esac
	shift
done

# --- какая это система ------------------------------------------------------

os=$(uname -s 2>/dev/null || echo unknown)
manager="-"
packages=""
install_cmd=""

case "$os" in
Linux)
	if command -v pacman >/dev/null 2>&1; then
		manager="pacman"
		packages="base-devel libx11 libxft libxrandr bison pkgconf"
		install_cmd="pacman -S --needed --noconfirm"
	elif command -v apt-get >/dev/null 2>&1; then
		manager="apt"
		packages="build-essential libx11-dev libxft-dev libxrandr-dev bison pkg-config"
		install_cmd="env DEBIAN_FRONTEND=noninteractive apt-get install -y"
	elif command -v dnf >/dev/null 2>&1; then
		manager="dnf"
		packages="gcc make libX11-devel libXft-devel libXrandr-devel bison pkgconf-pkg-config"
		install_cmd="dnf install -y"
	elif command -v zypper >/dev/null 2>&1; then
		manager="zypper"
		packages="gcc make libX11-devel libXft-devel libXrandr-devel bison pkg-config"
		install_cmd="zypper --non-interactive install"
	fi
	;;
FreeBSD)
	manager="pkg"
	packages="libX11 libXft libXrandr bison pkgconf"
	install_cmd="pkg install -y"
	;;
NetBSD)
	# В базовой системе NetBSD X11 уже есть - ставится только то, чего нет.
	manager="pkgin"
	packages="bison pkgconf"
	install_cmd="pkgin -y install"
	;;
OpenBSD)
	# X11 и yacc в базе; ставить нечего, отсюда пустой список.
	manager="base"
	packages=""
	install_cmd=""
	;;
Darwin)
	die "на macOS оконному менеджеру X11 нечем управлять.
     Оболочка Workbench на маке упаковывается через Tauri - это другой
     репозиторий. Конфигурацию редактора и терминала можно разложить и здесь:
     session/install.sh --skip-install"
	;;
esac

[ "$manager" != "-" ] || die "не опознан пакетный менеджер для $os.
     Поставьте вручную: компилятор C, make, yacc/bison, pkg-config и
     заголовки x11, xft, xrandr - и запустите session/install.sh"

if [ "$(id -u 2>/dev/null || echo 0)" != "0" ] && command -v sudo >/dev/null 2>&1; then
	SUDO_BIN="sudo"
fi

# Ставить то, что уже стоит, - лишний повод спросить пароль. Спрашиваем у
# системы сборки ровно то, что ей нужно, а не наличие имён пакетов: имена
# везде разные, а заголовки одни.
have_build_deps() {
	command -v make >/dev/null 2>&1 || return 1
	{ command -v cc >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1; } || return 1
	{ command -v yacc >/dev/null 2>&1 || command -v bison >/dev/null 2>&1; } || return 1
	command -v pkg-config >/dev/null 2>&1 || return 1
	pkg-config --exists x11 xft xrandr 2>/dev/null || return 1
	return 0
}

if [ "$NO_PACKAGES" -eq 1 ]; then
	packages=""
elif have_build_deps; then
	packages=""
	DEPS_PRESENT=1
fi
DEPS_PRESENT=${DEPS_PRESENT:-0}

# --- план -------------------------------------------------------------------

say "== digitwm bootstrap =="
say "  система   : $os"
say "  пакеты    : $manager"
say "  префикс   : $PREFIX"
say ""

if [ -n "$packages" ]; then
	say "  [пакеты]  $SUDO_BIN $install_cmd $packages"
elif [ "$DEPS_PRESENT" -eq 1 ]; then
	say "  [пакеты]  ничего: компилятор, yacc, pkg-config и x11/xft/xrandr уже есть"
elif [ "$NO_PACKAGES" -eq 1 ]; then
	say "  [пакеты]  пропущены по --no-packages"
else
	say "  [пакеты]  ничего: всё нужное в базовой системе"
fi
say "  [сборка]  make && make install PREFIX=$PREFIX (в $REPO_ROOT)"
if [ "$NO_SESSION" -eq 0 ]; then
	say "  [сессия]  session/install.sh$SESSION_ARGS"
	say "            редактор, терминал, оболочка, мультиплексор, темы"
	say "            Digitable Focus и Digit - подробности в session/README.md"
fi
say ""

if [ "$PLAN_ONLY" -eq 1 ] && [ "$NO_SESSION" -eq 1 ]; then
	exit 0
fi

# --- сборочные зависимости --------------------------------------------------

if [ "$PLAN_ONLY" -eq 0 ] && [ -n "$packages" ]; then
	say "== Сборочные зависимости =="
	# shellcheck disable=SC2086
	$SUDO_BIN $install_cmd $packages || \
		die "пакеты не поставились. Поставьте их сами и запустите снова"
	say ""
fi

# --- оконный менеджер -------------------------------------------------------

if [ "$PLAN_ONLY" -eq 0 ]; then
	say "== Оконный менеджер =="
	command -v make >/dev/null 2>&1 || die "нет make"
	command -v cc >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1 || die "нет компилятора C"
	( cd "$REPO_ROOT" && make ) || die "сборка не прошла"
	( cd "$REPO_ROOT" && make install PREFIX="$PREFIX" ) || \
		die "установка в $PREFIX не прошла"
	say "  установлен: $PREFIX/bin/cwm"
	say ""
fi

# --- окружение --------------------------------------------------------------

if [ "$NO_SESSION" -eq 0 ]; then
	[ -x "$REPO_ROOT/session/install.sh" ] || \
		die "нет session/install.sh - репозиторий неполон"
	# Установщику сессии нужен bash: он рассчитан на bash 3.2 и пользуется
	# массивами. Здесь его нет только потому, что bootstrap работает раньше.
	command -v bash >/dev/null 2>&1 || \
		die "для session/install.sh нужен bash; сам оконный менеджер уже собран"
	# shellcheck disable=SC2086
	bash "$REPO_ROOT/session/install.sh" $SESSION_ARGS
fi

if [ "$PLAN_ONLY" -eq 1 ]; then
	say "Это был план. Без --plan будет сделано перечисленное выше."
fi
