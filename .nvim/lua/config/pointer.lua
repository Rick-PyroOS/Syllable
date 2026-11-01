-- The mouse pointer's shape, like a GUI editor: a text beam only over the
-- text of the pane you're typing in; an arrow everywhere else (the tab bar,
-- the project tree, other panes, the margin, the scrollbars); a sideways (or
-- up/down) resize arrow over the line between two panes you can drag to
-- resize.
--
-- Neovim tells the terminal with the OSC 22 escape code, which iTerm2 has
-- had since its early-2026 builds (kitty, Ghostty, foot and xterm too).
-- Terminals that don't know it ignore it.

local M = {}

local current -- shape last sent

local function send(shape)
  if shape == current then return end
  current = shape
  local seq = "\027]22;" .. shape .. "\027\\"
  if vim.api.nvim_ui_send then
    pcall(vim.api.nvim_ui_send, seq)
  else
    pcall(vim.fn.chansend, vim.v.stderr, seq)
  end
end

local function tabline_shown()
  local stal = vim.o.showtabline
  return stal == 2 or (stal == 1 and #vim.api.nvim_list_tabpages() > 1) or vim.o.tabline ~= ""
end

local function has_border(cfg)
  local b = cfg.border
  if type(b) == "string" then return b ~= "" and b ~= "none" end
  return type(b) == "table" and #b > 0 and b[1] ~= ""
end

-- The window under screen cell (row, col) (1-based), worked out from the
-- layout (getmousepos() doesn't say which side of a divider you're on).
-- Returns the window and where the cell is: "text", "right" (its separator)
-- or "below" (its status line / the line under it).
local function window_at(row, col)
  local floats, splits = {}, {}
  for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    local cfg = vim.api.nvim_win_get_config(win)
    if cfg.relative == "" then
      splits[#splits + 1] = win
    elseif not cfg.hide and cfg.mouse ~= false then
      floats[#floats + 1] = { win = win, z = cfg.zindex or 50, cfg = cfg }
    end
  end
  table.sort(floats, function(a, b) return a.z > b.z end)
  for _, f in ipairs(floats) do
    local pos = vim.api.nvim_win_get_position(f.win) -- 0-based, the border's corner
    local extra = has_border(f.cfg) and 2 or 0
    local h = vim.api.nvim_win_get_height(f.win) + extra
    local w = vim.api.nvim_win_get_width(f.win) + extra
    if row > pos[1] and row <= pos[1] + h and col > pos[2] and col <= pos[2] + w then return f.win, "text", f.cfg end
  end
  for _, win in ipairs(splits) do
    local pos = vim.api.nvim_win_get_position(win)
    local h, w = vim.api.nvim_win_get_height(win), vim.api.nvim_win_get_width(win)
    local top, left = pos[1] + 1, pos[2] + 1
    if row >= top and row < top + h then
      if col >= left and col < left + w then return win, "text" end
      if col == left + w then return win, "right" end
    elseif row == top + h and col >= left and col < left + w then
      return win, "below"
    end
  end
end

-- What's under the mouse decides the shape.
local function shape_at(m)
  if m.screenrow == 1 and tabline_shown() then return "default" end
  if m.screenrow > vim.o.lines - vim.o.cmdheight then return "default" end -- command line
  local win, part, cfg = window_at(m.screenrow, m.screencol)
  if not win then return "default" end
  -- The line right of a pane, or under it, can be dragged to resize it.
  if part == "right" then return "ew-resize" end
  if part == "below" then return "ns-resize" end
  local ft = vim.bo[vim.api.nvim_win_get_buf(win)].filetype
  if ft:find("^snacks_picker") or ft:find("^snacks_layout") or ft == "snacks_notif" then return "default" end
  if cfg and cfg.focusable == false then return "default" end -- scrollbars, tips
  if win ~= vim.api.nvim_get_current_win() then return "default" end -- not the pane you're in
  local info = vim.fn.getwininfo(win)[1]
  if info and m.screencol < info.wincol + info.textoff then return "default" end -- the margin
  return "text"
end

local MOUSE_MOVE, PRESS, RELEASE = vim.keycode("<MouseMove>"), vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
local DRAGS = {}
for _, k in ipairs({ "<LeftDrag>", "<LeftMouse>", "<LeftRelease>", "<ScrollWheelUp>", "<ScrollWheelDown>" }) do
  DRAGS[vim.keycode(k)] = true
end

local dragging = false
local last -- the last mouse position, to redo the shape when focus moves

local function on_key(k, typed)
  k = typed and typed ~= "" and typed or k
  if k ~= MOUSE_MOVE and not DRAGS[k] then return end
  local m = vim.fn.getmousepos()
  last = m
  -- While dragging a separator the pointer keeps its resize arrow.
  if k == PRESS then dragging = current == "ew-resize" or current == "ns-resize" end
  if k == RELEASE then dragging = false end
  if dragging then return end
  vim.schedule(function() send(shape_at(m)) end)
end

function M.setup()
  vim.o.mousemoveevent = true
  vim.on_key(on_key, vim.api.nvim_create_namespace("rick_pointer"))
  -- Clicking into a pane (or Esc out of the tree) changes which pane gets the text beam.
  vim.api.nvim_create_autocmd("WinEnter", {
    group = vim.api.nvim_create_augroup("rick_pointer_focus", { clear = true }),
    callback = function()
      vim.schedule(function()
        if last and not dragging then send(shape_at(last)) end
      end)
    end,
  })
  -- Hand the terminal its own pointer back when leaving or suspending.
  vim.api.nvim_create_autocmd({ "VimLeavePre", "VimSuspend", "FocusLost" }, {
    group = vim.api.nvim_create_augroup("rick_pointer", { clear = true }),
    callback = function()
      current = nil
      send("")
      current = nil
    end,
  })
end

return M
