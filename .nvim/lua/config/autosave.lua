-- Auto-save, debounced: a file is written once you've stopped changing it for
-- the auto-save delay (500 ms unless you change it), while you type too. Every
-- change, in insert or normal mode, restarts that file's countdown, so nothing
-- is written mid-word; each file keeps its own countdown, so switching files
-- doesn't lose a pending save. It waits while the completion list is open or a
-- snippet is being filled in. Files are also written at once when you switch
-- buffers and when iTerm loses focus. Only real files: not scratch buffers,
-- terminals or read-only files. Auto-saves aren't formatted; only a save you
-- make is (plugins/format.lua).
-- Space ta toggles it. :AutoSaveDelay [ms] or Settings (Cmd+,) sets the delay
-- (prefs.lua, remembered across restarts).

vim.g.autosave = true
local prefs = require("config.prefs")
local timers = {} -- buffer -> its countdown (uv timer)

local function savable(buf)
  if not vim.api.nvim_buf_is_valid(buf) then return false end
  local bo = vim.bo[buf]
  return bo.modified and bo.buftype == "" and bo.modifiable and not bo.readonly
    and vim.api.nvim_buf_get_name(buf) ~= ""
    and vim.uv.fs_stat(vim.fs.dirname(vim.api.nvim_buf_get_name(buf))) ~= nil   -- folder exists
end

-- The completion list is open or a snippet is being filled in: writing now
-- would save a half-accepted item, so wait for the next pause.
local function busy(buf)
  if buf ~= vim.api.nvim_get_current_buf() or not vim.fn.mode():find("^[iRs]") then return false end
  local ok, cmp = pcall(require, "blink.cmp")
  if ok then
    local f = cmp.is_menu_visible or cmp.is_visible
    if f and f() then return true end
  end
  return vim.snippet ~= nil and vim.snippet.active() == true
end

local function cancel(buf)
  local t = timers[buf]
  if t then t:stop() end
end

local function save(buf)
  cancel(buf)
  if not vim.g.autosave or not savable(buf) then return end
  vim.g.rick_autosaving = true
  pcall(vim.api.nvim_buf_call, buf, function() vim.cmd("silent! lockmarks update") end)
  vim.g.rick_autosaving = false
end

-- (Re)start buf's countdown: it saves once nothing has changed for the delay.
local function debounce(buf)
  if not vim.g.autosave or not savable(buf) then return cancel(buf) end
  local t = timers[buf]
  if not t then
    t = assert(vim.uv.new_timer())
    timers[buf] = t
  end
  t:stop()
  t:start(math.max(100, prefs.get("autosave_delay")), 0, vim.schedule_wrap(function()
    if busy(buf) then return debounce(buf) end
    save(buf)
  end))
end

local group = vim.api.nvim_create_augroup("rick_autosave", { clear = true })

vim.api.nvim_create_autocmd({ "TextChanged", "TextChangedI", "TextChangedP", "InsertLeave" }, {
  group = group,
  callback = function(ev) debounce(ev.buf) end,
})

vim.api.nvim_create_autocmd({ "BufLeave", "FocusLost", "VimSuspend" }, {
  group = group,
  nested = true, -- so the save still runs BufWritePre
  callback = function(ev)
    if ev.event == "BufLeave" then return save(ev.buf) end
    for _, b in ipairs(vim.api.nvim_list_bufs()) do save(b) end
  end,
})

vim.api.nvim_create_autocmd({ "BufWipeout", "BufWritePost" }, {
  group = group,
  callback = function(ev)
    local t = timers[ev.buf]
    if not t then return end
    if ev.event == "BufWritePost" then return t:stop() end -- saved by hand: nothing pending
    timers[ev.buf] = nil
    t:stop()
    t:close()
  end,
})

vim.keymap.set("n", "<leader>ta", function()
  vim.g.autosave = not vim.g.autosave
  if not vim.g.autosave then
    for b in pairs(timers) do cancel(b) end
  end
  vim.notify("Auto-save: " .. (vim.g.autosave and "on" or "off"))
end, { desc = "Toggle auto-save" })
