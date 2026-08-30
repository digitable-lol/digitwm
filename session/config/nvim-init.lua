-- ===========================================================================
-- Digitable Session — Neovim.
--
-- Neovim здесь намеренно НЕ получает отдельной раскладки. Владелец живёт в
-- vim, и держать две расходящиеся конфигурации — это способ гарантировать,
-- что одна из них тихо отстанет. Поэтому init.lua подключает тот же самый
-- session/config/vimrc: клавиши, повадки и плагины совпадают до строчки.
--
-- Отличается только цвет: у Neovim собственный генератор темы в Workbench
-- (products/workbench/scripts/targets/neovim.mjs), он даёт .lua-схему с
-- поддержкой treesitter-групп, и она лучше vim-схемы. Её и берём.
--
-- Файл ставится как ~/.config/nvim/init.lua. Если у вас уже был свой
-- init.lua, установщик его не трогает: он положит этот рядом под именем
-- digitwm-session.lua и напечатает строку для подключения.
--
-- Строка ниже — метка установщика, и она здесь не для красоты. По ней
-- повторный запуск узнаёт СВОЙ ЖЕ файл и обновляет его. Без метки второй
-- запуск считал этот файл вашим и клал рядом второй, digitwm-session.lua,
-- вместе с советом подключить его через dofile — конфигурация Neovim
-- удваивалась на ровном месте. Уберите метку, и файл станет вашим:
-- установщик больше его не тронет и положит свою копию рядом.
--
-- digitwm-session: managed file
-- ===========================================================================

local shared = vim.fn.expand('@@VIMRC_TARGET@@')

if vim.fn.filereadable(shared) == 1 then
  vim.cmd('source ' .. vim.fn.fnameescape(shared))
end

vim.opt.termguicolors = true

-- Схема приезжает файлом из Workbench в ~/.config/nvim/colors.
-- pcall — чтобы отсутствие файла темы не роняло старт редактора.
pcall(vim.cmd.colorscheme, '@@VIM_COLORSCHEME@@')

-- Neovim умеет то, чего нет в vim 8: подсветка скопированного куска
-- средствами ядра, без плагина.
vim.api.nvim_create_autocmd('TextYankPost', {
  group = vim.api.nvim_create_augroup('digitable_yank', { clear = true }),
  callback = function()
    vim.highlight.on_yank({ higroup = 'IncSearch', timeout = 700 })
  end,
})
