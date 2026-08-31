#!/bin/sh
# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
# Digitable Session — поиск по содержимому: ripgrep + fzf + bat.
#
# Это тот самый rgfzf.sh, который в конфигурации vim из dotfiles висит на
# \g, \P и \sf. Идея и роль взяты оттуда (scripts/customs/rgfzf.sh),
# реализация написана заново: добавлены проверка инструментов, открытие
# найденного в $EDITOR на нужной строке и поддержка batcat из Debian.
#
# Печатает путь к выбранному файлу — так его использует vim; при запуске из
# оболочки открывает файл в редакторе на найденной строке.

set -u

for tool in rg fzf; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'rgfzf: нужен %s\n' "$tool" >&2
        exit 1
    fi
done

# В Debian и Ubuntu bat называется batcat.
PREVIEW_BIN=""
if command -v bat >/dev/null 2>&1; then
    PREVIEW_BIN="bat"
elif command -v batcat >/dev/null 2>&1; then
    PREVIEW_BIN="batcat"
fi

if [ -n "$PREVIEW_BIN" ]; then
    PREVIEW="$PREVIEW_BIN -p --color=always {1} --highlight-line {2}"
else
    PREVIEW="sed -n '{2}p' {1}"
fi

selection=$(
    rg --color=always --line-number --no-heading --smart-case "${*:-}" 2>/dev/null \
    | fzf -d':' --ansi \
        --preview "$PREVIEW" \
        --preview-window '~8,+{2}-5'
) || exit 0

[ -n "$selection" ] || exit 0

file=$(printf '%s' "$selection" | awk -F':' '{print $1}')
line=$(printf '%s' "$selection" | awk -F':' '{print $2}')

# Внутри vim нас зовут за именем файла — редактор откроет его сам.
if [ -n "${VIM_TERMINAL:-}${VIMRUNTIME:-}" ]; then
    printf '%s\n' "$file"
    exit 0
fi

if [ -n "${EDITOR:-}" ] && [ -n "$line" ]; then
    exec "$EDITOR" "+$line" "$file"
fi
printf '%s\n' "$file"
