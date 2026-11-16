-- Right-click menus (the project tree's in explorer.lua, the tabs' in tabs.lua).
-- In iTerm, right-clicks only reach Neovim with Settings > Pointer >
-- "Right click reported to apps, does not open menu" turned on.

local M = {}

local menus = {} -- name -> { { label, fn } | { "-" } }

-- Define menu name; returns the command that shows it at the mouse.
-- items: { { label, fn }, { "-" }, ... }.
function M.build(name, items)
  menus[name] = items
  local path = "]Rick" .. name
  pcall(vim.cmd, "aunmenu " .. path)
  local sep = 0
  for i, it in ipairs(items) do
    local label
    if it[1] == "-" then
      sep = sep + 1
      label = "-sep" .. sep .. "-"
    else
      label = it[1]:gsub("([%s%.\\|])", "\\%1")
    end
    vim.cmd(("anoremenu <silent> %s.%s <Cmd>lua require('config.menu').run(%q, %d)<CR>"):format(path, label, name, i))
  end
  M.swallow = true
  -- Run :popup from typed keys, not from Lua: then the chosen entry runs
  -- after the menu has closed rather than inside it (where a second menu
  -- would fail). popup! opens it at the mouse.
  return "<Cmd>popup! " .. path .. "<CR>"
end

-- Show a menu at the mouse, from a click handler (the tabs'). The tree's
-- right-click mapping returns the :popup keys itself; fed from there, they
-- didn't open the menu.
function M.open(name, items)
  vim.api.nvim_feedkeys(vim.keycode(M.build(name, items)), "n", false)
end

function M.run(name, i)
  local it = menus[name] and menus[name][i]
  if it and it[2] then it[2]() end
end

local LEFT_MOUSE, LEFT_RELEASE = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
-- Keys that come and go while the menu opens and runs: the :popup command
-- and the chosen entry are <Cmd> keys (their text isn't passed on), and the
-- mouse moves on the way to an entry.
local KEEP = {}
for _, key in ipairs({ "<LeftMouse>", "<LeftDrag>", "<RightRelease>", "<MouseMove>", "<Cmd>" }) do
  KEEP[vim.keycode(key)] = true
end

function M.setup()
  -- The editor's own right-click menu: Delete Line (the clicked line, or the
  -- selected lines) after Select All, in place of "How-to disable mouse".
  -- It deletes without copying, like CLion's Delete Line. Inspect (which
  -- lists highlight groups under the cursor) is dropped.
  pcall(vim.cmd, [[aunmenu PopUp.Inspect]])
  pcall(vim.cmd, [[aunmenu PopUp.How-to\ disable\ mouse]])
  pcall(vim.cmd, [[aunmenu PopUp.-2-]])
  vim.cmd([[nnoremenu PopUp.Delete\ Line <Cmd>normal! "_dd<CR>]])
  vim.cmd([[xnoremenu PopUp.Delete\ Lines :<C-u>'<,'>delete _<CR>]])
  vim.cmd([[nnoremenu PopUp.Type\ Hierarchy <Cmd>lua require("config.hierarchy").open()<CR>]])
  vim.cmd([[anoremenu PopUp.Compare\ with\ Clipboard <Cmd>lua require("config.diff").compare_clipboard()<CR>]])
  vim.cmd([[nnoremenu PopUp.Local\ History <Cmd>lua require("config.local_history").show()<CR>]])
  vim.cmd([[nnoremenu .501 PopUp.Annotate\ with\ Git\ Blame <Cmd>lua require("config.blame").toggle()<CR>]])
  -- While the blame column is open, the same entry reads "Close Annotations".
  -- It is put back on every right-click, so it carries its own priority (.501)
  -- to stay last but one: Which Threads Run This? (.503) still comes after it.
  vim.api.nvim_create_autocmd("MenuPopup", {
    group = vim.api.nvim_create_augroup("rick_popup_blame", { clear = true }),
    callback = function()
      local open = require("config.blame").win() ~= nil
      pcall(vim.cmd, [[nunmenu PopUp.Annotate\ with\ Git\ Blame]])
      pcall(vim.cmd, [[nunmenu PopUp.Close\ Annotations]])
      vim.cmd(("nnoremenu .501 PopUp.%s <Cmd>lua require('config.blame').toggle()<CR>")
        :format(open and [[Close\ Annotations]] or [[Annotate\ with\ Git\ Blame]]))
    end,
  })
  -- Neovim's diagnostics entries, as popups: the line's problems, the Problems
  -- list, and the checks you can turn on and off (not the help page).
  vim.cmd([[anoremenu PopUp.Show\ Diagnostics <Cmd>lua require("config.diagnostics").show_cursor(true)<CR>]])
  vim.cmd([[anoremenu PopUp.Show\ All\ Diagnostics <Cmd>lua require("config.diagnostics").picker()<CR>]])
  vim.cmd([[anoremenu PopUp.Configure\ Diagnostics <Cmd>lua require("config.diagnostics").configure()<CR>]])
  -- Neovim hides Configure Diagnostics when the file has no problems, which
  -- is exactly when you'd want it to turn a check back on: keep it available.
  vim.api.nvim_create_autocmd("MenuPopup", {
    group = vim.api.nvim_create_augroup("rick_popup_diagnostics", { clear = true }),
    callback = function() pcall(vim.cmd, [[anoremenu enable PopUp.Configure\ Diagnostics]]) end,
  })
  -- Clicking a menu entry uses the press; drop its release, or the window
  -- under the menu would take it as a click of its own.
  vim.on_key(function(k, typed)
    if not M.swallow then return end
    k = typed and typed ~= "" and typed or k
    if k == LEFT_RELEASE then
      M.swallow = false
      return ""
    elseif not KEEP[k] then
      M.swallow = false
    end
  end, vim.api.nvim_create_namespace("rick_menu"))
end

return M
