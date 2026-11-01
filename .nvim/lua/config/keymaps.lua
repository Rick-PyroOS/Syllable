local map = vim.keymap.set

map("n", "<Esc>", "<cmd>nohlsearch<cr>", { desc = "Clear search highlight" })

-- Space on its own does nothing (Vim's Space moves right, onto the next line),
-- and it waits for the rest of a Space key with no time limit, so Space Space
-- or Space t w still work after a pause. Esc gives up (and does what Esc does).
local function leader(mode)
  return function()
    local seq = " "
    while true do
      local ok, c = pcall(vim.fn.getcharstr)
      if not ok or c == "\3" then return end
      if c == "\27" then return vim.api.nvim_feedkeys(c, "n", false) end -- Esc as usual
      seq = seq .. c
      local exact, longer = false, false
      for _, list in ipairs({ vim.api.nvim_buf_get_keymap(0, mode), vim.api.nvim_get_keymap(mode) }) do
        for _, m in ipairs(list) do
          local lhs = m.lhsraw or vim.keycode(m.lhs)
          if lhs == seq then exact = true elseif #lhs > #seq and lhs:sub(1, #seq) == seq then longer = true end
        end
      end
      -- Run it (Neovim picks the mapping, and still waits its usual time if a
      -- longer one starts the same way); nothing starts like this: drop it.
      if exact then return vim.api.nvim_feedkeys(seq, "m", false) end
      if not longer then return end
    end
  end
end
map("n", "<Space>", leader("n"))
map("x", "<Space>", leader("x"))
map("n", "<leader>w", "<cmd>write<cr>", { desc = "Save" })
map("n", "<leader>tb", function()
  vim.o.laststatus = vim.o.laststatus == 0 and 3 or 0
end, { desc = "Toggle status bar" })

-- Down on the last line goes to its end, Up on the first line to its start
-- (CLion), instead of doing nothing. With a right-click menu open they move
-- in the menu.
map("n", "<Down>", function()
  if vim.fn.pumvisible() == 1 then return "<Down>" end
  local l = vim.fn.line(".")
  if l == vim.fn.line("$") then
    return ("<Cmd>call cursor(%d, %d)<CR>"):format(l, #vim.fn.getline(l) + 1)
  end
  return "j"
end, { expr = true })
map("n", "<Up>", function()
  if vim.fn.pumvisible() == 1 then return "<Up>" end
  if vim.fn.line(".") == 1 then return "<Cmd>call cursor(1, 1)<CR>" end
  return "k"
end, { expr = true })
-- End goes just past the last character, where typing adds to the line (as
-- in CLion, and as the arrows can), not onto the last character.
map("n", "<End>", function()
  vim.api.nvim_win_set_cursor(0, { vim.fn.line("."), #vim.fn.getline(".") })
end, { desc = "End of line" })

-- Window movement
map("n", "<C-h>", "<C-w>h", { desc = "Window left" })
map("n", "<C-j>", "<C-w>j", { desc = "Window down" })
map("n", "<C-k>", "<C-w>k", { desc = "Window up" })
map("n", "<C-l>", "<C-w>l", { desc = "Window right" })

-- Leave terminal mode (Claude runs in a terminal split) with Esc Esc
map("t", "<Esc><Esc>", [[<C-\><C-n>]], { desc = "Exit terminal mode" })

-- Keep selection when indenting
map("v", "<", "<gv")
map("v", ">", ">gv")

-- Diagnostics
map("n", "<leader>cd", function() require("config.diagnostics").show_cursor(true) end, { desc = "Line diagnostics" })
