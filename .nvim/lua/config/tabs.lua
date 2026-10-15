-- Extras for the tabs across the top (plugins/bufferline.lua) and the tab rows
-- of the other splits (config/split_tabs.lua):
--   rest the mouse on a tab to see the file's full path, with a copy button
--   ( Copy) after it: click the button to copy the path;
--   scrolling over the tabs (the wheel up/down or sideways) scrolls them, without
--   switching files;
--   switching files leaves the tabs where they are, unless the file's tab is
--   out of view (then they move just enough to show it);
--   closing a tab pulls the rest back to fill the row (none left hidden);
--   right-click a tab for Close, Close Others, Copy Path and the like.

local M = {}

local function bl_state() return require("bufferline.state") end

-- Tabs in the order shown, and the ones on screen now.
local function all_tabs() return bl_state().components or {} end
local function shown_tabs() return bl_state().visible_components or {} end

local function index_of(list, id)
  for i, it in ipairs(list) do
    if it.id == id then return i end
  end
end

---------------------------------------------------------------------------
-- Scrolling. Bufferline lays the tabs out around the current one, so they
-- shift whenever you switch; here they're always laid out from tab M.start.

M.start = 1

local function refresh() require("bufferline.ui").refresh() end

local function scroll(step)
  local tabs, shown = all_tabs(), shown_tabs()
  if #tabs == 0 or #shown == 0 then return end
  local first = index_of(tabs, shown[1].id) or 1
  if step > 0 and shown[#shown].id == tabs[#tabs].id then return end -- the last one is already in view
  M.start = math.max(1, math.min(#tabs, first + step))
  refresh()
end

-- Move the tabs only as far as needed to show buf's tab.
local function show_tab(buf)
  local tabs = all_tabs()
  local i = index_of(tabs, buf)
  if not i then return end
  M.start = math.max(1, math.min(M.start, #tabs))
  if i < M.start then
    M.start = i
    return refresh()
  end
  for _ = 1, #tabs do
    if index_of(shown_tabs(), buf) or M.start >= i then return end
    M.start = M.start + 1
    vim.cmd.redrawtabline() -- lays the tabs out again now, updating what's shown
  end
end

-- After a tab closes, pull the tabs back so none hide off the left while
-- there's empty room on the right.
local function settle()
  vim.cmd.redrawtabline()
  local tabs = all_tabs()
  M.start = math.max(1, math.min(M.start, #tabs))
  while M.start > 1 do
    local shown = shown_tabs()
    if #shown == 0 or shown[#shown].id ~= tabs[#tabs].id then return end -- the row is full
    M.start = M.start - 1
    vim.cmd.redrawtabline()
    shown = shown_tabs()
    if #shown == 0 or shown[#shown].id ~= tabs[#tabs].id then -- went one too far
      M.start = M.start + 1
      return vim.cmd.redrawtabline()
    end
  end
end

-- Wrap bufferline's layout so it starts from M.start. It decides what fits
-- around whichever tab says current() in get_sections(); for that one call,
-- the tab at M.start answers instead (drawing still highlights the real one),
-- and the tabs before it are left out.
local function patch_bufferline()
  local ui = require("bufferline.ui")
  if ui.rick_scroll then return end
  ui.rick_scroll = true
  local layout = ui.tabline
  ui.tabline = function(items, ...)
    local shown = {}
    for _, it in ipairs(items) do
      if not it.hidden then shown[#shown + 1] = it end
    end
    local start = math.max(1, math.min(M.start, #shown))
    local anchor = shown[start]
    local touched = {}
    for i, it in ipairs(shown) do
      local real = it.current
      touched[#touched + 1] = it
      it.current = function(self)
        if debug.getinfo(2, "n").name == "get_sections" then return self == anchor end
        return real(self)
      end
      if anchor and i < start then it.hidden = true end
    end
    local ok, res = pcall(layout, items, ...)
    for _, it in ipairs(touched) do
      it.current = nil
      if it.hidden then it.hidden = nil end
    end
    if not ok then error(res) end
    return res
  end
end

---------------------------------------------------------------------------
-- Hovering: the full path

local hover = { timer = vim.uv.new_timer() } -- win, path, id, row; copy = the icon's columns in the tip
local MOUSE_MOVE, LEFT_MOUSE, LEFT_RELEASE = vim.keycode("<MouseMove>"), vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
local drop_release = false

local function close_tip()
  if hover.win and vim.api.nvim_win_is_valid(hover.win) then pcall(vim.api.nvim_win_close, hover.win, true) end
  hover.win, hover.path, hover.copy = nil, nil, nil
end

-- The tab under screen column col (1-based), from what bufferline drew last.
local function tab_at(col)
  local ok, state = pcall(bl_state)
  if not ok or not state.visible_components then return end
  local pos = state.left_offset_size or 0
  for _, item in ipairs(state.visible_components) do
    local next_pos = pos + (item.length or 0)
    if col > pos and col <= next_pos then return item end
    pos = next_pos
  end
end

-- The path, then the copy icon (the same one the other popups use).
local function show_tip(item, col, row)
  local path = item.path and item.path ~= "" and item.path or nil
  if not path then return end
  close_tip()
  -- The same copy button as the other popups (config/copy_button.lua).
  local glyph = require("config.scanners").glyphs().copy
  local icon = glyph ~= "" and glyph .. " Copy" or "Copy" -- letters instead of icons: no Nerd Font
  local before = " " .. vim.fn.fnamemodify(path, ":~") .. "  "
  local text = before .. icon .. " "
  local buf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, { text })
  vim.hl.range(buf, vim.api.nvim_create_namespace("rick_tab_tip"), "RickTabTipCopy", { 0, #before }, { 0, #before + #icon })
  local width = vim.fn.strdisplaywidth(text)
  local from = vim.fn.strdisplaywidth(before) + 1
  hover.copy = { from, from + vim.fn.strdisplaywidth(icon) - 1 }
  hover.path = path
  hover.win = vim.api.nvim_open_win(buf, false, {
    relative = "editor", row = row or 1, col = math.max(0, math.min(col - 1, vim.o.columns - width - 2)),
    width = width, height = 1, style = "minimal", border = "rounded", focusable = false, mouse = true, zindex = 200,
  })
end

-- The tab under the mouse: bufferline's (top row), or one of a split's own.
local function tab_under(m)
  local item = m.screenrow == 1 and vim.o.showtabline > 0 and tab_at(m.screencol) or nil
  if item then return item, 1 end
  item = require("config.split_tabs").tab_at(m.screenrow, m.screencol)
  if item then return item, m.screenrow end
end

local function on_mouse_move()
  local m = vim.fn.getmousepos()
  if hover.win and m.winid == hover.win then return end -- on the tip itself
  local item, row = tab_under(m)
  local id = item and (item.id .. ":" .. (item.win or 0))
  if id == hover.id then return end
  hover.id = id
  hover.timer:stop()
  -- Leaving the tab for the tip below it keeps the tip.
  if hover.win and hover.row and m.screenrow > hover.row and m.screenrow <= hover.row + 2 and not item then return end
  close_tip()
  if not item then return end
  hover.timer:start(require("config.prefs").get("hover_delay"), 0, vim.schedule_wrap(function()
    if hover.id == id then
      show_tip(item, m.screencol, row)
      hover.row = row
    end
  end))
end

---------------------------------------------------------------------------
-- Right-click menu

-- bufnr's tab menu; win is the split whose tab row it's in (nil: the top row).
function M.menu(bufnr, win)
  close_tip()
  local split_tabs = require("config.split_tabs")
  local function close(ids)
    for _, id in ipairs(ids) do
      if vim.api.nvim_buf_is_valid(id) then split_tabs.close(id, win) end
    end
  end
  local tabs = all_tabs()
  if win and win ~= split_tabs.main() then
    tabs = vim.tbl_map(function(b) return { id = b } end, split_tabs.tabs(win))
  end
  local i = index_of(tabs, bufnr) or 0
  local path = vim.api.nvim_buf_get_name(bufnr)
  local function ids(from, to)
    local out = {}
    for j = from, to do
      if tabs[j] and tabs[j].id ~= bufnr then out[#out + 1] = tabs[j].id end
    end
    return out
  end
  local explorer = require("config.explorer")
  local entries = {
    { "Close", function() close({ bufnr }) end },
    { "Close Other Tabs", function() close(ids(1, #tabs)) end },
    { "Close Tabs to the Left", function() close(ids(1, i - 1)) end },
    { "Close Tabs to the Right", function() close(ids(i + 1, #tabs)) end },
    { "Close All Tabs", function() close(vim.list_extend(ids(1, #tabs), { bufnr })) end },
    { "-" },
    { "Open to the Right", function() vim.cmd("vertical sbuffer " .. bufnr) end },
  }
  if path ~= "" then
    vim.list_extend(entries, {
      { "-" },
      { "Copy Path", function() explorer.copy_path(path) end },
      { "Copy Relative Path", function() explorer.copy_path(path, true) end },
      { "Show in Project Tree", function() Snacks.explorer.reveal({ file = path }) end },
      { vim.fn.has("mac") == 1 and "Reveal in Finder" or "Open Containing Folder", function()
        if vim.fn.has("mac") == 1 then vim.system({ "open", "-R", path }) else vim.ui.open(vim.fs.dirname(path)) end
      end },
    })
  end
  require("config.menu").open("Tab", entries)
end

---------------------------------------------------------------------------

-- Scrolling over the tab row (up/down or sideways) scrolls the tabs. Elsewhere it scrolls as usual.
local function wheel(key, step)
  return function()
    local m = vim.fn.getmousepos()
    local split_win = require("config.split_tabs").row_at(m.screenrow, m.screencol)
    if split_win then
      vim.schedule(function() require("config.split_tabs").scroll(split_win, step) end)
      return ""
    end
    if m.screenrow == 1 and vim.o.showtabline > 0 then
      vim.schedule(function() scroll(step) end)
      return ""
    end
    return key
  end
end

local function tip_hl()
  vim.api.nvim_set_hl(0, "RickTabTipCopy", { link = "Special" })
end

function M.setup()
  patch_bufferline()
  tip_hl()
  vim.o.mousemoveevent = true
  vim.on_key(function(k, typed)
    k = typed and typed ~= "" and typed or k -- what was typed, before any mapping
    if k == MOUSE_MOVE then
      vim.schedule(on_mouse_move)
    elseif k == LEFT_MOUSE and hover.win and vim.fn.getmousepos().winid == hover.win then
      -- On the tip: the copy icon copies the path; the rest of it does nothing.
      local m, path, copy = vim.fn.getmousepos(), hover.path, hover.copy
      local c = m.wincol - 1 -- wincol counts the tip's border
      if copy and c >= copy[1] and c <= copy[2] then
        vim.schedule(function()
          close_tip()
          require("config.explorer").copy_path(path)
        end)
      end
      drop_release = true
      return ""
    elseif k == LEFT_RELEASE and drop_release then
      drop_release = false
      return ""
    elseif hover.win then
      vim.schedule(function() -- any other key or click hides it (resting on the tab again brings it back)
        close_tip()
        hover.id = nil
      end)
    end
  end, vim.api.nvim_create_namespace("rick_tab_hover"))
  for _, mode in ipairs({ "n", "i", "v" }) do
    for key, step in pairs({ ["<ScrollWheelLeft>"] = -1, ["<ScrollWheelRight>"] = 1, ["<ScrollWheelUp>"] = -1, ["<ScrollWheelDown>"] = 1 }) do
      vim.keymap.set(mode, key, wheel(key, step), { expr = true })
    end
  end
  vim.api.nvim_create_augroup("rick_tabs", { clear = true })
  vim.api.nvim_create_autocmd("ColorScheme", { group = "rick_tabs", callback = tip_hl })
  -- Switching to a tab that's scrolled out of view brings it into view.
  vim.api.nvim_create_autocmd("BufEnter", {
    group = "rick_tabs",
    callback = function(ev)
      vim.schedule(function() show_tab(ev.buf) end)
    end,
  })
  -- Closing a tab: the tabs after it move left, and any hidden off the left
  -- come back into the room it leaves.
  vim.api.nvim_create_autocmd({ "BufDelete", "BufWipeout" }, {
    group = "rick_tabs",
    callback = function(ev)
      local i = index_of(all_tabs(), ev.buf)
      if i and i < M.start then M.start = M.start - 1 end
      vim.schedule(settle)
    end,
  })
end

return M
