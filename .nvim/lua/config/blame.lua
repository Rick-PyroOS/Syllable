-- Annotate with Git Blame, like CLion's: the whole file's blame in a column on
-- the left. Running it again closes the column instead of opening a second one
-- (gitsigns would fail with E95, since the column's buffer name is taken), so
-- Space ga and the right-click entry both toggle it; the right-click entry
-- reads "Close Annotations" while the column is open. In the column, q or Esc
-- closes it too, and a right-click there does nothing.

local M = {}

-- The blame column's window in this tab, if open.
function M.win()
  for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if vim.bo[vim.api.nvim_win_get_buf(w)].filetype == "gitsigns-blame" then return w end
  end
end

function M.toggle()
  local w = M.win()
  if w then
    vim.api.nvim_win_close(w, true)
  else
    require("gitsigns").blame()
  end
end

vim.api.nvim_create_autocmd("FileType", {
  group = vim.api.nvim_create_augroup("rick_blame", { clear = true }),
  pattern = "gitsigns-blame",
  callback = function(ev)
    vim.b[ev.buf].rick_no_popup = true -- right-clicking the column shows no menu (explorer.lua)
    for _, key in ipairs({ "q", "<Esc>" }) do
      vim.keymap.set("n", key, function() vim.api.nvim_win_close(0, true) end,
        { buffer = ev.buf, nowait = true, desc = "Close Annotations" })
    end
  end,
})

return M
