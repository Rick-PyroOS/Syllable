-- Project explorer extras (the tree itself is set up in plugins/snacks.lua):
--   right-click a file or folder for a menu of the usual file actions
--   (Show Problems lists a file's errors);
--   errors show on files only, not on the folders above them;
--   Space te turns off remembering which folders were open, so each time the
--   tree opens it starts collapsed;
--   the tree follows the file you're editing: switch tabs and it opens the
--   folders down to that file and highlights it (CLion's "Always Select
--   Opened File"); Space tl turns that off and on (remembered);
--   collapsing a folder also collapses the folders inside it;
--   folders get ▸ / ▾ arrows, and the tree remembers the width you drag it to;
--   no cursor in the tree: the highlighted row shows where you are.

local M = {}

local state_file = vim.fn.stdpath("state") .. "/explorer.json"

local function read_state()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(state_file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end

local state = read_state()
M.remember = state.remember ~= false
M.follow = state.follow_file ~= false -- on unless Space tl turned it off
M.width = tonumber(state.width) or 40

local function save_state()
  pcall(vim.fn.mkdir, vim.fs.dirname(state_file), "p")
  pcall(vim.fn.writefile, { vim.json.encode({ remember = M.remember, follow_file = M.follow, width = M.width }) }, state_file)
end

local function picker()
  return Snacks.picker.get({ source = "explorer" })[1]
end

-- The tree row under the mouse: picker, item, mouse position.
local function row_under_mouse(m)
  local p = picker()
  local win = p and p.list.win.win
  if not win or m.winid ~= win or (m.line or 0) == 0 then return end
  return p, p.list:get(p.list:row2idx(m.winrow)), m
end

---------------------------------------------------------------------------
-- Problems in a file or folder

-- Open a picker with every problem under a file or folder.
local function problems_picker(item)
  local path, is_dir = item.file, item.dir
  require("config.diagnostics").picker({
    filter = {
      filter = function(it)
        local f = it.file and vim.fs.normalize(vim.fn.fnamemodify(it.file, ":p"))
        return f and (f == path or (is_dir and vim.startswith(f, path .. "/")))
      end,
    },
  })
end

---------------------------------------------------------------------------
-- Right-click menu

local function reveal(item)
  local path = item.file
  if vim.fn.has("mac") == 1 then
    vim.system({ "open", "-R", path })
  else
    vim.ui.open(item.dir and path or vim.fs.dirname(path))
  end
end

function M.copy_path(path, relative)
  if relative then path = vim.fs.relpath(vim.fn.getcwd(), path) or path end
  vim.fn.setreg("+", path)
  vim.notify("Copied " .. path)
end

-- Select the row under the mouse and define its menu; returns the keys that show it.
local function context_menu(p, m)
  local item
  vim.api.nvim_set_current_win(m.winid)
  p.list:move(p.list:row2idx(m.winrow), true)
  pcall(vim.api.nvim_win_set_cursor, m.winid, { vim.api.nvim_win_get_cursor(m.winid)[1], m.column - 1 })
  item = p:current() or {}
  local function act(name) return function() p:action(name) end end
  -- A file opens here or in a split to the right of the code; a folder only
  -- opens and closes.
  local entries = {
    { item.dir and (item.open and "Collapse" or "Expand") or "Open", act("confirm") },
    { "-" },
    { "New File or Folder…", act("explorer_add") }, -- a name ending in / makes a folder
    { "Rename…", act("explorer_rename") },
    { "Delete", act("explorer_del") },
    { "Duplicate…", act("explorer_copy") },
    { "-" },
    { "Copy Path", function() M.copy_path(item.file) end },
    { "Copy Relative Path", function() M.copy_path(item.file, true) end },
    { vim.fn.has("mac") == 1 and "Reveal in Finder" or "Open Containing Folder", function() reveal(item) end },
    { "Find in Folder…", function()
      require("config.find_in_files").open("find", item.dir and item.file or vim.fs.dirname(item.file))
    end },
    { "Terminal Here", act("terminal") },
    { "Local History", function()
      local lh = require("config.local_history")
      if item.dir then lh.show_project(item.file) else lh.show(item.file) end
    end },
  }
  if not item.dir then table.insert(entries, 2, { "Open in Right Split", act("edit_vsplit") }) end
  if item.severity then
    table.insert(entries, { "-" })
    table.insert(entries, { "Show Problems", function() problems_picker(item) end })
  end
  table.insert(entries, { "-" })
  table.insert(entries, { "Collapse All", act("explorer_close_all") })
  return require("config.menu").build("Explorer", entries)
end

-- Right-click in the tree opens our menu; anywhere else, the tab bar's
-- (tabs.lua) or Neovim's usual one (none in a pane whose buffer sets
-- b:rick_no_popup). A mapping in the tree's own buffer only
-- works once the tree has focus, so this one is global.
local function right_click()
  local m = vim.fn.getmousepos()
  -- Some panes (the blame column) want no menu at all.
  if m.winid ~= 0 and vim.api.nvim_win_is_valid(m.winid) and vim.b[vim.api.nvim_win_get_buf(m.winid)].rick_no_popup then
    return ""
  end
  local p = row_under_mouse(m)
  if not p then return "<RightMouse>" end
  M._click = m
  -- :popup (not popup!): the tree is a float, and popup! misplaces the menu
  -- over it; select_clicked() puts the cursor at the mouse instead.
  return "<Cmd>lua require('config.explorer').select_clicked()<CR><Cmd>popup ]RickExplorer<CR>"
end

-- Runs from right_click()'s keys, outside the mapping, so it may move the
-- cursor; the :popup after it then shows the menu.
function M.select_clicked()
  local p, _, m = row_under_mouse(M._click or {})
  if p then context_menu(p, m) end
end

---------------------------------------------------------------------------
-- Remembering open folders

-- Called when the explorer closes.
function M.on_close(p)
  if not M.remember then require("snacks.explorer.tree"):close_all(p:cwd()) end
end

function M.toggle_remember()
  M.remember = not M.remember
  save_state()
  vim.notify("Explorer remembers open folders: " .. (M.remember and "on" or "off"))
end

---------------------------------------------------------------------------
-- Following the current file (Space tl)

-- Select the file being edited (the one in the window the tree opened from
-- when the tree itself has focus).
local function reveal_current(p)
  local win = vim.api.nvim_get_current_win()
  if vim.api.nvim_win_get_config(win).relative ~= "" then win = p.main end
  if not (win and vim.api.nvim_win_is_valid(win)) then return end
  local buf = vim.api.nvim_win_get_buf(win)
  local file = vim.api.nvim_buf_get_name(buf)
  if file == "" or vim.bo[buf].buftype ~= "" then return end
  file = vim.fs.normalize(file)
  local item = p:current()
  if item and item.file == file then return end
  require("snacks.explorer.actions").update(p, { target = file })
end

function M.toggle_follow()
  M.follow = not M.follow
  save_state()
  local p = picker()
  if M.follow and p and not p:is_focused() then reveal_current(p) end
  vim.notify("Project tree follows the current file: " .. (M.follow and "on" or "off"))
end

---------------------------------------------------------------------------
-- Width, arrows, collapsing

-- The tree's layout (plugins/snacks.lua), at the width it was last dragged to.
function M.layout()
  return { preset = "sidebar", preview = false, layout = { width = M.width, min_width = 15 } }
end

-- When the tree opens.
function M.on_show(p)
  if M.follow then
    vim.schedule(function()
      if not p.closed then reveal_current(p) end
    end)
  end
end

-- Snacks' file row with an arrow in front of each folder (files get a gap
-- so their names line up with the folders').
function M.format(item, p)
  if item.dir then item.severity = nil end -- errors show on files, not rolled up to folders
  local ret = require("snacks.picker.format").file(item, p)
  local at = 1
  for i, part in ipairs(ret) do
    if part[2] == "SnacksPickerTree" then at = i + 1 end
  end
  local arrow = item.dir and (item.open and "▾ " or "▸ ") or "  "
  table.insert(ret, at, { arrow, "RickExplorerArrow" })
  return ret
end

-- Collapsing a folder collapses everything inside it too, so opening it
-- again shows one level, not the subfolders that were open before.
local function patch_tree()
  local Tree = require("snacks.explorer.tree")
  if Tree.rick_close then return end
  Tree.rick_close = true
  local close = Tree.close
  function Tree:close(path)
    close(self, path)
    local node = self:find(self:dir(path))
    if node then
      self:walk(node, function(n)
        if n.dir then
          n.open = false
        end
      end)
    end
  end
end

---------------------------------------------------------------------------
-- No cursor in the tree

local cursor = {} -- saved = 'guicursor' while the tree has focus, busy = while snapping it back

local function in_tree()
  local p = picker()
  return p and p.list and p.list.win.win == vim.api.nvim_get_current_win()
end

-- Hide the cursor while the tree has focus (not in the command line).
local function hide_cursor()
  if in_tree() then
    -- The tree's cursor may sit past the end of short rows, so scrolling it
    -- sideways keeps working.
    vim.wo.virtualedit = "all"
    if not cursor.saved then
      cursor.saved = vim.o.guicursor
      vim.o.guicursor = cursor.saved .. ",n-v-sm:block-RickCursorHidden"
    end
  elseif cursor.saved then
    vim.o.guicursor = cursor.saved
    cursor.saved = nil
  end
end

-- Keep the hidden cursor at the start of what's shown of its row, so Space,
-- l or a click can't move it along the row (or scroll the tree sideways).
local function pin_cursor()
  -- (Not while a right-click menu opens: :popup shows it at the cursor.)
  if cursor.busy or require("config.menu").swallow or not in_tree() then return end
  local view = vim.fn.winsaveview()
  local at = vim.fn.virtcol(".", 1)
  if at[1] <= view.leftcol + 1 and view.leftcol + 1 <= at[2] then return end
  cursor.busy = true
  vim.cmd("normal! " .. (view.leftcol + 1) .. "|")
  vim.fn.winrestview({ topline = view.topline, leftcol = view.leftcol })
  cursor.busy = false
end

function M.setup()
  patch_tree()
  local group = vim.api.nvim_create_augroup("rick_explorer", { clear = true })
  -- Tree lines are LineNr by default, which is too faint to see in tokyonight.
  local function colors()
    vim.api.nvim_set_hl(0, "SnacksPickerTree", { link = "Comment" })
    vim.api.nvim_set_hl(0, "RickExplorerArrow", { link = "Comment", default = true })
    vim.api.nvim_set_hl(0, "RickCursorHidden", { blend = 100, nocombine = true })
  end
  colors()
  vim.api.nvim_create_autocmd("ColorScheme", { group = group, callback = colors })
  vim.api.nvim_create_autocmd("BufEnter", {
    group = group,
    callback = function()
      if not M.follow then return end
      vim.schedule(function()
        local p = picker()
        if p and not p.closed and p:on_current_tab() and not p:is_focused() then reveal_current(p) end
      end)
    end,
  })
  vim.api.nvim_create_autocmd({ "WinEnter", "BufEnter", "WinLeave" }, {
    group = group,
    callback = function()
      vim.schedule(function()
        hide_cursor()
        pin_cursor()
      end)
    end,
  })
  vim.api.nvim_create_autocmd("CursorMoved", { group = group, callback = pin_cursor })
  -- Remember the width the tree is dragged to.
  vim.api.nvim_create_autocmd("WinResized", {
    group = group,
    callback = function()
      local p = picker()
      local root = p and p.layout and p.layout.root and p.layout.root.win
      if not (root and vim.api.nvim_win_is_valid(root)) then return end
      if not vim.tbl_contains(vim.v.event.windows or {}, root) then return end
      local width = vim.api.nvim_win_get_width(root)
      if width ~= M.width and width >= 15 then
        M.width = width
        save_state()
      end
    end,
  })
  vim.keymap.set({ "n", "i", "v" }, "<RightMouse>", right_click, { expr = true })
end

return M
