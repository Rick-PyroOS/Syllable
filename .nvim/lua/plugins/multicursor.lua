-- Multiple carets, CLion style. Ctrl+G puts a caret on the next occurrence of
-- the word under the cursor (or of the selection); Ctrl+Alt+G on all of them
-- (Ctrl+Cmd+G in CLion). Then typing edits them all, n / N step to the
-- next / previous match, q skips one, Q drops the current caret, Esc ends.
return {
  "mg979/vim-visual-multi",
  keys = { { "<C-g>", mode = { "n", "x" } }, { "<C-M-g>", mode = { "n", "x" } } },
  init = function()
    vim.g.VM_default_mappings = 0
    vim.g.VM_maps = {
      ["Find Under"] = "<C-g>",
      ["Find Subword Under"] = "<C-g>",
      ["Select All"] = "<C-M-g>",
      ["Visual All"] = "<C-M-g>",
    }
    vim.g.VM_silent_exit = 1
    vim.g.VM_show_warnings = 0
  end,
}
