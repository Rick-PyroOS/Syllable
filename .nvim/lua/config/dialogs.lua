-- One dialog at a time, like CLion. Find (Shift Shift), Find Action and the
-- other Cmd-key pickers go through open(): pressing a dialog's key while it's
-- already open just puts you back in it, and opening a different one closes
-- the first. Hover popups (quick docs, the margin) close when a dialog opens
-- and stay away while one is open, so they never sit on top of it.

local M = {}

local current = {} -- dialog name -> its snacks picker

-- Open pickers, not counting the project tree.
local function pickers()
  if not Snacks or not Snacks.picker then return {} end
  return vim.tbl_filter(function(p) return p.opts.source ~= "explorer" end, Snacks.picker.get({ tab = false }))
end

function M.busy() return #pickers() > 0 end

local function close_hovers()
  pcall(function() require("config.quick_docs").close() end)
  pcall(function() require("config.diagnostics").close_popup() end)
end

-- open(name, fn): fn opens the picker and returns it.
function M.open(name, fn)
  close_hovers()
  local mine = current[name]
  if mine and not mine.closed then
    mine:focus("input")
    return mine
  end
  for _, p in ipairs(pickers()) do p:close() end
  current[name] = fn()
  return current[name]
end

-- Pickers opened some other way (Space keys) close the hover popups too.
vim.api.nvim_create_autocmd("FileType", {
  group = vim.api.nvim_create_augroup("rick_dialogs", { clear = true }),
  pattern = "snacks_picker_input",
  callback = close_hovers,
})

return M
