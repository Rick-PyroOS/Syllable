-- Auto-save: a file is written a second after you stop changing it (after you
-- leave insert mode, or after a normal-mode edit), when you switch buffers, and
-- when iTerm loses focus. Only real files: not scratch buffers, terminals or
-- read-only files. Auto-saves aren't formatted; only a save you make is
-- (plugins/format.lua).
-- Space ta toggles it.

vim.g.autosave = true
local DELAY = 1000
local timer = assert(vim.uv.new_timer())

local function savable(buf)
  if not vim.api.nvim_buf_is_valid(buf) then return false end
  local bo = vim.bo[buf]
  return bo.modified and bo.buftype == "" and bo.modifiable and not bo.readonly
    and vim.api.nvim_buf_get_name(buf) ~= ""
    and vim.uv.fs_stat(vim.fs.dirname(vim.api.nvim_buf_get_name(buf))) ~= nil   -- folder exists
end

local function save(buf)
  if not vim.g.autosave or not savable(buf) then return end
  if buf == vim.api.nvim_get_current_buf() and vim.fn.mode():find("^[iR]") then return end -- mid-typing
  vim.g.rick_autosaving = true
  vim.api.nvim_buf_call(buf, function() vim.cmd("silent! lockmarks update") end)
  vim.g.rick_autosaving = false
end

local group = vim.api.nvim_create_augroup("rick_autosave", { clear = true })

vim.api.nvim_create_autocmd({ "InsertLeave", "TextChanged" }, {
  group = group,
  callback = function(ev)
    timer:stop()
    timer:start(DELAY, 0, vim.schedule_wrap(function() save(ev.buf) end))
  end,
})

vim.api.nvim_create_autocmd({ "BufLeave", "FocusLost", "VimSuspend" }, {
  group = group,
  nested = true, -- so the save still runs BufWritePre
  callback = function(ev)
    if ev.event == "BufLeave" then return save(ev.buf) end
    for _, b in ipairs(vim.api.nvim_list_bufs()) do save(b) end
  end,
})

vim.keymap.set("n", "<leader>ta", function()
  vim.g.autosave = not vim.g.autosave
  vim.notify("Auto-save: " .. (vim.g.autosave and "on" or "off"))
end, { desc = "Toggle auto-save" })
