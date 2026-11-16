-- CLion's macOS keymap, as far as it maps onto Neovim.
--
-- iTerm2 keeps Cmd shortcuts for its own menus, so Cmd keys only reach Neovim
-- through the "Neovim (CLion keys)" iTerm profile in extras/iterm2/, which sends
-- each one as a CSI u escape code that Neovim reads as <D-...>. In a GUI like
-- Neovide, <D-...> works with no setup. The Space-leader keys still work too.
-- Keep this list in sync with extras/iterm2/make_profile.py.

local map = vim.keymap.set
local n, nx, ni, nxi = "n", { "n", "x" }, { "n", "i" }, { "n", "x", "i" }

local dialogs = require("config.dialogs") -- one dialog at a time
local function pick(name, opts) return function() dialogs.open(name, function() return Snacks.picker[name](opts) end) end end

-- Navigation -----------------------------------------------------------------
local function search(tab) return function() require("config.search_everywhere").open(tab) end end
map(n, "<D-o>", search("Classes"), { desc = "Go to class" })
map(n, "<D-O>", search("Files"), { desc = "Go to file" })                            -- Cmd+Shift+O
map(n, "<D-M-o>", search("Symbols"), { desc = "Go to symbol" })
map(n, "<D-e>", pick("recent"), { desc = "Recent files" })
local function palette() require("config.command_palette").open() end
map(nxi, "<D-A>", search("Actions"), { desc = "Find action" })                     -- Cmd+Shift+A
map(nxi, "<D-P>", palette, { desc = "Command palette" })                            -- Cmd+Shift+P
map(nxi, "<F19>", palette, { desc = "Find action (Shift Shift)" })                 -- sent by extras/karabiner/
map(n, "<D-b>", function() require("config.define").definition() end, { desc = "Go to declaration" })
map(n, "<D-M-b>", vim.lsp.buf.implementation, { desc = "Go to implementation" })
map(n, "<M-F7>", pick("lsp_references"), { desc = "Find usages" })
map(n, "<F55>", pick("lsp_references"), { desc = "Find usages" })                     -- Alt+F7 as terminals send it
map(n, "<D-M-F7>", function() require("config.usages").show() end, { desc = "Show usages" }) -- popup under the name
map(n, "<D-C-Up>", "<cmd>LspClangdSwitchSourceHeader<cr>", { desc = "Switch header/source" })
map(n, "<D-[>", "<C-o>", { desc = "Back" })
map(n, "<D-]>", "<C-i>", { desc = "Forward" })
map(n, "<D-F12>", pick("lsp_symbols"), { desc = "File structure" })
map(n, "<D-l>", function()
  vim.ui.input({ prompt = "Go to line: " }, function(l)
    if l and tonumber(l) then vim.api.nvim_win_set_cursor(0, { math.min(tonumber(l), vim.api.nvim_buf_line_count(0)), 0 }) end
  end)
end, { desc = "Go to line" })
map(n, "<F2>", function() vim.diagnostic.jump({ count = 1, severity = vim.diagnostic.severity.ERROR }) end, { desc = "Next error" })
map(n, "<S-F2>", function() vim.diagnostic.jump({ count = -1, severity = vim.diagnostic.severity.ERROR }) end, { desc = "Previous error" })
map(n, "<F14>", function() vim.diagnostic.jump({ count = -1, severity = vim.diagnostic.severity.ERROR }) end, { desc = "Previous error" })

-- Tool windows and tabs --------------------------------------------------------
map(n, "<D-1>", function() Snacks.explorer() end, { desc = "Project tree" })
-- Alt+F12 (Terminal) is in config/toolwindow.lua.
map(n, "<D-w>", function() require("config.split_tabs").close_current() end, { desc = "Close tab" })
for _, lhs in ipairs({ "<D-S-[>", "<D-{>" }) do                                       -- Cmd+Shift+[
  map(n, lhs, function() require("config.split_tabs").cycle(-1) end, { desc = "Previous tab" })
end
for _, lhs in ipairs({ "<D-S-]>", "<D-}>" }) do                                       -- Cmd+Shift+]
  map(n, lhs, function() require("config.split_tabs").cycle(1) end, { desc = "Next tab" })
end

-- Search -----------------------------------------------------------------------
map(n, "<D-f>", search("All"), { desc = "Find (files, classes, symbols, text)" })        -- / still searches the file
map(n, "<D-r>", ":%s/", { desc = "Replace in file" })
-- Find in Files / Replace in Files: one popup, empty boxes, results as you type
-- (lua/config/find_in_files.lua). Ctrl+R in it switches between the two.
local function in_files(mode) return function() require("config.find_in_files").open(mode) end end
-- Cmd+Shift+F / Cmd+Shift+R work from anywhere, as in CLion: also while typing
-- and from a terminal panel.
local function in_files_anywhere(mode)
  return function()
    if vim.fn.mode():match("^[it]") then vim.cmd.stopinsert() end
    vim.schedule(in_files(mode))
  end
end
map(nx, "<D-F>", in_files("find"), { desc = "Find in files" })                        -- Cmd+Shift+F
map(nx, "<D-R>", in_files("replace"), { desc = "Replace in files" })                  -- Cmd+Shift+R
map({ "i", "t" }, "<D-F>", in_files_anywhere("find"), { desc = "Find in files" })
map({ "i", "t" }, "<D-R>", in_files_anywhere("replace"), { desc = "Replace in files" })
map(nx, "<leader>sf", in_files("find"), { desc = "Find in files" })
map(nx, "<leader>sr", in_files("replace"), { desc = "Replace in files" })

-- Call hierarchy: who calls the function under the cursor (clangd), as a popup
-- with a preview; Enter jumps. Space co lists what it calls instead.
map(n, "<C-M-h>", pick("lsp_incoming_calls", { title = "Callers" }), { desc = "Call hierarchy: callers" }) -- Ctrl+Alt+H
map(n, "<C-M-o>", function() require("config.optimize_includes").run() end, { desc = "Optimize imports (#includes)" }) -- Ctrl+Alt+O
map(n, "<leader>cI", function() require("config.optimize_includes").run() end, { desc = "Optimize imports (#includes)" })
map(n, "<leader>ch", function() require("config.hierarchy").open() end, { desc = "Type hierarchy" })
map(n, "<leader>ci", pick("lsp_incoming_calls", { title = "Callers" }), { desc = "Call hierarchy: callers" })
map(n, "<leader>co", pick("lsp_outgoing_calls", { title = "Callees" }), { desc = "Call hierarchy: callees" })

-- Event Log, like CLion's: every message that popped up (warnings included),
-- newest first, with the whole text in a preview; up/down step through them.
-- The preview shows the message as plain text, wrapped: as markdown, short
-- messages lost their [brackets] and *stars* (make[1]: *** [x.o] Error 1).
local event_log = {
  title = "Event Log",
  transform = function(item) if item.preview then item.preview.ft = "text" end end,
  win = { preview = { wo = { wrap = true, linebreak = true, number = false, signcolumn = "no", foldcolumn = "0" } } },
}
map(n, "<leader>n", pick("notifications", event_log), { desc = "Event log (past messages)" })

-- Editing ----------------------------------------------------------------------
map(nxi, "<D-s>", "<cmd>wall<cr>", { desc = "Save all" })
map(n, "<D-z>", "u", { desc = "Undo" })
map(n, "<D-Z>", "<C-r>", { desc = "Redo" })                                           -- Cmd+Shift+Z
map("i", "<D-z>", "<C-o>u", { desc = "Undo" })
map("i", "<D-Z>", "<C-o><C-r>", { desc = "Redo" })
map(n, "<D-a>", "ggVG", { desc = "Select all" })
-- Cmd+/ or Ctrl+/ comments or uncomments the line or the selection (the
-- selection stays selected). Some terminals send Ctrl+/ as Ctrl+_.
for _, lhs in ipairs({ "<D-/>", "<C-/>", "<C-_>" }) do
  map(n, lhs, "gcc", { remap = true, desc = "Comment line" })
  map("x", lhs, "gcgv", { remap = true, desc = "Comment selection" })
  map("i", lhs, "<C-o>gcc", { remap = true, desc = "Comment line" })
end
map(nx, "<D-M-t>", function() require("config.surround").open() end, { desc = "Surround with" })  -- Cmd+Alt+T
map(nx, "<leader>cs", function() require("config.surround").open() end, { desc = "Surround with" })
map(n, "<D-d>", "<cmd>t.<cr>", { desc = "Duplicate line" })
map("x", "<D-d>", ":t'><cr>gv", { desc = "Duplicate selection" })
map(n, "<D-BS>", '"_dd', { desc = "Delete line" })
map("i", "<D-BS>", '<C-o>"_dd', { desc = "Delete line" })
for _, lhs in ipairs({ "<M-S-Up>", "<D-S-Up>" }) do                                   -- move line / statement
  map(n, lhs, "<cmd>m .-2<cr>==", { desc = "Move line up" })
  map("x", lhs, ":m '<-2<cr>gv=gv", { desc = "Move selection up" })
end
for _, lhs in ipairs({ "<M-S-Down>", "<D-S-Down>" }) do
  map(n, lhs, "<cmd>m .+1<cr>==", { desc = "Move line down" })
  map("x", lhs, ":m '>+1<cr>gv=gv", { desc = "Move selection down" })
end
map(nx, "<D-M-l>", function() require("conform").format({ async = true, lsp_format = "fallback" }) end, { desc = "Reformat code" })
map(nx, "<M-CR>", vim.lsp.buf.code_action, { desc = "Show intention actions" })
map(n, "<S-F6>", vim.lsp.buf.rename, { desc = "Rename" })
map(n, "<F18>", vim.lsp.buf.rename, { desc = "Rename" })                              -- Shift+F6 as terminals send it
-- On a call with missing arguments, Cmd+P shows its constructors/overloads
-- (cycle with ]o / [o or the mouse wheel); otherwise the usual signature help.
-- Pressed again while it's showing, it goes into the popup (from insert mode
-- too) so you can select and copy (y); q goes back. F1 / K work the same way.
local function signature_open()
  for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if pcall(vim.api.nvim_win_get_var, w, "textDocument/signatureHelp") then return w end
  end
end
map(ni, "<D-p>", function()
  local d = require("config.diagnostics")
  if vim.fn.mode() == "n" and d.has_call(vim.api.nvim_get_current_buf(), vim.fn.line(".") - 1) then
    d.show_cursor()
  elseif vim.fn.mode() == "i" and signature_open() then
    -- The popup is open: step into it, in normal mode (going in first, as
    -- leaving insert mode moves the cursor, which closes the popup).
    vim.api.nvim_set_current_win(signature_open())
    vim.cmd.stopinsert()
  else
    pcall(function() require("blink.cmp").hide_signature() end) -- this one replaces the one shown while typing
    vim.lsp.buf.signature_help()
  end
end, { desc = "Parameter info" })
map(n, "<F1>", vim.lsp.buf.hover, { desc = "Quick documentation" })
