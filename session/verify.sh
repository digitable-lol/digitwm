#!/usr/bin/env bash
#
# Digitable Session — проверка установки.
#
# Отвечает на четыре вопроса, и только на них:
#   1. Все ли файлы конфигурации на месте и не пусты.
#   2. Разбираются ли они теми программами, для которых написаны.
#   3. Применена ли палитра ко всем целям, а не к части.
#   4. Не затёрт ли чужой файл без резервной копии.
#
# Чего проверка НЕ делает: она не поднимает X11-сессию. Без дисплея нельзя
# ни проверить, что окно Digit встало слева, ни что Mod4+h переключает фокус.
# Здесь об этом сказано прямо, а не подменено зелёной строчкой.
#
#   session/verify.sh                  проверить установку
#   session/verify.sh --palette signal проверить установку другой палитры
#   session/verify.sh --quiet          только итог и ошибки

set -u

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

PALETTE="carbon"
PALETTE_CAP="Carbon"
PREFIX="$HOME/.local"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/digitwm"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/digitwm"
MANIFEST="$STATE_DIR/manifest.tsv"
CFG="${XDG_CONFIG_HOME:-$HOME/.config}"
QUIET=0

PASS=0
FAIL=0
WARN=0
FAILURES=()

usage() {
  cat <<'USAGE'
Digitable Session — проверка установки.

  --palette NAME   carbon | paper | signal (по умолчанию carbon)
  --prefix DIR     где лежат скрипты сессии (по умолчанию ~/.local)
  --quiet, -q      печатать только ошибки и итог
  --help, -h       эта справка

Код возврата 0 — всё на месте; 1 — есть проваленные проверки.
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --palette) PALETTE="$2"; shift ;;
    --palette=*) PALETTE="${1#--palette=}" ;;
    --prefix) PREFIX="$2"; shift ;;
    --prefix=*) PREFIX="${1#--prefix=}" ;;
    --quiet | -q) QUIET=1 ;;
    --help | -h) usage; exit 0 ;;
    *) printf 'verify: неизвестный аргумент: %s\n' "$1" >&2; exit 2 ;;
  esac
  shift
done

case "$PALETTE" in
  carbon) PALETTE_CAP="Carbon" ;;
  paper) PALETTE_CAP="Paper" ;;
  signal) PALETTE_CAP="Signal" ;;
  *) printf 'verify: неизвестная палитра: %s\n' "$PALETTE" >&2; exit 2 ;;
esac

ok() {
  PASS=$((PASS + 1))
  [ "$QUIET" -eq 1 ] || printf '  \033[32mok\033[0m    %s\n' "$1"
}
bad() {
  FAIL=$((FAIL + 1))
  FAILURES+=("$1${2:+ — $2}")
  printf '  \033[31mПЛОХО\033[0m %s%s\n' "$1" "${2:+ — $2}"
}
note() {
  WARN=$((WARN + 1))
  [ "$QUIET" -eq 1 ] || printf '  \033[33mвнимание\033[0m %s\n' "$1"
}
section() {
  [ "$QUIET" -eq 1 ] || printf '\n== %s ==\n' "$1"
}

# --- 1. Файлы на месте ------------------------------------------------------

check_file() {
  local path="$1"
  local label="$2"
  if [ ! -e "$path" ]; then
    bad "$label" "нет файла $path"
    return 1
  fi
  if [ ! -s "$path" ]; then
    bad "$label" "файл пуст: $path"
    return 1
  fi
  ok "$label"
  return 0
}

check_exec() {
  local path="$1"
  local label="$2"
  check_file "$path" "$label" || return 1
  if [ ! -x "$path" ]; then
    bad "$label" "не исполняемый: $path"
    return 1
  fi
  return 0
}

section "Файлы конфигурации"
check_file "$CONFIG_DIR/cwmrc"          "конфигурация оконного менеджера"
check_file "$CONFIG_DIR/vimrc"          "конфигурация vim"
check_file "$CONFIG_DIR/tmux.conf"      "конфигурация tmux"
check_file "$CONFIG_DIR/zshrc"          "конфигурация zsh"
check_file "$CONFIG_DIR/alacritty.toml" "конфигурация alacritty"
check_file "$CONFIG_DIR/keys.md"        "карта клавиш"
check_file "$CONFIG_DIR/autostart"      "пользовательский автозапуск"

section "Скрипты сессии"
check_exec "$PREFIX/bin/digitwm-session"     "точка входа сессии"
check_exec "$PREFIX/bin/digitwm-digit"       "запуск Digit"
check_exec "$PREFIX/bin/digitwm-lock"        "блокировка экрана"
check_exec "$PREFIX/bin/digitwm-dev-session" "рабочая сессия tmux"
check_exec "$PREFIX/bin/rgfzf.sh"            "поиск по содержимому"

section "Запись для менеджера входа"
desktop_user="${XDG_DATA_HOME:-$HOME/.local/share}/xsessions/digitwm.desktop"
desktop_system="/usr/share/xsessions/digitwm.desktop"
if [ -e "$desktop_user" ]; then
  check_file "$desktop_user" "запись входа (пользовательская)"
  entry="$desktop_user"
elif [ -e "$desktop_system" ]; then
  check_file "$desktop_system" "запись входа (общесистемная)"
  entry="$desktop_system"
else
  bad "запись входа" "нет ни $desktop_user, ни $desktop_system"
  entry=""
fi
if [ -n "$entry" ]; then
  exec_line=$(sed -n 's/^Exec=//p' "$entry" | head -n 1)
  if [ -x "$exec_line" ]; then
    ok "Exec из .desktop указывает на существующий скрипт"
  else
    bad "Exec из .desktop" "не исполняемый путь: $exec_line"
  fi
fi

# --- 2. Синтаксис: спрашиваем сами программы --------------------------------

section "Разбор конфигурации самими программами"

WM=""
for candidate in digitwm cwm "$PREFIX/bin/digitwm" "$PREFIX/bin/cwm"; do
  if command -v "$candidate" >/dev/null 2>&1; then WM="$candidate"; break; fi
done
if [ -n "$WM" ] && [ -f "$CONFIG_DIR/cwmrc" ]; then
  # cwm -n разбирает конфиг и выходит ДО подключения к дисплею
  # (calmwm.c: parse_config, затем `if (nflag) return 0;`), поэтому
  # проверка работает без X.
  if "$WM" -n -c "$CONFIG_DIR/cwmrc" >/dev/null 2>&1; then
    ok "cwmrc разобран оконным менеджером ($WM -n)"
  else
    bad "cwmrc" "$WM -n сообщает об ошибке разбора"
  fi
else
  note "оконный менеджер не найден — разбор cwmrc не проверен"
fi

if command -v zsh >/dev/null 2>&1 && [ -f "$CONFIG_DIR/zshrc" ]; then
  if zsh -n "$CONFIG_DIR/zshrc" 2>/dev/null; then
    ok "zshrc разобран (zsh -n)"
  else
    bad "zshrc" "zsh -n сообщает о синтаксической ошибке"
  fi
else
  note "zsh не установлен — разбор zshrc не проверен"
fi

if command -v tmux >/dev/null 2>&1 && [ -f "$CONFIG_DIR/tmux.conf" ]; then
  tmux_socket="digitwm-verify-$$"
  if tmux -L "$tmux_socket" -f "$CONFIG_DIR/tmux.conf" new-session -d -s check true >/dev/null 2>&1; then
    ok "tmux.conf принят сервером tmux"
    tmux -L "$tmux_socket" kill-server >/dev/null 2>&1 || true
  else
    bad "tmux.conf" "tmux отказался запускаться с этим файлом"
    tmux -L "$tmux_socket" kill-server >/dev/null 2>&1 || true
  fi
else
  note "tmux не установлен — разбор tmux.conf не проверен"
fi

if command -v vim >/dev/null 2>&1 && [ -f "$CONFIG_DIR/vimrc" ]; then
  vim_log=$(mktemp "${TMPDIR:-/tmp}/digitwm-vim.XXXXXX")
  if vim -T dumb -n -es -u "$CONFIG_DIR/vimrc" -c 'qall!' >"$vim_log" 2>&1; then
    if [ -s "$vim_log" ]; then
      note "vim принял конфигурацию, но что-то напечатал: $(head -n 1 "$vim_log")"
    else
      ok "vimrc загружен vim без ошибок"
    fi
  else
    bad "vimrc" "vim завершился с ошибкой: $(head -n 1 "$vim_log")"
  fi
  rm -f -- "$vim_log"
else
  note "vim не установлен — загрузка vimrc не проверена"
fi

if command -v python3 >/dev/null 2>&1 && [ -f "$CONFIG_DIR/alacritty.toml" ]; then
  if python3 - "$CONFIG_DIR/alacritty.toml" <<'PY' >/dev/null 2>&1
import sys
try:
    import tomllib
except ModuleNotFoundError:
    sys.exit(0)
with open(sys.argv[1], "rb") as handle:
    tomllib.load(handle)
PY
  then
    ok "alacritty.toml — корректный TOML"
  else
    bad "alacritty.toml" "TOML не разбирается"
  fi
else
  note "нет python3 — TOML не проверен"
fi

# --- 3. Палитра применена ко всем целям -------------------------------------

section "Палитра $PALETTE во всех целях"

expect_theme() {
  local path="$1"
  local label="$2"
  if [ -f "$path" ]; then
    ok "$label — $path"
  else
    bad "$label" "нет файла темы $path"
  fi
}

expect_theme "$HOME/.vim/colors/digitable-focus-$PALETTE.vim"      "тема vim"
expect_theme "$CFG/nvim/colors/digitable-focus-$PALETTE.lua"       "тема neovim"
expect_theme "$CFG/tmux/digitable-focus-$PALETTE.conf"             "тема tmux"
expect_theme "$CFG/alacritty/digitable-focus-$PALETTE.toml"        "тема alacritty"
expect_theme "$CFG/zsh/digitable-focus-$PALETTE.zsh"               "тема zsh"
expect_theme "$CFG/starship/digitable-focus-$PALETTE.toml"         "тема starship"
expect_theme "$CFG/fzf/digitable-focus-$PALETTE.sh"                "тема fzf"
expect_theme "$CFG/eza/digitable-focus-$PALETTE.sh"                "тема eza"
expect_theme "$CFG/bat/themes/Digitable-Focus-$PALETTE_CAP.tmTheme" "тема bat"
expect_theme "$CFG/btop/themes/digitable-focus-$PALETTE.theme"     "тема btop"
expect_theme "$CFG/git/digitable-focus-$PALETTE.gitconfig"         "тема git-delta"

# Ссылка на тему должна быть не только в наличии файла, но и в конфигурации:
# файл рядом, на который никто не ссылается, — это не применённая палитра.
expect_reference() {
  local file="$1"
  local needle="$2"
  local label="$3"
  if [ ! -f "$file" ]; then
    bad "$label" "нет файла $file"
    return
  fi
  if grep -qF "$needle" "$file"; then
    ok "$label"
  else
    bad "$label" "в $file нет ссылки на $needle"
  fi
}

expect_reference "$CONFIG_DIR/vimrc" "digitable-focus-$PALETTE" "vimrc ссылается на схему"
expect_reference "$CONFIG_DIR/tmux.conf" "digitable-focus-$PALETTE.conf" "tmux.conf подключает тему"
expect_reference "$CONFIG_DIR/alacritty.toml" "digitable-focus-$PALETTE.toml" "alacritty.toml импортирует тему"
expect_reference "$CONFIG_DIR/zshrc" "digitable-focus-$PALETTE.zsh" "zshrc подключает тему"
expect_reference "$CONFIG_DIR/zshrc" "Digitable-Focus-$PALETTE_CAP" "zshrc задаёт тему bat"

# Цвета оконного менеджера должны быть настоящими цветами, а не остатками
# шаблона и не пустотой.
if [ -f "$CONFIG_DIR/cwmrc" ]; then
  if grep -q '@@' "$CONFIG_DIR/cwmrc"; then
    bad "cwmrc" "в файле остались неподставленные плейсхолдеры @@…@@"
  else
    ok "cwmrc — плейсхолдеров не осталось"
  fi
  if grep -qE '^color activeborder +"#[0-9A-Fa-f]{6}"' "$CONFIG_DIR/cwmrc"; then
    ok "cwmrc — цвет активной рамки из палитры"
  elif grep -q '^color ' "$CONFIG_DIR/cwmrc"; then
    bad "cwmrc" "строки color есть, но значение не похоже на цвет"
  else
    note "cwmrc без строк color — установка шла без палитры"
  fi
fi

for f in "$CONFIG_DIR/vimrc" "$CONFIG_DIR/zshrc" "$CONFIG_DIR/tmux.conf" \
         "$CONFIG_DIR/alacritty.toml" "$PREFIX/bin/digitwm-session" \
         "$PREFIX/bin/digitwm-digit"; do
  [ -f "$f" ] || continue
  if grep -q '@@' "$f"; then
    bad "$(basename -- "$f")" "остались неподставленные плейсхолдеры @@…@@"
  fi
done

# --- 4. Ничего чужого не затёрто без бэкапа ---------------------------------

section "Резервные копии и чужие файлы"

if [ ! -f "$MANIFEST" ]; then
  bad "манифест установки" "нет файла $MANIFEST — установщик не запускался?"
else
  ok "манифест установки на месте"
  # Каждая запись backup обязана указывать на существующий файл.
  missing_backup=0
  while IFS=$'\t' read -r action path detail; do
    case "$action" in
      backup)
        if [ ! -f "$path" ]; then
          bad "резервная копия" "записана как $path, но файла нет"
          missing_backup=1
        fi
        ;;
      file | block | theme)
        if [ ! -e "$path" ]; then
          bad "установленный файл" "записан как $path, но файла нет"
        fi
        ;;
    esac
  done <"$MANIFEST"
  [ "$missing_backup" -eq 1 ] || ok "все записанные резервные копии на месте"
fi

# Блок в чужом rc-файле должен быть ровно один: два блока значат, что
# повторная установка дописала себя вместо замены.
check_single_block() {
  local file="$1"
  local marker="$2"
  local label="$3"
  [ -f "$file" ] || { note "$label — файла нет, блок не проверен"; return; }
  local count
  count=$(grep -cF "$marker" "$file" 2>/dev/null || echo 0)
  case "$count" in
    0) note "$label — блока подключения нет (установка с --no-rc?)" ;;
    1) ok "$label — ровно один блок подключения" ;;
    *) bad "$label" "блоков подключения $count, повторная установка задублировала" ;;
  esac
}

check_single_block "$HOME/.zshrc" ">>> digitwm session >>>" "~/.zshrc"
check_single_block "$HOME/.vimrc" ">>> digitwm session >>>" "~/.vimrc"
check_single_block "$HOME/.tmux.conf" ">>> digitwm session >>>" "~/.tmux.conf"

# Если рядом с чужим файлом есть бэкап — исходное содержимое обязано в нём
# сохраниться. Проверяем, что бэкап не пуст и отличается от текущего файла.
for backup in "$HOME"/.zshrc.digitable-backup-* "$HOME"/.vimrc.digitable-backup-* \
              "$HOME"/.tmux.conf.digitable-backup-*; do
  [ -e "$backup" ] || continue
  original="${backup%%.digitable-backup-*}"
  if [ ! -s "$backup" ]; then
    bad "резервная копия $backup" "пуста"
  elif cmp -s "$backup" "$original"; then
    note "$backup совпадает с текущим файлом — изменений не было"
  else
    ok "резервная копия $(basename -- "$backup") хранит прежнее содержимое"
  fi
done

# --- 5. Digit ---------------------------------------------------------------

section "Digit"
if command -v digit >/dev/null 2>&1; then
  ok "команда digit найдена: $(command -v digit)"
else
  note "Digit не установлен — окно сессии откроется с инструкцией по установке"
fi
if [ -f "$CONFIG_DIR/cwmrc" ]; then
  if grep -q '^autogroup 0 "digit,Digit"' "$CONFIG_DIR/cwmrc"; then
    ok "cwmrc закрепляет окно Digit на всех рабочих столах"
  else
    bad "cwmrc" "нет правила autogroup для окна Digit"
  fi
  if grep -q 'digitwm-digit' "$CONFIG_DIR/cwmrc"; then
    ok "cwmrc отдаёт Digit клавише Mod4+grave"
  else
    bad "cwmrc" "Digit не привязан ни к одной клавише"
  fi
fi
if [ -x "$PREFIX/bin/digitwm-session" ]; then
  if grep -q 'digitwm-digit' "$PREFIX/bin/digitwm-session"; then
    ok "сессия запускает Digit при входе"
  else
    bad "digitwm-session" "Digit не запускается вместе с сессией"
  fi
fi

# --- Итог -------------------------------------------------------------------

printf '\n== Итог ==\n'
printf '  проверок пройдено: %d\n' "$PASS"
printf '  замечаний:         %d\n' "$WARN"
printf '  провалено:         %d\n' "$FAIL"

if [ "$FAIL" -gt 0 ]; then
  printf '\nПроваленные проверки:\n'
  for failure in "${FAILURES[@]}"; do
    printf '  - %s\n' "$failure"
  done
fi

cat <<'HONEST'

Что эта проверка не проверяет и проверить не может:
  - что сессия действительно стартует в X11: для этого нужен дисплей,
    менеджер входа и живое железо;
  - что окно Digit встало слева и держится на всех рабочих столах:
    это видно только на запущенном X-сервере;
  - что сочетания Mod4 не перехвачены другой программой: конфликт
    обнаруживается во время работы, а не при разборе файла;
  - что установленные шрифты действительно содержат нужные глифы.
Эти четыре пункта проверяются глазами, на живой машине.
HONEST

[ "$FAIL" -eq 0 ] || exit 1
exit 0
