-- :q (and :quit, :qa, :wq, :x, Space q) quit all of Neovim from any window,
-- asking first if anything is unsaved: Save all / Discard / Cancel. :q! quits
-- all without asking.
-- To close just one split, use :close or Ctrl-w c.

local function quit_all(force)
  if force then return vim.cmd("qall!") end -- :q! quits without asking
  local named, unnamed = {}, {}
  for _, b in ipairs(vim.api.nvim_list_bufs()) do
    local bo = vim.bo[b]
    if bo.modified and bo.buflisted and bo.buftype == "" then
      local name = vim.api.nvim_buf_get_name(b)
      if name == "" then table.insert(unnamed, b) else table.insert(named, vim.fn.fnamemodify(name, ":~:.")) end
    end
  end
  if #named + #unnamed > 0 then
    local lines = {}
    for _, n in ipairs(named) do table.insert(lines, "  " .. n) end
    if #unnamed > 0 then table.insert(lines, ("  %d unnamed scratch buffer(s), discarded either way"):format(#unnamed)) end
    local choice = vim.fn.confirm("Unsaved changes:\n" .. table.concat(lines, "\n") .. "\n\nSave before quitting?",
      "&Save all\n&Discard\n&Cancel", 1)
    if choice == 1 then
      for _, b in ipairs(vim.api.nvim_list_bufs()) do
        if vim.bo[b].modified and vim.bo[b].buftype == "" and vim.api.nvim_buf_get_name(b) ~= "" then
          local ok, err = pcall(vim.api.nvim_buf_call, b, function() vim.cmd("write") end)
          if not ok then
            vim.notify("Couldn't save " .. vim.api.nvim_buf_get_name(b) .. ": " .. err, vim.log.levels.ERROR)
            return
          end
        end
      end
    elseif choice ~= 2 then
      return
    end
  end
  vim.cmd("qall!")
end

vim.api.nvim_create_user_command("QuitAll", function(o) quit_all(o.bang) end, { bang = true, desc = "Quit Neovim, asking about unsaved files" })

-- Only rewrite the exact command typed alone on the : line (:q! becomes :QuitAll!,
-- which quits without asking).
for lhs, rhs in pairs({ q = "QuitAll", quit = "QuitAll", qa = "QuitAll", qall = "QuitAll",
                        wq = "w <bar> QuitAll", x = "update <bar> QuitAll" }) do
  vim.cmd(("cnoreabbrev <expr> %s (getcmdtype() ==# ':' && getcmdline() ==# '%s') ? '%s' : '%s'"):format(lhs, lhs, rhs, lhs))
end

vim.keymap.set("n", "<leader>q", function() quit_all(false) end, { desc = "Quit Neovim" })
