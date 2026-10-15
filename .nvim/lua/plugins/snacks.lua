-- snacks.nvim: fuzzy finder, file explorer, terminal (used by Claude), notifications.

-- Whether the last mouse press landed on a window edge (the line between the
-- project tree and the editor) rather than on text, for the tree's mouse keys below.
local edge_press = false
vim.on_key(function(key)
  if key == vim.keycode("<LeftMouse>") then
    local m = vim.fn.getmousepos()
    edge_press = m.winid ~= 0 and (m.line == 0 or m.wincol > vim.api.nvim_win_get_width(m.winid))
  end
end)

return {
  "folke/snacks.nvim",
  priority = 1000,
  lazy = false,
  opts = {
    bigfile = { enabled = true },
    explorer = { enabled = true, replace_netrw = true },
    -- Real images only where the terminal speaks kitty graphics; elsewhere config/image_view.lua.
    image = { enabled = require("config.image_view").kitty() },
    input = { enabled = true },
    -- Messages show in the CLion-style balloon (config/balloon.lua); Snacks keeps the history.
    notifier = { enabled = true, filter = function(notif) return require("config.balloon").filter(notif) end },
    picker = {
      enabled = true,
      sources = {
        explorer = {
          -- Errors show on files only; config/explorer.lua keeps them off folders.
          diagnostics_open = false,
          -- Following the current file is Space tl in config/explorer.lua (off by default).
          follow_file = false,
          -- ▸ / ▾ in front of folders, and the width you last dragged the tree to.
          format = function(item, picker) return require("config.explorer").format(item, picker) end,
          layout = function() return require("config.explorer").layout() end,
          on_show = function(picker) require("config.explorer").on_show(picker) end,
          on_close = function(picker) require("config.explorer").on_close(picker) end,
          actions = {
            -- Back to the file you were editing; the tree stays open.
            focus_editor = function(picker)
              local win = picker.main
              if win and vim.api.nvim_win_is_valid(win) then vim.api.nvim_set_current_win(win) else vim.cmd("wincmd p") end
            end,
            -- Release after a click in the tree: open the file / expand the folder,
            -- unless the press was on the tree's edge (dragging it to resize).
            click_open = function(picker)
              if not edge_press then picker:action("confirm") end
            end,
            -- Esc in the tree's filter box clears the filter and goes back to the tree.
            clear_filter = function(picker)
              picker.input:set("", "")
              picker:focus("list")
            end,
          },
          win = {
            list = {
              keys = {
                -- Single click opens a file or expands a folder. A trackpad click often
                -- jiggles a cell, which would start a text selection instead, so
                -- dragging inside the tree is ignored; dragging its edge resizes it.
                ["<LeftDrag>"] = { function() return edge_press and "<LeftDrag>" or "" end, expr = true },
                ["<LeftRelease>"] = "click_open",
                ["<Esc>"] = "focus_editor", -- Esc leaves the tree instead of closing it, like CLion
              },
            },
            input = { keys = { ["<Esc>"] = { "clear_filter", mode = { "n", "i" } } } },
          },
        },
      },
    },
    -- No title row on terminal panels: the title shows in the gutter row above
    -- (split_tabs.lua), so the panel isn't padded by two rows.
    terminal = { enabled = true, win = { wo = { winbar = "" } } },
  },
  init = function()
    -- Project tree on the left at startup, like CLion: when Neovim starts in a
    -- folder (nvim, nvim .) or on a file inside a git repo. Focus stays on the file.
    vim.api.nvim_create_autocmd("VimEnter", {
      group = vim.api.nvim_create_augroup("rick_project_tree", { clear = true }),
      callback = function()
        local arg = vim.fn.argv(0)
        if vim.bo.filetype == "gitcommit" or vim.bo.filetype == "gitrebase" then return end
        local is_dir = arg ~= "" and vim.fn.isdirectory(arg) == 1
        if is_dir then return end -- replace_netrw already opens the explorer for a folder
        if arg ~= "" and not vim.fs.root(0, ".git") then return end
        local file_win = vim.api.nvim_get_current_win()
        vim.schedule(function()
          Snacks.explorer({
            on_show = function(picker)
              require("config.explorer").on_show(picker)
              if arg ~= "" then
                vim.schedule(function()
                  if vim.api.nvim_win_is_valid(file_win) then vim.api.nvim_set_current_win(file_win) end
                end)
              end
            end,
          })
        end)
      end,
    })
  end,
  keys = {
    { "<leader><space>", function() require("config.command_palette").open() end, desc = "Command palette" },
    { "<leader>p", function() require("config.command_palette").open() end, desc = "Command palette" },
    { "<leader>ff", function() Snacks.picker.files() end, desc = "Find files" },
    { "<leader>fg", function() Snacks.picker.grep() end, desc = "Grep" },
    { "<leader>fw", function() Snacks.picker.grep_word() end, desc = "Grep word under cursor", mode = { "n", "x" } },
    { "<leader>fb", function() Snacks.picker.buffers() end, desc = "Buffers" },
    { "<leader>fr", function() Snacks.picker.recent() end, desc = "Recent files" },
    { "<leader>fs", function() Snacks.picker.lsp_symbols() end, desc = "Symbols in file" },
    { "<leader>fS", function() Snacks.picker.lsp_workspace_symbols() end, desc = "Symbols in project" },
    { "<leader>fd", function() require("config.diagnostics").picker() end, desc = "Diagnostics (all problems)" },
    { "<leader>fh", function() Snacks.picker.help() end, desc = "Help pages" },
    { "<leader>fk", function() Snacks.picker.keymaps() end, desc = "Keymaps" },
    { "<leader>e", function() Snacks.explorer() end, desc = "File explorer" },
    { "<leader>te", function() require("config.explorer").toggle_remember() end, desc = "Toggle explorer remembering open folders" },
    { "<leader>tl", function() require("config.explorer").toggle_follow() end, desc = "Toggle project tree following the current file" },
    { "<leader>gs", function() Snacks.picker.git_status() end, desc = "Git status" },
    { "<leader>gl", function() Snacks.picker.git_log_file() end, desc = "Git log (file)" },
    { "<leader>gb", function() Snacks.picker.git_branches() end, desc = "Git branches" },
    { "<leader>tt", function() Snacks.terminal() end, desc = "Terminal", mode = { "n", "t" } },
  },
}
