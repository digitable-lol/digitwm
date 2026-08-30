#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# Digitable Session — установка окружения вокруг digitwm.
#
# Ставит оконный менеджер, раскладывает конфигурацию редактора, терминала,
# мультиплексора и оболочки, подключает темы Digitable Focus и заводит Digit
# в сессии. Набор инструментов ставится общим установщиком Workbench
# (products/workbench/toolchain/bootstrap.sh), если он найден.
#
#   session/install.sh --plan                     показать план и выйти
#   session/install.sh --palette carbon            поставить всё
#   session/install.sh --skip-install              только конфигурация
#   session/install.sh --no-rc                     не трогать ~/.zshrc и прочие
#   session/install.sh --help                      все флаги
#
# ЧТО ЭТОТ УСТАНОВЩИК НЕ ДЕЛАЕТ И НЕ БУДЕТ:
# он не кладёт в репозиторий digitwm ни одного чужого бинарника и ни одной
# чужой строки кода. vim, tmux, alacritty, zsh, fzf, bat живут под своими
# лицензиями и ставятся пакетным менеджером. Наше здесь — конфигурация и
# склейка. Подробности и причина — в session/README.md.
#
# Рассчитан на bash 3.2 (штатный bash в macOS): без ассоциативных массивов,
# mapfile и ${var^}.

set -eu
set -o pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)

PALETTE="carbon"
PALETTE_CAP="Carbon"
PREFIX="$HOME/.local"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/digitwm"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/digitwm"
TERMINAL=""
FONT_FAMILY=""
FONT_SIZE="12"
PALETTE_FILE=""
THEMES_DIR=""
THEMES_SOURCE=""
PALETTE_SOURCE=""
BOOTSTRAP=""
FETCH_CONFIGS=0
CONFIGS_URL="https://courses.digitable.life/workbench/configs"
JSON_TOOL=""
PLAN_ONLY=0
SKIP_INSTALL=0
SKIP_BOOTSTRAP=0
SKIP_BUILD=0
SKIP_THEMES=0
TOUCH_RC=1
SYSTEM_DESKTOP=0
WITH_DIGIT=0
ASSUME_YES=0

OS_NAME=""
PKG_MANAGER="-"
SUDO_BIN=""
STAMP=$(date +%Y%m%d-%H%M%S)
LOG_FILE="${TMPDIR:-/tmp}/digitwm-session-$STAMP.log"
MANIFEST="$STATE_DIR/manifest.tsv"

PLAN_LINES=()
DONE_WRITTEN=()
DONE_UNCHANGED=()
DONE_BACKUPS=()
DONE_THEMES=()
DONE_SKIPPED=()
NEXT_STEPS=()

# Инструменты, которые сессия действительно использует. Список идёт в
# bootstrap.sh флагом --tools: он умеет ставить и те, что помечены optional.
SESSION_TOOLS="git,zsh,zsh-syntax-highlighting,starship,ripgrep,fd,fzf,zoxide,eza,bat,jq,delta,tmux,alacritty,vim,neovim,btop,dust,duf,lazygit,lf,python3,fira-code,meslo-lgs-nf"

die() { printf 'install: %s\n' "$1" >&2; exit 1; }
say() { printf '%s\n' "$1"; }
warn() { printf 'install: %s\n' "$1" >&2; }

usage() {
  cat <<'USAGE'
Digitable Session — установка окружения вокруг digitwm.

Использование: session/install.sh [флаги]

  --plan, --dry-run, -n   показать план и выйти, ничего не меняя
  --palette NAME          carbon | paper | signal (по умолчанию carbon)
  --prefix DIR            куда класть скрипты сессии (по умолчанию ~/.local)
  --terminal NAME         alacritty | kitty | xterm (по умолчанию — что найдётся)
  --font FAMILY           семейство шрифта (по умолчанию MesloLGS NF)
  --font-size N           кегль терминала (по умолчанию 12)
  --themes-dir DIR        каталог themes/ Workbench (сгенерированный или из архива)
  --palette-file FILE     путь к focus-palettes.json
  --fetch-configs         скачать темы и палитру с открытого портала, ЕСЛИ их
                          не нашлось на диске (без флага — ни одного запроса)
  --configs-url URL       откуда именно скачивать (по умолчанию портал Digitable)
  --bootstrap FILE        путь к products/workbench/toolchain/bootstrap.sh
  --skip-bootstrap        не звать общий bootstrap Workbench
  --skip-install          не ставить пакеты вообще
  --skip-build            не собирать и не ставить оконный менеджер
  --skip-themes           не раскладывать файлы тем
  --no-rc                 не трогать ~/.zshrc, ~/.vimrc, ~/.tmux.conf
  --system                положить .desktop в /usr/share/xsessions (нужен root)
  --with-digit            поставить Digit официальным установщиком и выполнить digit setup
  --yes, -y               не спрашивать подтверждения
  --help, -h              эта справка

Откуда берутся темы, три источника, в порядке предпочтения:

  1. локальный клон Workbench рядом с репозиторием или в ~/projects/courses;
  2. --themes-dir и --palette-file — любой каталог, который вы укажете;
  3. --fetch-configs — открытый портал, только по этому флагу и только когда
     первых двух нет. Без флага установщик в сеть не ходит вовсе.

Что выбрано в этом запуске, печатает строка «темы из» — и в --plan тоже.

Идемпотентность. Повторный запуск ничего не портит: совпадающие файлы не
переписываются, отличающиеся сохраняются рядом с суффиксом
.digitable-backup-<метка времени>. В пользовательские rc-файлы дописывается
один помеченный блок, и при повторе он заменяется целиком, а не дублируется.
USAGE
}

parse_args() {
  while [ $# -gt 0 ]; do
    case "$1" in
      --plan | --dry-run | -n) PLAN_ONLY=1 ;;
      --palette) [ $# -ge 2 ] || die "--palette требует значение"; PALETTE="$2"; shift ;;
      --palette=*) PALETTE="${1#--palette=}" ;;
      --prefix) [ $# -ge 2 ] || die "--prefix требует значение"; PREFIX="$2"; shift ;;
      --prefix=*) PREFIX="${1#--prefix=}" ;;
      --terminal) [ $# -ge 2 ] || die "--terminal требует значение"; TERMINAL="$2"; shift ;;
      --terminal=*) TERMINAL="${1#--terminal=}" ;;
      --font) [ $# -ge 2 ] || die "--font требует значение"; FONT_FAMILY="$2"; shift ;;
      --font=*) FONT_FAMILY="${1#--font=}" ;;
      --font-size) [ $# -ge 2 ] || die "--font-size требует значение"; FONT_SIZE="$2"; shift ;;
      --font-size=*) FONT_SIZE="${1#--font-size=}" ;;
      --themes-dir) [ $# -ge 2 ] || die "--themes-dir требует значение"; THEMES_DIR="$2"; shift ;;
      --themes-dir=*) THEMES_DIR="${1#--themes-dir=}" ;;
      --palette-file) [ $# -ge 2 ] || die "--palette-file требует значение"; PALETTE_FILE="$2"; shift ;;
      --palette-file=*) PALETTE_FILE="${1#--palette-file=}" ;;
      --bootstrap) [ $# -ge 2 ] || die "--bootstrap требует значение"; BOOTSTRAP="$2"; shift ;;
      --bootstrap=*) BOOTSTRAP="${1#--bootstrap=}" ;;
      --fetch-configs) FETCH_CONFIGS=1 ;;
      --configs-url) [ $# -ge 2 ] || die "--configs-url требует значение"; CONFIGS_URL="$2"; FETCH_CONFIGS=1; shift ;;
      --configs-url=*) CONFIGS_URL="${1#--configs-url=}"; FETCH_CONFIGS=1 ;;
      --skip-bootstrap) SKIP_BOOTSTRAP=1 ;;
      --skip-install) SKIP_INSTALL=1 ;;
      --skip-build) SKIP_BUILD=1 ;;
      --skip-themes) SKIP_THEMES=1 ;;
      --no-rc) TOUCH_RC=0 ;;
      --system) SYSTEM_DESKTOP=1 ;;
      --with-digit) WITH_DIGIT=1 ;;
      --yes | -y) ASSUME_YES=1 ;;
      --help | -h) usage; exit 0 ;;
      *) die "неизвестный аргумент: $1 (см. --help)" ;;
    esac
    shift
  done

  case "$PALETTE" in
    carbon) PALETTE_CAP="Carbon" ;;
    paper) PALETTE_CAP="Paper" ;;
    signal) PALETTE_CAP="Signal" ;;
    *) die "неизвестная палитра: $PALETTE (carbon, paper, signal)" ;;
  esac
}

detect_platform() {
  case "$(uname -s 2>/dev/null || echo unknown)" in
    Darwin) OS_NAME="macos" ;;
    Linux) OS_NAME="linux" ;;
    FreeBSD) OS_NAME="freebsd" ;;
    NetBSD) OS_NAME="netbsd" ;;
    OpenBSD) OS_NAME="openbsd" ;;
    *) OS_NAME="linux" ;;
  esac

  if command -v brew >/dev/null 2>&1; then PKG_MANAGER="brew"
  elif command -v apt-get >/dev/null 2>&1; then PKG_MANAGER="apt"
  elif command -v dnf >/dev/null 2>&1; then PKG_MANAGER="dnf"
  elif command -v pacman >/dev/null 2>&1; then PKG_MANAGER="pacman"
  elif command -v pkgin >/dev/null 2>&1; then PKG_MANAGER="pkgin"
  elif command -v pkg >/dev/null 2>&1 && [ "$OS_NAME" = "freebsd" ]; then PKG_MANAGER="pkg"
  else PKG_MANAGER="-"
  fi

  if [ "$(id -u 2>/dev/null || echo 0)" != "0" ] && command -v sudo >/dev/null 2>&1; then
    SUDO_BIN="sudo"
  fi
}

# --- поиск Workbench --------------------------------------------------------
# Палитра и темы живут в репозитории портала. Ищем в очевидных местах, но
# ничего не скачиваем из сети: молчаливая загрузка чужого файла установщиком —
# ровно то, за что ругают curl | sh.

# Тринадцать целей, которые сессия раскладывает (install_all_themes). Имена
# совпадают с идентификаторами целей на портале — проверено по его index.json.
CONFIG_TARGETS="vim neovim tmux alacritty kitty zsh starship fzf eza bat btop delta lazygit"

fetch_url() {
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$1" -o "$2"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "$2" "$1"
  else
    return 127
  fi
}

# json_items <файл> [путь]
#   без пути  — печатает пути всех файлов выбранной палитры и общих;
#   с путём   — печатает текст ровно этого файла, без лишнего перевода строки.
json_items() {
  case "$JSON_TOOL" in
    jq)
      if [ $# -ge 2 ]; then
        jq -j --arg p "$PALETTE" --arg f "$2" \
          '([.shared[]?] + [.palettes[]?|select(.id==$p)|.files[]?])[]|select(.path==$f)|.text' "$1"
      else
        jq -r --arg p "$PALETTE" \
          '([.shared[]?] + [.palettes[]?|select(.id==$p)|.files[]?])[]|.path' "$1"
      fi
      ;;
    *)
      python3 -c '
import json, sys
data = json.load(open(sys.argv[1], encoding="utf-8"))
items = list(data.get("shared") or [])
for pal in data.get("palettes") or []:
    if pal.get("id") == sys.argv[2]:
        items += pal.get("files") or []
if len(sys.argv) > 3:
    for item in items:
        if item.get("path") == sys.argv[3]:
            sys.stdout.write(item.get("text", ""))
            break
else:
    for item in items:
        print(item.get("path", ""))
' "$1" "$PALETTE" ${2+"$2"}
      ;;
  esac
}

# --- открытый источник конфигов --------------------------------------------
# Только по явному флагу и только когда на диске ничего не нашлось. Правило,
# ради которого этот блок написан именно так, — тремя абзацами выше: молчаливая
# загрузка чужого файла установщиком есть ровно то, за что ругают curl | sh.
# Без --fetch-configs отсюда не уходит ни одного запроса.
#
# Отдельного focus-palettes.json на портале нет (404 — проверено). Палитра там
# при этом есть целиком: тема helix несёт блок [palette] с ровно теми именами
# ключей, которых ждёт этот установщик, а два терминальных чёрных лежат в теме
# alacritty — colors.normal.black и colors.bright.black. Собранный так файл
# сверен с products/workbench/themes/focus-palettes.json: 17 ключей на каждую
# из трёх палитр совпадают до символа. Если формат портала изменится, палитра
# соберётся неполной, и об этом будет сказано, а не подставлено молча.
fetch_configs() {
  local dest="$STATE_DIR/workbench-configs"
  local target path json count=0

  JSON_TOOL=""
  if command -v jq >/dev/null 2>&1; then JSON_TOOL=jq
  elif command -v python3 >/dev/null 2>&1; then JSON_TOOL=python3
  else die "--fetch-configs: нужен jq или python3, чтобы разобрать конфиги портала"
  fi
  command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1 \
    || die "--fetch-configs: нужен curl или wget"

  say ""
  say "== Конфигурации Workbench с портала =="
  say "  источник: $CONFIGS_URL"
  say "  палитра:  $PALETTE"
  mkdir -p -- "$dest"

  for target in $CONFIG_TARGETS; do
    json="$dest/$target.json"
    if ! fetch_url "$CONFIGS_URL/$target.json" "$json"; then
      warn "не скачалось: $CONFIGS_URL/$target.json"
      rm -f -- "$json"
      continue
    fi
    while IFS= read -r path; do
      [ -n "$path" ] || continue
      mkdir -p -- "$dest/$target/$(dirname -- "$path")"
      json_items "$json" "$path" >"$dest/$target/$path"
      count=$((count + 1))
    done <<INNER
$(json_items "$json")
INNER
  done

  if [ "$count" -eq 0 ]; then
    warn "с $CONFIGS_URL не удалось взять ни одного файла темы"
    return 1
  fi
  say "  файлов тем: $count"

  fetch_palette "$dest" || return 1

  THEMES_DIR="$dest"
  PALETTE_FILE="$dest/focus-palettes.json"
  return 0
}

# fetch_palette <каталог> — собирает focus-palettes.json из тем helix и
# alacritty. Ни одного шестнадцатеричного кода здесь не написано: все
# приходят с портала.
fetch_palette() {
  local dest="$1"
  local helix="$dest/helix.json"
  local alacritty="$dest/alacritty.json"
  local theme_file="digitable-focus-$PALETTE.toml"

  fetch_url "$CONFIGS_URL/helix.json" "$helix" || { warn "не скачалась тема helix — палитру собрать не из чего"; return 1; }
  [ -f "$alacritty" ] || fetch_url "$CONFIGS_URL/alacritty.json" "$alacritty" \
    || { warn "не скачалась тема alacritty — палитру собрать не из чего"; return 1; }

  local body
  body=$(json_items "$helix" "$theme_file" \
    | sed -n '/^\[palette\]/,$p' \
    | sed -n 's/^\([A-Za-z]*\)[[:space:]]*=[[:space:]]*"\(#[0-9A-Fa-f]\{6,8\}\)".*/  "\1": "\2",/p')
  [ -n "$body" ] || { warn "в теме helix нет блока [palette] — формат портала изменился"; return 1; }

  local black bright
  black=$(json_items "$alacritty" "$theme_file" | sed -n '/^\[colors\.normal\]/,/^\[colors\.bright\]/p' \
    | sed -n 's/^black[[:space:]]*=[[:space:]]*"\(#[0-9A-Fa-f]\{6\}\)".*/\1/p' | head -n 1)
  bright=$(json_items "$alacritty" "$theme_file" | sed -n '/^\[colors\.bright\]/,$p' \
    | sed -n 's/^black[[:space:]]*=[[:space:]]*"\(#[0-9A-Fa-f]\{6\}\)".*/\1/p' | head -n 1)
  if [ -z "$black" ] || [ -z "$bright" ]; then
    warn "в теме alacritty не нашлись терминальные чёрные — формат портала изменился"
    return 1
  fi

  {
    printf '{\n'
    printf '  "version": "%s",\n' "собрано session/install.sh --fetch-configs из $CONFIGS_URL"
    printf '  "palettes": {\n'
    printf '    "%s": {\n' "$PALETTE"
    printf '%s\n' "$body" | sed 's/^  /      /'
    printf '      "terminalBlack": "%s",\n' "$black"
    printf '      "terminalBrightBlack": "%s"\n' "$bright"
    printf '    }\n'
    printf '  }\n'
    printf '}\n'
  } >"$dest/focus-palettes.json"
  say "  палитра $PALETTE собрана из тем helix и alacritty"
  return 0
}

resolve_workbench() {
  local candidate

  if [ -z "$BOOTSTRAP" ]; then
    for candidate in \
      "$REPO_ROOT/../courses/products/workbench/toolchain/bootstrap.sh" \
      "$HOME/projects/courses/products/workbench/toolchain/bootstrap.sh" \
      "$HOME/courses/products/workbench/toolchain/bootstrap.sh" \
      "$HOME/.local/share/digitable-workbench/toolchain/bootstrap.sh"; do
      if [ -f "$candidate" ]; then BOOTSTRAP=$(cd -- "$(dirname -- "$candidate")" && pwd)/bootstrap.sh; break; fi
    done
  fi

  [ -z "$THEMES_DIR" ] || THEMES_SOURCE="указан флагом --themes-dir"
  [ -z "$PALETTE_FILE" ] || PALETTE_SOURCE="указан флагом --palette-file"

  if [ -z "$THEMES_DIR" ] && [ -n "$BOOTSTRAP" ]; then
    local product_root
    product_root=$(cd -- "$(dirname -- "$BOOTSTRAP")/.." && pwd)
    for candidate in "$product_root/themes" "$product_root/tmp/generated/themes"; do
      if [ -d "$candidate/vim" ]; then THEMES_DIR="$candidate"; THEMES_SOURCE="локальный клон Workbench"; break; fi
    done
  fi
  if [ -z "$THEMES_DIR" ]; then
    for candidate in \
      "$HOME/.local/share/digitable-workbench/themes" \
      "$REPO_ROOT/../courses/products/workbench/tmp/generated/themes"; do
      if [ -d "$candidate/vim" ]; then THEMES_DIR="$candidate"; THEMES_SOURCE="локальный клон Workbench"; break; fi
    done
  fi

  if [ -z "$PALETTE_FILE" ]; then
    for candidate in \
      "$THEMES_DIR/focus-palettes.json" \
      "$REPO_ROOT/../courses/products/workbench/themes/focus-palettes.json" \
      "$HOME/projects/courses/products/workbench/themes/focus-palettes.json" \
      "$HOME/.local/share/digitable-workbench/themes/focus-palettes.json"; do
      if [ -n "$candidate" ] && [ -f "$candidate" ]; then
        PALETTE_FILE="$candidate"
        PALETTE_SOURCE="локальный клон Workbench"
        break
      fi
    done
  fi

  # Третий источник — открытый портал, и только если первых двух не нашлось.
  # По умолчанию сюда не заходят никогда: нужен явный --fetch-configs.
  if [ -z "$THEMES_DIR" ] && [ "$FETCH_CONFIGS" -eq 1 ]; then
    if [ "$PLAN_ONLY" -eq 1 ]; then
      THEMES_SOURCE="открытый портал $CONFIGS_URL (--fetch-configs)"
      PALETTE_SOURCE="$THEMES_SOURCE, собирается из тем helix и alacritty"
      PLAN_LINES+=("  [сеть]     скачать 13 целей и палитру $PALETTE с $CONFIGS_URL")
      PLAN_LINES+=("  [кэш]      $STATE_DIR/workbench-configs")
    elif fetch_configs; then
      THEMES_SOURCE="открытый портал $CONFIGS_URL (--fetch-configs)"
      PALETTE_SOURCE="$THEMES_SOURCE, собрана из тем helix и alacritty"
    else
      NEXT_STEPS+=("--fetch-configs ничего не принёс с $CONFIGS_URL. Темы не разложены: укажите каталог флагом --themes-dir.")
    fi
  fi

  if [ -z "$THEMES_DIR" ] && [ "$FETCH_CONFIGS" -eq 0 ]; then
    NEXT_STEPS+=("Тем Workbench на диске нет. Три источника: локальный клон courses рядом с репозиторием; свой каталог через --themes-dir; открытый портал через --fetch-configs — по флагу, молча в сеть установщик не ходит.")
  fi

  [ -z "$PALETTE_FILE" ] || [ -f "$PALETTE_FILE" ] || die "не найден файл палитры: $PALETTE_FILE"
}

# Читает одно поле палитры. Пусто — если палитры нет.
PALETTE_KEYS="background surface surfaceRaised foreground muted subtle border accent accentSoft blue green yellow orange purple red terminalBlack terminalBrightBlack"
PALETTE_VALUES=""

load_palette() {
  [ -n "$PALETTE_FILE" ] || return 0
  if command -v jq >/dev/null 2>&1; then
    PALETTE_VALUES=$(jq -r --arg p "$PALETTE" --arg keys "$PALETTE_KEYS" '
      .palettes[$p] as $pal
      | ($keys | split(" "))
      | map(. + "=" + ($pal[.] // ""))
      | join("\n")' "$PALETTE_FILE") || die "не удалось прочитать $PALETTE_FILE"
  elif command -v python3 >/dev/null 2>&1; then
    PALETTE_VALUES=$(python3 -c '
import json, sys
path, name, keys = sys.argv[1], sys.argv[2], sys.argv[3].split()
with open(path, encoding="utf-8") as handle:
    data = json.load(handle)
palette = data["palettes"][name]
print("\n".join("%s=%s" % (k, palette.get(k, "")) for k in keys))
' "$PALETTE_FILE" "$PALETTE" "$PALETTE_KEYS") || die "не удалось прочитать $PALETTE_FILE"
  else
    warn "нет ни jq, ни python3 — палитра не прочитана, цвета останутся стандартными"
    PALETTE_FILE=""
    return 0
  fi
  # Цвет вида #RRGGBBAA обрезаем до #RRGGBB: X11 не понимает альфу.
  PALETTE_VALUES=$(printf '%s\n' "$PALETTE_VALUES" | sed 's/\(=#[0-9A-Fa-f]\{6\}\)[0-9A-Fa-f]\{2\}$/\1/')
}

palette_get() {
  [ -n "$PALETTE_VALUES" ] || { printf ''; return 0; }
  printf '%s\n' "$PALETTE_VALUES" | sed -n "s/^$1=//p" | head -n 1
}

# --- выбор терминала и шрифта ----------------------------------------------

resolve_terminal() {
  if [ -z "$TERMINAL" ]; then
    local candidate
    for candidate in alacritty kitty xterm; do
      if command -v "$candidate" >/dev/null 2>&1; then TERMINAL="$candidate"; break; fi
    done
    # Ничего не нашли — всё равно пишем alacritty: его поставит bootstrap,
    # а конфигурация должна быть согласованной, а не пустой.
    [ -n "$TERMINAL" ] || TERMINAL="alacritty"
  fi
  case "$TERMINAL" in
    kitty) TERMINAL_EXEC="$TERMINAL" ;;
    *) TERMINAL_EXEC="$TERMINAL -e" ;;
  esac
  [ -n "$FONT_FAMILY" ] || FONT_FAMILY="MesloLGS NF"
}

# --- запись файлов ----------------------------------------------------------

record() {
  [ "$PLAN_ONLY" -eq 1 ] && return 0
  mkdir -p -- "$(dirname -- "$MANIFEST")"
  printf '%s\t%s\t%s\n' "$1" "$2" "${3:-}" >>"$MANIFEST"
}

backup_file() {
  local target="$1"
  local backup="$target.digitable-backup-$STAMP"
  cp -- "$target" "$backup"
  DONE_BACKUPS+=("$backup")
  record backup "$backup" "$target"
}

# place_file <источник> <назначение> <режим> — кладёт готовый файл.
# Намеренно без конвейеров: `render ... | place` увёл бы функцию в подоболочку,
# и все списки для итогового отчёта остались бы пустыми.
place_file() {
  local tmp="$1"
  local dest="$2"
  local mode="${3:-644}"

  if [ -f "$dest" ] && cmp -s "$tmp" "$dest"; then
    DONE_UNCHANGED+=("$dest")
    rm -f -- "$tmp"
    record unchanged "$dest"
    return 0
  fi
  if [ "$PLAN_ONLY" -eq 1 ]; then
    if [ -e "$dest" ]; then
      PLAN_LINES+=("  [замена]   $dest (+ бэкап)")
    else
      PLAN_LINES+=("  [создаём]  $dest")
    fi
    rm -f -- "$tmp"
    return 0
  fi

  mkdir -p -- "$(dirname -- "$dest")"
  if [ -e "$dest" ]; then
    backup_file "$dest"
    DONE_WRITTEN+=("$dest (заменён, бэкап рядом)")
  else
    DONE_WRITTEN+=("$dest")
  fi
  cat "$tmp" >"$dest"
  chmod "$mode" "$dest"
  rm -f -- "$tmp"
  record file "$dest"
}

# install_rendered <шаблон> <назначение> <режим>
install_rendered() {
  local tmp
  tmp=$(mktemp "${TMPDIR:-/tmp}/digitwm-render.XXXXXX")
  render "$1" >"$tmp"
  place_file "$tmp" "$2" "${3:-644}"
}

# install_copy <источник> <назначение> <режим>
install_copy() {
  local tmp
  tmp=$(mktemp "${TMPDIR:-/tmp}/digitwm-copy.XXXXXX")
  cat "$1" >"$tmp"
  place_file "$tmp" "$2" "${3:-644}"
}

# install_content <строка> <назначение> <режим>
install_content() {
  local tmp
  tmp=$(mktemp "${TMPDIR:-/tmp}/digitwm-content.XXXXXX")
  printf '%s\n' "$1" >"$tmp"
  place_file "$tmp" "$2" "${3:-644}"
}

# render <шаблон> — печатает шаблон с подставленными значениями.
render() {
  local template="$1"
  [ -f "$template" ] || die "шаблон не найден: $template"

  local sed_script
  sed_script=$(mktemp "${TMPDIR:-/tmp}/digitwm-sed.XXXXXX")

  subst() {
    # $1 — плейсхолдер без @@, $2 — значение
    local value="$2"
    value=${value//\\/\\\\}
    value=${value//|/\\|}
    value=${value//&/\\&}
    printf 's|@@%s@@|%s|g\n' "$1" "$value" >>"$sed_script"
  }

  subst PALETTE "$PALETTE"
  subst PALETTE_CAP "$PALETTE_CAP"
  subst PREFIX "$PREFIX"
  subst CONFIG_DIR "$CONFIG_DIR"
  subst TERMINAL "$TERMINAL"
  subst TERMINAL_EXEC "$TERMINAL_EXEC"
  subst FONT_FAMILY "$FONT_FAMILY"
  subst FONT_SIZE "$FONT_SIZE"
  subst FONT "$FONT_FAMILY:pixelsize=14"
  subst VIM_COLORSCHEME "digitable-focus-$PALETTE"
  subst VIMRC_TARGET "$CONFIG_DIR/vimrc"
  subst TMUX_CONF_TARGET "$CONFIG_DIR/tmux.conf"
  subst KEYS_DOC "$CONFIG_DIR/keys.md"
  subst TMUX_THEME "${XDG_CONFIG_HOME:-$HOME/.config}/tmux/digitable-focus-$PALETTE.conf"
  subst ALACRITTY_THEME "${XDG_CONFIG_HOME:-$HOME/.config}/alacritty/digitable-focus-$PALETTE.toml"
  subst STARSHIP_THEME "${XDG_CONFIG_HOME:-$HOME/.config}/starship/digitable-focus-$PALETTE.toml"
  subst ZSH_THEME "${XDG_CONFIG_HOME:-$HOME/.config}/zsh/digitable-focus-$PALETTE.zsh"
  subst FZF_THEME "${XDG_CONFIG_HOME:-$HOME/.config}/fzf/digitable-focus-$PALETTE.sh"
  subst EZA_THEME "${XDG_CONFIG_HOME:-$HOME/.config}/eza/digitable-focus-$PALETTE.sh"
  subst BAT_THEME "Digitable-Focus-$PALETTE_CAP"

  local key camel
  for key in $PALETTE_KEYS; do
    # surfaceRaised -> SURFACE_RAISED
    camel=$(printf '%s' "$key" | sed 's/\([A-Z]\)/_\1/g' | tr '[:lower:]' '[:upper:]')
    subst "$camel" "$(palette_get "$key")"
  done

  if [ -z "$PALETTE_FILE" ] || [ -z "$PALETTE_VALUES" ]; then
    # Палитры нет: блок цветов удаляем целиком, чтобы не остались пустые
    # значения — cwm на них ругается при разборе.
    sed '/#@palette-begin/,/#@palette-end/d' "$template" | sed -f "$sed_script"
  else
    sed '/#@palette-begin/d;/#@palette-end/d' "$template" | sed -f "$sed_script"
  fi
  rm -f -- "$sed_script"
}

# --- помеченные блоки в чужих rc-файлах ------------------------------------
# Блок ставится один раз, при повторе заменяется целиком. Всё, что было в
# файле, остаётся на месте; перед изменением делается бэкап.

ensure_block() {
  local target="$1"
  local comment="$2"
  local payload="$3"
  local begin="$comment >>> digitwm session >>>"
  local end="$comment <<< digitwm session <<<"

  local block
  block=$(printf '%s\n%s\n%s\n' "$begin" "$payload" "$end")

  if [ ! -e "$target" ]; then
    if [ "$PLAN_ONLY" -eq 1 ]; then
      PLAN_LINES+=("  [создаём]  $target (блок подключения)")
      return 0
    fi
    mkdir -p -- "$(dirname -- "$target")"
    printf '%s\n' "$block" >"$target"
    DONE_WRITTEN+=("$target (создан, блок подключения)")
    record block "$target"
    return 0
  fi

  if grep -qF "$begin" "$target" 2>/dev/null; then
    local current
    current=$(awk -v b="$begin" -v e="$end" '
      index($0, b) { inside = 1 }
      inside { print }
      index($0, e) { inside = 0 }' "$target")
    if [ "$current" = "$block" ]; then
      DONE_UNCHANGED+=("$target (блок на месте)")
      record unchanged "$target"
      return 0
    fi
    if [ "$PLAN_ONLY" -eq 1 ]; then
      PLAN_LINES+=("  [обновим]  $target (блок подключения, + бэкап)")
      return 0
    fi
    backup_file "$target"
    local tmp
    tmp=$(mktemp "${TMPDIR:-/tmp}/digitwm-block.XXXXXX")
    awk -v b="$begin" -v e="$end" '
      index($0, b) { skip = 1 }
      !skip { print }
      index($0, e) { skip = 0 }' "$target" >"$tmp"
    printf '%s\n' "$block" >>"$tmp"
    cat "$tmp" >"$target"
    rm -f -- "$tmp"
    DONE_WRITTEN+=("$target (блок обновлён, бэкап рядом)")
    record block "$target"
    return 0
  fi

  if [ "$PLAN_ONLY" -eq 1 ]; then
    PLAN_LINES+=("  [допишем]  $target (блок подключения, + бэкап)")
    return 0
  fi
  backup_file "$target"
  printf '\n%s\n' "$block" >>"$target"
  DONE_WRITTEN+=("$target (блок дописан, бэкап рядом)")
  record block "$target"
}

# --- темы Workbench ---------------------------------------------------------

install_theme() {
  local integration="$1"
  local file="$2"
  local dest_dir="$3"
  [ "$SKIP_THEMES" -eq 0 ] || return 0
  [ -n "$THEMES_DIR" ] || return 0

  local src="$THEMES_DIR/$integration/$file"
  local dest="$dest_dir/$(basename -- "$file")"
  if [ ! -f "$src" ]; then
    DONE_SKIPPED+=("тема $integration: нет файла $src")
    return 0
  fi
  if [ -f "$dest" ] && cmp -s "$src" "$dest"; then
    DONE_THEMES+=("$integration — уже актуально")
    record unchanged "$dest"
    return 0
  fi
  if [ "$PLAN_ONLY" -eq 1 ]; then
    PLAN_LINES+=("  [тема]     $dest")
    return 0
  fi
  mkdir -p -- "$dest_dir"
  [ ! -e "$dest" ] || backup_file "$dest"
  cp -- "$src" "$dest"
  DONE_THEMES+=("$integration — $dest")
  record theme "$dest" "$integration"
}

install_all_themes() {
  local cfg="${XDG_CONFIG_HOME:-$HOME/.config}"
  install_theme vim "colors/digitable-focus-$PALETTE.vim" "$HOME/.vim/colors"
  install_theme neovim "colors/digitable-focus-$PALETTE.lua" "$cfg/nvim/colors"
  install_theme tmux "digitable-focus-$PALETTE.conf" "$cfg/tmux"
  install_theme alacritty "digitable-focus-$PALETTE.toml" "$cfg/alacritty"
  install_theme kitty "digitable-focus-$PALETTE.conf" "$cfg/kitty"
  install_theme zsh "digitable-focus-$PALETTE.zsh" "$cfg/zsh"
  install_theme starship "digitable-focus-$PALETTE.toml" "$cfg/starship"
  install_theme fzf "digitable-focus-$PALETTE.sh" "$cfg/fzf"
  install_theme eza "digitable-focus-$PALETTE.sh" "$cfg/eza"
  install_theme bat "Digitable-Focus-$PALETTE_CAP.tmTheme" "$cfg/bat/themes"
  install_theme btop "digitable-focus-$PALETTE.theme" "$cfg/btop/themes"
  install_theme delta "digitable-focus-$PALETTE.gitconfig" "$cfg/git"
  install_theme lazygit "digitable-focus-$PALETTE.yml" "$cfg/lazygit"

  # bat читает темы из кэша, а не из каталога.
  if [ "$PLAN_ONLY" -eq 0 ] && [ -f "$cfg/bat/themes/Digitable-Focus-$PALETTE_CAP.tmTheme" ]; then
    local bat_bin=""
    command -v bat >/dev/null 2>&1 && bat_bin=bat
    [ -n "$bat_bin" ] || { command -v batcat >/dev/null 2>&1 && bat_bin=batcat; }
    if [ -n "$bat_bin" ]; then
      "$bat_bin" cache --build >>"$LOG_FILE" 2>&1 || warn "bat cache --build не отработал, см. $LOG_FILE"
    fi
  fi
}

# --- установка пакетов ------------------------------------------------------

run_bootstrap() {
  [ "$SKIP_INSTALL" -eq 0 ] || { DONE_SKIPPED+=("установка пакетов — --skip-install"); return 0; }
  [ "$SKIP_BOOTSTRAP" -eq 0 ] || { DONE_SKIPPED+=("bootstrap Workbench — --skip-bootstrap"); return 0; }
  if [ -z "$BOOTSTRAP" ]; then
    DONE_SKIPPED+=("bootstrap Workbench не найден — пакеты не ставились")
    NEXT_STEPS+=("Набор инструментов не ставился: bootstrap.sh Workbench не найден. Укажите его: session/install.sh --bootstrap /путь/к/products/workbench/toolchain/bootstrap.sh")
    return 0
  fi

  local args
  args="--palette $PALETTE --yes --tools $SESSION_TOOLS"
  [ -z "$THEMES_DIR" ] || args="$args --themes-dir $THEMES_DIR"

  if [ "$PLAN_ONLY" -eq 1 ]; then
    PLAN_LINES+=("  [bootstrap] bash $BOOTSTRAP $args")
    return 0
  fi
  say ""
  say "== Общий bootstrap Workbench =="
  say "  $BOOTSTRAP $args"
  # shellcheck disable=SC2086
  bash "$BOOTSTRAP" $args || warn "bootstrap завершился с ошибкой, продолжаем со своей частью"
}

# ensure_packages <проба> <метка> <apt> <dnf> <pacman>
#
# Ставит пакет, которого нет в toolchain.json Workbench, а значит и в
# --tools общего bootstrap. Проба — имя команды: если она уже есть, ничего не
# делаем. Если пакетный менеджер не из трёх известных или установка не
# удалась, это не повод валить установку — но и молчать нельзя: строка уходит
# в «Следующие шаги», потому что без пакета молча не работает целая ветка.
ensure_packages() {
  local probe="$1"
  local label="$2"
  local pkgs=""
  case "$PKG_MANAGER" in
    apt) pkgs="$3" ;;
    dnf) pkgs="$4" ;;
    pacman) pkgs="$5" ;;
  esac

  if [ -n "$probe" ] && command -v "$probe" >/dev/null 2>&1; then
    return 0
  fi
  if [ -z "$pkgs" ]; then
    NEXT_STEPS+=("$label: пакетный менеджер $PKG_MANAGER здесь не поддержан, поставьте $probe сами")
    return 0
  fi
  if [ "$PLAN_ONLY" -eq 1 ]; then
    PLAN_LINES+=("  [пакеты]   $PKG_MANAGER: $pkgs — $label")
    return 0
  fi

  local runner=""
  [ "$(id -u)" = "0" ] || runner="$SUDO_BIN"
  case "$PKG_MANAGER" in
    apt) $runner env DEBIAN_FRONTEND=noninteractive apt-get install -y $pkgs >>"$LOG_FILE" 2>&1 || warn "не поставились $pkgs" ;;
    dnf) $runner dnf install -y $pkgs >>"$LOG_FILE" 2>&1 || warn "не поставились $pkgs" ;;
    pacman) $runner pacman -S --needed --noconfirm $pkgs >>"$LOG_FILE" 2>&1 || warn "не поставились $pkgs" ;;
  esac
  if [ -n "$probe" ] && ! command -v "$probe" >/dev/null 2>&1; then
    NEXT_STEPS+=("$label: $pkgs не поставились, см. $LOG_FILE")
  fi
}

install_x_extras() {
  [ "$SKIP_INSTALL" -eq 0 ] || return 0
  [ "$OS_NAME" != "macos" ] || return 0

  # Мелочи, без которых сессия стартует, но выглядит сырой: цвет фона,
  # база ресурсов, раскладка мониторов. Ставим по возможности, молча
  # переживаем неудачу — это не повод валить установку.
  if ! command -v xsetroot >/dev/null 2>&1 || ! command -v xrdb >/dev/null 2>&1; then
    local pkgs=""
    case "$PKG_MANAGER" in
      apt) pkgs="x11-xserver-utils xinit" ;;
      dnf) pkgs="xorg-x11-server-utils xorg-x11-xinit" ;;
      pacman) pkgs="xorg-xsetroot xorg-xrdb xorg-xset xorg-xinit" ;;
    esac
    if [ -n "$pkgs" ]; then
      if [ "$PLAN_ONLY" -eq 1 ]; then
        PLAN_LINES+=("  [пакеты]   $PKG_MANAGER: $pkgs (xsetroot, xrdb)")
      else
        local runner=""
        [ "$(id -u)" = "0" ] || runner="$SUDO_BIN"
        case "$PKG_MANAGER" in
          apt) $runner env DEBIAN_FRONTEND=noninteractive apt-get install -y $pkgs >>"$LOG_FILE" 2>&1 || warn "не поставились $pkgs" ;;
          dnf) $runner dnf install -y $pkgs >>"$LOG_FILE" 2>&1 || warn "не поставились $pkgs" ;;
          pacman) $runner pacman -S --needed --noconfirm $pkgs >>"$LOG_FILE" 2>&1 || warn "не поставились $pkgs" ;;
        esac
      fi
    fi
  fi

  # xdotool. Без него digitwm-digit не умеет поднять уже открытое окно, и
  # Mod4+grave открывает второе окно Digit вместо того, чтобы вернуть первое.
  # В toolchain.json Workbench его нет — ставим здесь.
  ensure_packages xdotool "Mod4+grave поднимает открытое окно Digit, а не открывает второе" \
    xdotool xdotool xdotool

  # polybar. Установщик кладёт для него готовую конфигурацию, cwmrc отдаёт ему
  # Mod4+Shift+b, doc/panel.md выбирает его числами — а поставить его было
  # некому: в toolchain.json Workbench его тоже нет (0 вхождений). Панель при
  # этом сама не запускается: строка для неё есть в autostart, но
  # закомментированная, а Mod4+Shift+b поднимает её и без автозапуска.
  ensure_packages polybar "панель, которой сессия отдаёт Mod4+Shift+b" \
    polybar polybar polybar
}

install_digit() {
  [ "$WITH_DIGIT" -eq 1 ] || {
    command -v digit >/dev/null 2>&1 || \
      NEXT_STEPS+=("Digit не установлен. Официальный установщик: curl -fsSL https://raw.githubusercontent.com/digitable-lol/digit/main/scripts/install.sh | bash && digit setup")
    return 0
  }
  if command -v digit >/dev/null 2>&1; then
    DONE_SKIPPED+=("Digit уже установлен")
    return 0
  fi
  if [ "$PLAN_ONLY" -eq 1 ]; then
    PLAN_LINES+=("  [digit]    официальный установщик digitable-lol/digit, затем digit setup")
    return 0
  fi
  command -v curl >/dev/null 2>&1 || {
    warn "нет curl — Digit не поставлен"
    NEXT_STEPS+=("Digit не поставлен: нет curl. Поставьте curl и повторите с --with-digit.")
    return 0
  }
  say ""
  say "== Digit =="
  say "  запускаем официальный установщик digitable-lol/digit"
  if ! curl -fsSL https://raw.githubusercontent.com/digitable-lol/digit/main/scripts/install.sh | bash; then
    warn "установщик Digit завершился с ошибкой, см. вывод выше"
    NEXT_STEPS+=("Установщик Digit завершился с ошибкой. После починки: digit setup")
    return 0
  fi

  # Второй шаг официальной установки. Раньше он только печатался в «Следующие
  # шаги», а --with-digit его не выполнял: обещание и действие расходились.
  # Теперь шаг выполняется — но он диалоговый, и без терминала запускать
  # диалог нечестно: тогда он остаётся в шагах, а не притворяется сделанным.
  hash -r 2>/dev/null || true
  if ! command -v digit >/dev/null 2>&1; then
    NEXT_STEPS+=("Digit поставлен, но команда digit не появилась в PATH этого сеанса. Откройте новый терминал и выполните: digit setup")
    return 0
  fi
  if [ -t 0 ] && [ -t 1 ]; then
    say "  digit setup"
    if ! digit setup; then
      warn "digit setup завершился с ошибкой"
      NEXT_STEPS+=("digit setup завершился с ошибкой — выполните его сами.")
    fi
  else
    NEXT_STEPS+=("Digit поставлен. Остался диалоговый шаг: digit setup — он задаёт вопросы, поэтому в неинтерактивном запуске не выполняется.")
  fi
}

build_wm() {
  [ "$SKIP_BUILD" -eq 0 ] || { DONE_SKIPPED+=("сборка оконного менеджера — --skip-build"); return 0; }
  if [ "$PLAN_ONLY" -eq 1 ]; then
    PLAN_LINES+=("  [сборка]   make && make install PREFIX=$PREFIX (в $REPO_ROOT)")
    return 0
  fi
  if ! command -v make >/dev/null 2>&1 || ! command -v cc >/dev/null 2>&1; then
    DONE_SKIPPED+=("сборка оконного менеджера — нет make или cc")
    NEXT_STEPS+=("Оконный менеджер не собран: нужны make, cc и заголовки x11, xft, xrandr.")
    return 0
  fi
  say ""
  say "== Оконный менеджер =="
  if ( cd "$REPO_ROOT" && make >>"$LOG_FILE" 2>&1 && make install PREFIX="$PREFIX" >>"$LOG_FILE" 2>&1 ); then
    say "  собран и установлен в $PREFIX/bin"
    record binary "$PREFIX/bin/cwm"
  else
    warn "сборка не удалась, см. $LOG_FILE"
    NEXT_STEPS+=("Оконный менеджер не собрался. Журнал: $LOG_FILE")
  fi
}

# --- собственно установка конфигурации -------------------------------------

install_configs() {
  install_rendered "$SCRIPT_DIR/config/cwmrc.in"          "$CONFIG_DIR/cwmrc" 644
  install_rendered "$SCRIPT_DIR/config/vimrc"             "$CONFIG_DIR/vimrc" 644
  install_rendered "$SCRIPT_DIR/config/tmux.conf.in"      "$CONFIG_DIR/tmux.conf" 644
  install_rendered "$SCRIPT_DIR/config/zshrc.in"          "$CONFIG_DIR/zshrc" 644
  install_rendered "$SCRIPT_DIR/config/alacritty.toml.in" "$CONFIG_DIR/alacritty.toml" 644
  install_copy     "$SCRIPT_DIR/docs/KEYS.md"             "$CONFIG_DIR/keys.md" 644

  if [ -n "$PALETTE_VALUES" ]; then
    install_rendered "$SCRIPT_DIR/config/Xresources.in"   "$CONFIG_DIR/Xresources" 644
    # Панель — единственный файл, который без палитры теряет смысл целиком:
    # у polybar цвета обязательны, и пустые значения он принимает молча,
    # выкрашивая бар в чужие умолчания. Лучше не класть, чем положить чужой.
    install_rendered "$SCRIPT_DIR/config/polybar.ini.in"  "$CONFIG_DIR/polybar.ini" 644
  else
    DONE_SKIPPED+=("Xresources — палитра не найдена")
    DONE_SKIPPED+=("polybar.ini — палитра не найдена")
  fi

  install_rendered "$SCRIPT_DIR/bin/digitwm-session.in" "$PREFIX/bin/digitwm-session" 755
  install_rendered "$SCRIPT_DIR/bin/digitwm-digit.in"   "$PREFIX/bin/digitwm-digit" 755
  install_copy     "$SCRIPT_DIR/bin/digitwm-lock"       "$PREFIX/bin/digitwm-lock" 755
  install_copy     "$SCRIPT_DIR/bin/digitwm-panel"      "$PREFIX/bin/digitwm-panel" 755
  install_copy     "$SCRIPT_DIR/bin/digitwm-dev-session" "$PREFIX/bin/digitwm-dev-session" 755
  install_copy     "$SCRIPT_DIR/bin/rgfzf.sh"           "$PREFIX/bin/rgfzf.sh" 755

  # Автозапуск и переменные окружения — файлы пользователя. Создаём их один
  # раз и больше не трогаем никогда: это единственные два места, где человек
  # пишет своё, и переписывать их при обновлении было бы предательством.
  #
  # Их именно два, и граница между ними жёсткая. env читает САМА сессия, до
  # автозапуска и до окна Digit, поэтому переменные из него видят оба.
  # autostart — отдельный фоновый потомок: экспорт из него не попадает ни в
  # Digit, ни в оконный менеджер, и переменным там не место.
  if [ -e "$CONFIG_DIR/env" ]; then
    DONE_UNCHANGED+=("$CONFIG_DIR/env (ваш файл, не трогаем)")
  else
    install_copy "$SCRIPT_DIR/config/env.example" "$CONFIG_DIR/env" 644
  fi

  if [ -e "$CONFIG_DIR/autostart" ]; then
    DONE_UNCHANGED+=("$CONFIG_DIR/autostart (ваш файл, не трогаем)")
  else
    install_copy "$SCRIPT_DIR/config/autostart.example" "$CONFIG_DIR/autostart" 755
  fi
}

install_desktop_entry() {
  local dest
  if [ "$SYSTEM_DESKTOP" -eq 1 ]; then
    dest="/usr/share/xsessions/digitwm.desktop"
    if [ "$(id -u)" != "0" ] && [ -z "$SUDO_BIN" ]; then
      NEXT_STEPS+=("Для --system нужен root: скопируйте session/digitwm.desktop в /usr/share/xsessions/ сами")
      return 0
    fi
    if [ "$PLAN_ONLY" -eq 1 ]; then
      PLAN_LINES+=("  [создаём]  $dest (нужен root)")
      return 0
    fi
    local tmp
    tmp=$(mktemp "${TMPDIR:-/tmp}/digitwm-desktop.XXXXXX")
    render "$SCRIPT_DIR/digitwm.desktop.in" >"$tmp"
    if [ "$(id -u)" = "0" ]; then
      install -d /usr/share/xsessions && install -m 644 "$tmp" "$dest"
    else
      $SUDO_BIN install -d /usr/share/xsessions && $SUDO_BIN install -m 644 "$tmp" "$dest"
    fi
    rm -f -- "$tmp"
    DONE_WRITTEN+=("$dest")
    record file "$dest"
  else
    dest="${XDG_DATA_HOME:-$HOME/.local/share}/xsessions/digitwm.desktop"
    install_rendered "$SCRIPT_DIR/digitwm.desktop.in" "$dest" 644
    NEXT_STEPS+=("Запись входа лежит в $dest. GDM, SDDM и LightDM читают этот каталог, но не все сборки — если digitwm не появился в списке сессий, повторите с --system.")
  fi
}

install_rc_blocks() {
  if [ "$TOUCH_RC" -eq 0 ]; then
    NEXT_STEPS+=("Подключение не дописано (--no-rc). Добавьте сами: source $CONFIG_DIR/zshrc в ~/.zshrc; source $CONFIG_DIR/vimrc в ~/.vimrc; source-file $CONFIG_DIR/tmux.conf в ~/.tmux.conf")
    return 0
  fi

  ensure_block "$HOME/.zshrc" "#" "source \"$CONFIG_DIR/zshrc\""
  ensure_block "$HOME/.vimrc" '"' "source $CONFIG_DIR/vimrc"
  ensure_block "$HOME/.tmux.conf" "#" "source-file \"$CONFIG_DIR/tmux.conf\""

  # Alacritty читает TOML: дописать строку в чужой файл нельзя — второй ключ
  # import сделает файл невалидным. Поэтому пишем только если своего нет.
  local alacritty_main="${XDG_CONFIG_HOME:-$HOME/.config}/alacritty/alacritty.toml"
  if [ -e "$alacritty_main" ]; then
    if grep -qF "$CONFIG_DIR/alacritty.toml" "$alacritty_main" 2>/dev/null; then
      DONE_UNCHANGED+=("$alacritty_main (подключение уже есть)")
    else
      NEXT_STEPS+=("У вас свой $alacritty_main. Добавьте в его список import путь \"$CONFIG_DIR/alacritty.toml\" — двух ключей import в TOML быть не может, поэтому автоматически это не делается.")
    fi
  else
    install_content "import = [\"$CONFIG_DIR/alacritty.toml\"]" "$alacritty_main" 644
  fi

  # Neovim: init.lua — тоже не текстовый rc, куда можно дописать строку.
  local nvim_init="${XDG_CONFIG_HOME:-$HOME/.config}/nvim/init.lua"
  if [ -e "$nvim_init" ]; then
    install_rendered "$SCRIPT_DIR/config/nvim-init.lua" \
      "${XDG_CONFIG_HOME:-$HOME/.config}/nvim/digitwm-session.lua" 644
    NEXT_STEPS+=("У вас свой $nvim_init. Подключите сессию строкой: dofile(vim.fn.expand('~/.config/nvim/digitwm-session.lua'))")
  else
    install_rendered "$SCRIPT_DIR/config/nvim-init.lua" "$nvim_init" 644
  fi

  # ~/.xinitrc — для запуска через startx, если менеджера входа нет.
  if [ -e "$HOME/.xinitrc" ]; then
    NEXT_STEPS+=("У вас свой ~/.xinitrc. Для запуска через startx последней строкой должно быть: exec $PREFIX/bin/digitwm-session")
  else
    install_content "$(printf '#!/bin/sh\n# Digitable Session\nexec "%s/bin/digitwm-session"' "$PREFIX")" \
      "$HOME/.xinitrc" 755
  fi
}

print_header() {
  say "Digitable Session — установка окружения вокруг digitwm"
  say "  репозиторий : $REPO_ROOT"
  say "  платформа   : $OS_NAME, пакетный менеджер: $PKG_MANAGER"
  say "  палитра     : $PALETTE"
  say "  терминал    : $TERMINAL"
  say "  шрифт       : $FONT_FAMILY $FONT_SIZE"
  say "  конфигурация: $CONFIG_DIR"
  say "  скрипты     : $PREFIX/bin"
  if [ -n "$PALETTE_FILE" ]; then
    say "  палитра из  : $PALETTE_FILE"
    [ -z "$PALETTE_SOURCE" ] || say "                ($PALETTE_SOURCE)"
  elif [ -n "$PALETTE_SOURCE" ]; then
    say "  палитра из  : $PALETTE_SOURCE"
  else
    say "  палитра из  : НЕ НАЙДЕНА — цвета останутся стандартными"
  fi
  if [ -n "$THEMES_DIR" ]; then
    say "  темы из     : $THEMES_DIR"
    [ -z "$THEMES_SOURCE" ] || say "                ($THEMES_SOURCE)"
  elif [ -n "$THEMES_SOURCE" ]; then
    say "  темы из     : $THEMES_SOURCE"
  else
    say "  темы из     : НЕ НАЙДЕНЫ — ни клона Workbench, ни --themes-dir, ни --fetch-configs"
  fi
  if [ -n "$BOOTSTRAP" ]; then
    say "  bootstrap   : $BOOTSTRAP"
  else
    say "  bootstrap   : не найден — пакеты не ставятся"
  fi
  if [ "$PLAN_ONLY" -eq 1 ]; then
    say "  режим       : план, ничего не изменяется"
  else
    say "  режим       : установка, журнал: $LOG_FILE"
  fi
}

print_group() {
  local title="$1"
  shift
  [ $# -gt 0 ] || return 0
  say ""
  say "$title ($#):"
  local item
  for item in "$@"; do
    say "  - $item"
  done
}

confirm() {
  [ "$ASSUME_YES" -eq 0 ] || return 0
  if [ ! -t 0 ]; then
    say ""
    say "Неинтерактивный запуск: продолжаем без подтверждения."
    return 0
  fi
  local answer=""
  say ""
  printf 'Выполнить установку? [y/N] '
  read -r answer || answer=""
  case "$answer" in
    [yYдД] | yes | да) return 0 ;;
    *) die "отменено пользователем" ;;
  esac
}

main() {
  parse_args "$@"
  detect_platform
  resolve_workbench
  load_palette
  resolve_terminal

  print_header

  if [ "$PLAN_ONLY" -eq 1 ]; then
    run_bootstrap
    install_x_extras
    build_wm
    install_digit
    install_configs
    install_all_themes
    install_desktop_entry
    install_rc_blocks
    say ""
    say "== План =="
    if [ "${#PLAN_LINES[@]}" -gt 0 ]; then
      printf '%s\n' "${PLAN_LINES[@]}"
    else
      say "  всё уже на месте"
    fi
    print_group "Уже актуально" ${DONE_UNCHANGED[@]+"${DONE_UNCHANGED[@]}"}
    print_group "Пропущено" ${DONE_SKIPPED[@]+"${DONE_SKIPPED[@]}"}
    say ""
    say "План показан. Ничего не изменено."
    return 0
  fi

  confirm

  : >"$LOG_FILE"
  mkdir -p -- "$STATE_DIR"
  : >"$MANIFEST"
  printf '# digitwm session manifest\t%s\tпалитра=%s\n' "$STAMP" "$PALETTE" >>"$MANIFEST"

  run_bootstrap
  install_x_extras
  build_wm
  install_digit

  say ""
  say "== Конфигурация =="
  install_configs
  install_all_themes
  install_desktop_entry
  install_rc_blocks

  say ""
  say "== Итог =="
  print_group "Записано" ${DONE_WRITTEN[@]+"${DONE_WRITTEN[@]}"}
  print_group "Темы" ${DONE_THEMES[@]+"${DONE_THEMES[@]}"}
  print_group "Уже актуально" ${DONE_UNCHANGED[@]+"${DONE_UNCHANGED[@]}"}
  print_group "Резервные копии" ${DONE_BACKUPS[@]+"${DONE_BACKUPS[@]}"}
  print_group "Пропущено" ${DONE_SKIPPED[@]+"${DONE_SKIPPED[@]}"}
  print_group "Следующие шаги (руками)" ${NEXT_STEPS[@]+"${NEXT_STEPS[@]}"}

  say ""
  say "Проверить установку: session/verify.sh"
  say "Карта клавиш:        $CONFIG_DIR/keys.md"
  say "Журнал установки:    $LOG_FILE"
}

main "$@"
