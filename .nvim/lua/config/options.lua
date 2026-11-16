local o = vim.opt

o.number = true
o.relativenumber = false       -- plain line numbers, as in CLion
o.signcolumn = "yes:3"        -- room for E (clangd), S (cppcheck/semgrep) and M (local model) side by side
o.cursorline = true
o.winborder = "rounded"        -- popups (completion, its docs, parameter info, K) get a frame, so they don't blend into the code
o.scrolloff = 8
o.termguicolors = true
o.title = true                 -- the terminal's window title: "nvim · file.cpp"; Shift Shift via
o.titlestring = "nvim · %t"    -- Hammerspoon (extras/hammerspoon) only fires when it starts with "nvim"
o.mouse = "a"
-- Arrow keys like CLion's: the cursor can sit just past the end of a line,
-- Right there goes on to the next line (Left at the start back to the last).
o.virtualedit = "onemore"
o.whichwrap = "b,s,<,>,[,]"
o.clipboard = "unnamedplus"   -- share the macOS clipboard
o.undofile = true             -- undo survives closing the file
o.ignorecase = true
o.smartcase = true
o.splitright = true
o.splitbelow = true
o.updatetime = 250
o.timeoutlen = 400
o.wrap = false                -- Space tw turns word wrap on (config/prefs.lua)

-- Folding from the margin, like CLion: an arrow next to each function, class
-- and block (from treesitter); click it, or za, to fold or unfold. Files open
-- unfolded, and a folded block shows its first line in its own colors.
-- foldmethod / foldexpr are set where treesitter starts (plugins/treesitter.lua).
o.foldcolumn = "1"
o.foldtext = ""
o.foldlevelstart = 99
o.foldminlines = 2            -- no arrow on a block shorter than 3 lines
o.fillchars:append({ fold = " ", foldopen = "▾", foldclose = "▸", foldsep = " ", foldinner = " " })
-- Fold arrows and line numbers bright enough to see (tokyonight draws them
-- nearly in the background color). The current line's number stays orange.
local function margin_colors()
  vim.api.nvim_set_hl(0, "FoldColumn", { fg = "#a9b1d6", bold = true })
  vim.api.nvim_set_hl(0, "CursorLineFold", { fg = "#c0caf5", bold = true })
  for _, name in ipairs({ "LineNr", "LineNrAbove", "LineNrBelow" }) do
    vim.api.nvim_set_hl(0, name, { fg = "#737aa2" })
  end
end
margin_colors()
vim.api.nvim_create_autocmd("ColorScheme", {
  group = vim.api.nvim_create_augroup("rick_margin_colors", { clear = true }),
  callback = margin_colors,
})

-- Panes as separate frames, like the project tree's: the lines between
-- splits, the bottom terminal and the tree are blank gaps in a darker gutter
-- color (the tab row's), not drawn lines. The bars between panes stacked
-- top to bottom are the same gutter, or hold the tabs of the split below
-- (config/split_tabs.lua).
o.fillchars:append({ vert = " ", horiz = " ", horizup = " ", horizdown = " ", vertleft = " ", vertright = " ", verthoriz = " " })
local function gutter_color()
  local fill = vim.api.nvim_get_hl(0, { name = "BufferLineFill", link = false }).bg
    or vim.api.nvim_get_hl(0, { name = "TabLineFill", link = false }).bg or 0x0e0e14
  vim.api.nvim_set_hl(0, "WinSeparator", { fg = fill, bg = fill })
end
gutter_color()
vim.api.nvim_create_autocmd("ColorScheme", {
  group = vim.api.nvim_create_augroup("rick_gutter", { clear = true }),
  callback = function() vim.schedule(gutter_color) end, -- after bufferline sets its colors
})

-- A cursor that's easy to see: a bright orange block, blinking while you type
-- (insert mode). iTerm takes the color from Neovim.
o.guicursor = "n-v-c-sm:block-RickCursor,i-ci-ve:block-blinkwait300-blinkon500-blinkoff400-RickCursor,"
  .. "r-cr-o:hor20-RickCursor,t:block-blinkon500-blinkoff500-TermCursor"
local function cursor_color() vim.api.nvim_set_hl(0, "RickCursor", { fg = "#1a1b26", bg = "#ff9e64" }) end
cursor_color()
vim.api.nvim_create_autocmd("ColorScheme", {
  group = vim.api.nvim_create_augroup("rick_cursor", { clear = true }),
  callback = cursor_color,
})
o.shortmess:append("I")      -- no "NVIM v..." intro screen on startup
o.laststatus = 0              -- no status bar (Space tb shows it); the path is on the tab's hover
o.cmdheight = 2               -- a blank row between the code and the bottom line (line, column): a bottom margin
o.title = true                -- window title like CLion: "MyProject – window.cpp +"
o.titlestring = "%{fnamemodify(getcwd(), ':t')} – %t%( %M%)"

-- Tabs at width 4 by default. guess-indent.nvim overrides this per file when
-- a file clearly uses something else, and a project's .nvim.lua can change it.
o.tabstop = 4
o.shiftwidth = 4
o.softtabstop = 0
o.expandtab = false

-- Show trailing whitespace; tabs stay blank.
o.list = true
o.listchars = { tab = "  ", trail = "·", nbsp = "␣" }

-- Claude (and git, rsync pulls) change files behind the editor's back.
-- Reload them automatically when nothing is unsaved.
o.autoread = true
vim.api.nvim_create_autocmd({ "FocusGained", "BufEnter", "CursorHold" }, {
  group = vim.api.nvim_create_augroup("rick_autoread", { clear = true }),
  callback = function()
    if vim.fn.mode() ~= "c" then vim.cmd("checktime") end
  end,
})

-- Briefly highlight yanked text.
vim.api.nvim_create_autocmd("TextYankPost", {
  group = vim.api.nvim_create_augroup("rick_yank", { clear = true }),
  callback = function() vim.hl.on_yank() end,
})

-- Project settings: a .nvim.lua in the folder you start Neovim in runs after
-- this config (file types, indentation, a clang-format fallback...). Neovim
-- asks once whether to trust it, and again whenever it changes.
o.exrc = true

-- .h is ambiguous. Treat it as C++ when it looks like C++ (classes, namespaces,
-- templates, ::). Otherwise (including a new, empty header) go by the folder:
-- C++ unless it holds only .c sources. Picks the right formatter and highlighting.
vim.filetype.add({
  extension = {
    h = function(path, bufnr)
      local lines = vim.api.nvim_buf_get_lines(bufnr, 0, 400, false)
      for _, l in ipairs(lines) do
        if l:match("^%s*class%s") or l:match("^%s*namespace[%s{]") or l:match("^%s*template%s*<")
          or l:match("^%s*using%s") or l:match("^%s*public:") or l:match("^%s*private:")
          or l:match("^%s*extern%s+\"C%+%+\"") or l:match("%w::%w") then
          return "cpp"
        end
      end
      local dir = vim.fs.dirname(vim.fs.abspath(path))
      local n_c, n_cpp = 0, 0
      for name, kind in vim.fs.dir(dir) do
        if kind == "file" then
          if name:match("%.c$") then n_c = n_c + 1
          elseif name:match("%.cpp$") or name:match("%.cc$") or name:match("%.cxx$") then n_cpp = n_cpp + 1 end
        end
      end
      return (n_c > 0 and n_cpp == 0) and "c" or "cpp"
    end,
  },
})
