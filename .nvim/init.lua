-- Neovim config for Syllable (Pyro) work. Install: .nvim/install.sh (see README.md).
-- Layout:
--   lua/config/options.lua  editor settings
--   lua/config/keymaps.lua  global keymaps
--   lua/config/prefs.lua    Space t toggles: word wrap, Ctrl+Z, double-click, key popup, mouse docs
--   lua/config/lsp.lua      clangd / lua_ls setup and LSP keymaps
--   lua/config/diagnostics.lua  error popup on E/S hover, missing arguments in red
--   lua/config/quick_docs.lua  clangd's docs when the mouse rests on a name (Space tq)
--   lua/config/doc_links.lua  names in doc popups are links (click, Enter, Ctrl+])
--   lua/config/scanners.lua  cppcheck / semgrep in the background (the S in the margin)
--   lua/config/mcp.lua       local model checks (the M in the margin; mcp/server.py)
--   lua/config/tabs.lua     tab hover path (click to copy), sideways scroll, right-click menu
--   lua/config/split_tabs.lua  each split's own tab row, like CLion
--   lua/config/explorer.lua project tree right-click menu, problems popup, Space te
--   lua/config/menu.lua     right-click menus (needs iTerm to report right clicks)
--   lua/config/usages.lua   Show Usages popup (Cmd+Alt+F7, Space cu, Ctrl+click a declaration)
--   lua/config/scrollbar.lua scrollbars on files and the project tree
--   lua/config/pointer.lua  mouse pointer shapes (arrow, text beam, resize)
--   lua/config/binary.lua   asks before opening a binary file
--   lua/config/clion_keys.lua  CLion macOS shortcuts (Cmd keys need the iTerm profile)
--   lua/config/cheatsheet.lua  Space ? / Cmd+Shift+/ keymap popup
--   lua/config/quit.lua     :q quits everything, asking about unsaved files
--   lua/config/autosave.lua auto-save
--   lua/config/raycast.lua  lets the Raycast extension (Syllable/.raycast) drive Neovim
--   lua/config/lazy.lua     plugin manager bootstrap
--   lua/plugins/*.lua       one file per plugin area

if vim.fn.has("nvim-0.11") == 0 then
  vim.api.nvim_echo({ { "This config needs Neovim 0.11 or newer (brew upgrade neovim)", "ErrorMsg" } }, true, {})
  return
end

require("config.cheatsheet").snapshot()   -- before anything adds mappings

vim.g.mapleader = " "
vim.g.maplocalleader = "\\"

require("config.options")
require("config.keymaps")
require("config.prefs").setup()
require("config.binary").setup()
require("config.bookmarks").setup()
require("config.local_history").setup()
require("config.diff").setup()
require("config.recent_locations").setup()
require("config.balloon").setup()
require("config.templates").setup()
require("config.lazy")
require("config.lsp")
require("config.diagnostics").setup()
require("config.quick_docs").setup()
require("config.doc_links").setup()
require("config.copy_button").setup()
require("config.scanners").setup()
require("config.mcp").setup()
require("config.menu").setup()
require("config.usages").setup()
require("config.tabs").setup()
require("config.split_tabs").setup() -- each split's own tab row
require("config.explorer").setup()
require("config.scrollbar").setup()
require("config.pointer").setup()
require("config.clion_keys")
require("config.findbar").setup() -- after clion_keys: takes over / , Cmd+F and Cmd+R when the find bar is on
require("config.find_in_files").setup() -- Find / Replace in Files popup (Cmd+Shift+F, Cmd+Shift+R)
require("config.build").setup() -- after clion_keys: F2 / Shift+F2 also walk build errors
require("config.panels").setup()
require("config.quit")
require("config.autosave")
require("config.raycast").setup()
require("config.cheatsheet").setup()
