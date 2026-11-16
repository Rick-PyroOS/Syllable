-- Open files as tabs across the top, like CLion's editor tabs.
-- Shift-h / Shift-l (or [b / ]b) move between them, Space bd closes one,
-- Space 1..9 jumps to tab N, and you can click a tab with the mouse.
-- Resting the mouse on a tab shows its full path (with a copy icon), scrolling
-- sideways over the tabs scrolls them, and right-click opens a menu
-- (lua/config/tabs.lua).
-- When the editor is split, these are the first split's tabs and each other
-- split has its own row (lua/config/split_tabs.lua); the keys below then work
-- on the tabs of the split you're in.
local function split_tabs() return require("config.split_tabs") end

return {
  "akinsho/bufferline.nvim",
  version = "*",
  lazy = false,
  dependencies = { "nvim-tree/nvim-web-devicons" },
  keys = {
    { "<S-h>", function() split_tabs().cycle(-1) end, desc = "Previous tab" },
    { "<S-l>", function() split_tabs().cycle(1) end, desc = "Next tab" },
    { "[b", function() split_tabs().cycle(-1) end, desc = "Previous tab" },
    { "]b", function() split_tabs().cycle(1) end, desc = "Next tab" },
    { "<leader>bd", function() split_tabs().close_current() end, desc = "Close tab" },
    { "<leader>bo", function() split_tabs().close_current_others() end, desc = "Close other tabs" },
    { "<leader>bp", "<cmd>BufferLinePick<cr>", desc = "Pick tab" },
    { "<leader>b<", "<cmd>BufferLineMovePrev<cr>", desc = "Move tab left" },
    { "<leader>b>", "<cmd>BufferLineMoveNext<cr>", desc = "Move tab right" },
    { "<leader>1", function() split_tabs().go(1) end, desc = "Tab 1" },
    { "<leader>2", function() split_tabs().go(2) end, desc = "Tab 2" },
    { "<leader>3", function() split_tabs().go(3) end, desc = "Tab 3" },
    { "<leader>4", function() split_tabs().go(4) end, desc = "Tab 4" },
    { "<leader>5", function() split_tabs().go(5) end, desc = "Tab 5" },
    { "<leader>6", function() split_tabs().go(6) end, desc = "Tab 6" },
    { "<leader>7", function() split_tabs().go(7) end, desc = "Tab 7" },
    { "<leader>8", function() split_tabs().go(8) end, desc = "Tab 8" },
    { "<leader>9", function() split_tabs().go(9) end, desc = "Tab 9" },
  },
  opts = {
    options = {
      diagnostics = "nvim_lsp",          -- error/warning counts on each tab
      close_command = function(n) split_tabs().close(n) end,
      right_mouse_command = function(n) require("config.tabs").menu(n) end,
      middle_mouse_command = function(n) split_tabs().close(n) end,
      -- Split editor: only the first split's files (and ones no split has open).
      custom_filter = function(n) return split_tabs().main_shows(n) end,
      always_show_bufferline = true,
      tab_size = 0,                      -- each tab only as wide as its name, so more fit
      -- Leave room for the project tree on the left.
      -- The gap right of it is blank, like the gaps between the other panes.
      offsets = { { filetype = "snacks_layout_box", text = "Project", highlight = "Directory", separator = " " } },
    },
  },
}
