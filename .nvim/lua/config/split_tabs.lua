-- Tabs per split, like CLion's: when the editor is split (Open to the Right,
-- :vsplit, :split), each split has its own row of tabs with the files opened
-- in it.
--   The first split (top left) keeps the tabs across the top of the screen
--   (bufferline, plugins/bufferline.lua, with the hover path, scrolling and
--   right-click menu from config/tabs.lua). They only stretch as far as that
--   split; the other splits draw their own tabs in the same top row, above
--   themselves. A split lower down draws its tabs in the bar just above it
--   (the status line of the split above). So every split reads, top down:
--   its tabs, its breadcrumbs, its code.
--   Click a tab to show that file in its split; click its ✕ (or middle-click)
--   to close it; right-click for the tab menu. The split's file is the lit tab.
--   Closing a split's last tab closes the split. Closing a split moves its
--   files to the first split's tabs (the ones no other split has open).
--   Shift-h / Shift-l, Space 1..9, Space bd and Space bo work on the tabs of
--   the split you're in.
--   With one editor window nothing changes: one row of tabs across the top.
-- Bars between panes that have no tabs to show are left blank in the dark
-- gutter color, so the panes read as separate frames (see config/options.lua).

local M = {}

local lists = {}  -- editor window -> buffers opened in it, in order
local fresh = {}  -- windows just made by a split, filled in once the split command is done
local starts = {} -- window -> first tab shown, when its row is scrolled
local last = {}   -- window -> { buf = file it showed when last drawn, all = whether the last tab fit }
local bars = {}   -- where tab rows were last drawn, by bar: { rows = { row, from, to, win }, tabs = { row, from, to, win, buf } }

---------------------------------------------------------------------------
-- Which windows have tabs

local function tabbable(buf)
  return buf and vim.api.nvim_buf_is_valid(buf) and vim.bo[buf].buflisted and vim.bo[buf].buftype == ""
end

local function is_editor(win)
  if not win or not vim.api.nvim_win_is_valid(win) then return false end
  if vim.api.nvim_win_get_config(win).relative ~= "" or vim.wo[win].diff then return false end
  local buf = vim.api.nvim_win_get_buf(win)
  return vim.bo[buf].buftype == "" and vim.bo[buf].filetype ~= "snacks_layout_box"
end

local function geometry(win)
  local i = vim.fn.getwininfo(win)[1]
  -- top: the window's first screen row (its breadcrumbs, if it has them); bottom: its status line row
  return { win = win, top = i.winrow, col = i.wincol, width = i.width, bottom = i.winrow + i.winbar + i.height }
end

-- The editor windows of this tab page, top to bottom, left to right.
-- Bufferline asks for every tab on every redraw, so the answer is kept until
-- the windows change or the next turn of the event loop.
local cached
local function forget() cached = nil end
local function editors()
  if cached then return cached end
  local out = {}
  for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if is_editor(win) then out[#out + 1] = geometry(win) end
  end
  table.sort(out, function(a, b)
    if a.top ~= b.top then return a.top < b.top end
    return a.col < b.col
  end)
  cached = out
  vim.schedule(forget)
  return out
end

-- The first split: its tabs are bufferline's.
function M.main()
  local e = editors()
  return e[1] and e[1].win
end

function M.is_split()
  return #editors() > 1
end

local function list(win)
  if not lists[win] then
    lists[win] = {}
    local buf = vim.api.nvim_win_get_buf(win)
    if tabbable(buf) then lists[win][1] = buf end
  end
  return lists[win]
end

local function index_of(t, v)
  for i, x in ipairs(t) do
    if x == v then return i end
  end
end

local function redraw()
  vim.schedule(function()
    pcall(vim.cmd.redrawtabline)
    pcall(vim.cmd, "redrawstatus!")
  end)
end

local function remember(win, buf)
  if fresh[win] or not is_editor(win) or not tabbable(buf) then return end
  local l = list(win)
  if not index_of(l, buf) then
    l[#l + 1] = buf
    redraw()
  end
end

-- Is buf in some editor window's tabs (other than skip's)?
local function in_any_list(buf, skip)
  for _, g in ipairs(editors()) do
    if g.win ~= skip and index_of(list(g.win), buf) then return true end
  end
  return false
end

-- The first split's tab row also takes the files no split has open
-- (bufferline's custom_filter).
function M.main_shows(buf)
  if not M.is_split() then return true end
  local main = M.main()
  return index_of(list(main), buf) ~= nil or not in_any_list(buf, main)
end

-- The tabs of win, in order.
function M.tabs(win)
  if not M.is_split() or win == M.main() then
    local ok, state = pcall(require, "bufferline.state")
    local out = {}
    for _, c in ipairs(ok and state.components or {}) do out[#out + 1] = c.id end
    return out
  end
  local out = {}
  for _, buf in ipairs(list(win)) do
    if tabbable(buf) then out[#out + 1] = buf end
  end
  return out
end

---------------------------------------------------------------------------
-- Closing and switching

local function delete_if_unused(buf)
  if vim.api.nvim_buf_is_valid(buf) and not in_any_list(buf) and #vim.fn.win_findbuf(buf) == 0 then
    Snacks.bufdelete({ buf = buf })
  end
end

-- Close buf's tab in win's row.
function M.close(buf, win)
  win = win or M.main()
  if not M.is_split() or not win then return Snacks.bufdelete(buf) end
  local row = M.tabs(win)
  local i = index_of(row, buf)
  local l = list(win)
  local j = index_of(l, buf)
  if j then table.remove(l, j) end
  if vim.api.nvim_win_is_valid(win) and vim.api.nvim_win_get_buf(win) == buf then
    local next_buf = i and (row[i + 1] or row[i - 1])
    if next_buf and next_buf ~= buf then
      vim.api.nvim_win_call(win, function() vim.cmd.buffer(next_buf) end)
    else
      pcall(vim.api.nvim_win_close, win, false)
    end
  end
  vim.schedule(function()
    delete_if_unused(buf)
    redraw()
  end)
end

-- Close every tab in win's row but keep.
function M.close_others(win, keep)
  for _, buf in ipairs(M.tabs(win)) do
    if buf ~= keep then M.close(buf, win) end
  end
end

-- Show buf in win.
function M.open(win, buf)
  if not vim.api.nvim_win_is_valid(win) or not vim.api.nvim_buf_is_valid(buf) then return end
  vim.api.nvim_set_current_win(win)
  if vim.api.nvim_win_get_buf(win) ~= buf then vim.cmd.buffer(buf) end
end

local function secondary(win)
  return M.is_split() and is_editor(win) and win ~= M.main()
end

-- Shift-h / Shift-l: the next tab of the split you're in.
function M.cycle(step)
  local win = vim.api.nvim_get_current_win()
  if not secondary(win) then return vim.cmd(step > 0 and "BufferLineCycleNext" or "BufferLineCyclePrev") end
  local row = M.tabs(win)
  local i = index_of(row, vim.api.nvim_win_get_buf(win)) or 1
  local buf = row[(i - 1 + step) % #row + 1]
  if buf then M.open(win, buf) end
end

-- Space 1..9: tab n of the split you're in.
function M.go(n)
  local win = vim.api.nvim_get_current_win()
  if not secondary(win) then return vim.cmd("BufferLineGoToBuffer " .. n) end
  local buf = M.tabs(win)[n]
  if buf then M.open(win, buf) end
end

-- Space bd / Space bo
function M.close_current()
  local win = vim.api.nvim_get_current_win()
  local buf = vim.api.nvim_get_current_buf()
  if not M.is_split() or not is_editor(win) then return Snacks.bufdelete(buf) end
  M.close(buf, win)
end

function M.close_current_others()
  local win = vim.api.nvim_get_current_win()
  if not M.is_split() or not is_editor(win) then return Snacks.bufdelete.other() end
  M.close_others(win, vim.api.nvim_get_current_buf())
end

---------------------------------------------------------------------------
-- Drawing a split's tab row

local function hl_bg(name)
  local h = vim.api.nvim_get_hl(0, { name = name, link = false })
  return h.bg
end

-- An icon color on a tab's background.
local icon_groups = {}
local function icon_group(icon_hl, tab_hl)
  local key = (icon_hl or "") .. "_" .. tab_hl
  if icon_groups[key] then return icon_groups[key] end
  local name = "RickSplitTabIcon" .. vim.tbl_count(icon_groups)
  local fg = icon_hl and vim.api.nvim_get_hl(0, { name = icon_hl, link = false }).fg
  vim.api.nvim_set_hl(0, name, { fg = fg, bg = hl_bg(tab_hl) })
  icon_groups[key] = name
  return name
end

local function esc(s) return (s:gsub("%%", "%%%%")) end

-- Click handlers: minwid holds the window and the buffer.
local function click_id(win, buf) return win * 100000 + buf end
local function from_click_id(id) return math.floor(id / 100000), id % 100000 end

_G.RickSplitTabs = _G.RickSplitTabs or {}
function RickSplitTabs.click(id, _, button)
  local win, buf = from_click_id(id)
  vim.schedule(function()
    if button == "m" then return M.close(buf, win) end
    if button == "r" then return require("config.tabs").menu(buf, win) end
    M.open(win, buf)
  end)
end
function RickSplitTabs.close(id, _, button)
  local win, buf = from_click_id(id)
  if button == "l" or button == "m" then vim.schedule(function() M.close(buf, win) end) end
end

-- One tab: { str = drawn text, width = cells, buf = buffer }.
local function tab_cell(win, buf)
  local shown = vim.api.nvim_win_get_buf(win) == buf -- lit, as in the top row
  local kind = shown and "Selected" or ""
  local base = shown and "BufferLineBufferSelected" or "BufferLineBackground"
  local name = vim.fn.fnamemodify(vim.api.nvim_buf_get_name(buf), ":t")
  if name == "" then name = "[No Name]" end
  local ok, devicons = pcall(require, "nvim-web-devicons")
  local icon, icon_hl = "", nil
  if ok then icon, icon_hl = devicons.get_icon(name, vim.fn.fnamemodify(name, ":e"), { default = true }) end
  icon = icon or ""
  local bl = package.loaded["bufferline.config"]
  local close_icon = bl and bl.options and bl.options.buffer_close_icon or "✕"
  local modified = vim.bo[buf].modified
  local mark = modified and "●" or close_icon
  local mark_hl = (modified and "BufferLineModified" or "BufferLineCloseButton") .. kind
  local indicator = shown and "▎" or " "
  local ind_hl = shown and "BufferLineIndicatorSelected" or base
  local id = click_id(win, buf)
  local text = indicator .. " " .. icon .. " " .. name .. " "
  local str = "%" .. id .. "@v:lua.RickSplitTabs.click@"
    .. "%#" .. ind_hl .. "#" .. indicator
    .. "%#" .. base .. "# "
    .. "%#" .. icon_group(icon_hl, base) .. "#" .. esc(icon)
    .. "%#" .. base .. "# " .. esc(name) .. " "
    .. "%X%" .. id .. "@v:lua.RickSplitTabs.close@%#" .. mark_hl .. "#" .. mark .. "%#" .. base .. "# %X"
  local width = vim.fn.strdisplaywidth(text) + vim.fn.strdisplaywidth(mark) + 1
  return { str = str, width = width, buf = buf, close_from = width - 1 }
end

-- win's tab row, width cells wide, starting at screen column col on screen
-- row row (recorded so the hover path and the wheel find the tabs).
local function draw_row(win, width, row, col, hit_list)
  local tabs = M.tabs(win)
  local cells = {}
  for _, buf in ipairs(tabs) do cells[#cells + 1] = tab_cell(win, buf) end
  local shown = vim.api.nvim_win_get_buf(win)
  local cur = index_of(tabs, shown) or 1
  local s = math.max(1, math.min(starts[win] or 1, #cells))
  -- The wheel scrolls the row freely; switching files scrolls it just
  -- enough to show the file's tab.
  if not last[win] or last[win].buf ~= shown then
    if cur < s then s = cur end
    local function fits(from)
      local used = 0
      for k = from, cur do used = used + cells[k].width end
      return used <= width
    end
    while s < cur and not fits(s) do s = s + 1 end
  end
  starts[win] = s
  local out, used = {}, 0
  for k = s, #cells do
    local c = cells[k]
    if used + c.width > width then break end
    out[#out + 1] = c.str
    hit_list[#hit_list + 1] = { row = row, from = col + used, to = col + used + c.width - 1, win = win, buf = c.buf }
    used = used + c.width
    last[win] = { buf = shown, all = k == #cells }
  end
  if #out == 0 then last[win] = { buf = shown, all = true } end
  return table.concat(out) .. "%#RickGutter#" .. string.rep(" ", width - used)
end

-- The bar on screen row row that spans columns [from, from + width): draws
-- the tab rows of the editor windows whose top is the row below it, in the
-- part of the bar above each one; gutter elsewhere.
-- A terminal panel's title, shown in the gutter row above it instead of a
-- row of its own (its winbar is off: snacks.lua).
local function term_title(win, width)
  local buf = vim.api.nvim_win_get_buf(win)
  local t = vim.b[buf].snacks_terminal
  local title = vim.b[buf].term_title or ""
  if vim.bo[buf].filetype == "snacks_terminal" and t and t.id then title = t.id .. ": " .. title end
  title = " " .. vim.fn.strcharpart(title, 0, math.max(0, width - 2)) .. " "
  local w = vim.fn.strdisplaywidth(title)
  return "%#RickGutterTitle#" .. title:gsub("%%", "%%%%") .. "%#RickGutter#" .. string.rep(" ", math.max(0, width - w))
end

local function terminals()
  local out = {}
  for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if vim.api.nvim_win_get_config(win).relative == "" and vim.bo[vim.api.nvim_win_get_buf(win)].buftype == "terminal" then
      local g = geometry(win)
      g.term = true
      out[#out + 1] = g
    end
  end
  return out
end

-- The window whose status line covers screen column col of row.
local function bar_owner(row, col)
  for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if vim.api.nvim_win_get_config(win).relative == "" then
      local g = geometry(win)
      if g.bottom == row and col >= g.col and col < g.col + g.width then return win end
    end
  end
end

local function hidden_box(win)
  return win and vim.bo[vim.api.nvim_win_get_buf(win)].filetype == "snacks_layout_box"
end

local function draw_bar(key, row, from, width, skip)
  local hit_list, rows = {}, {}
  local parts, at = {}, from
  local guests = {}
  for _, g in ipairs(editors()) do
    if g.top == row + 1 and g.win ~= skip and g.col >= from and g.col < from + width then guests[#guests + 1] = g end
  end
  -- A terminal's title goes in the bar over its left edge, or the next one when
  -- that bar belongs to the project tree's placeholder (it's too narrow).
  for _, g in ipairs(terminals()) do
    if g.top == row + 1 and g.col < from + width and g.col + g.width > from then
      local here = g.col >= from
      if here and hidden_box(type(key) == "number" and key or nil) then here = false end
      if not here and g.col < from and hidden_box(bar_owner(row, g.col)) then
        local w = g.col + g.width - from
        here, g = true, { win = g.win, term = true, col = from, width = w, top = g.top }
      end
      if here then guests[#guests + 1] = g end
    end
  end
  table.sort(guests, function(a, b) return a.col < b.col end)
  for _, g in ipairs(guests) do
    if g.col > at then
      parts[#parts + 1] = "%#RickGutter#" .. string.rep(" ", g.col - at)
      at = g.col
    end
    local w = math.min(g.width, from + width - at)
    if w > 0 and g.term then
      parts[#parts + 1] = term_title(g.win, w)
      at = at + w
    elseif w > 0 then
      rows[#rows + 1] = { row = row, from = at, to = at + w - 1, win = g.win }
      parts[#parts + 1] = draw_row(g.win, w, row, at, hit_list)
      at = at + w
    end
  end
  if at < from + width then parts[#parts + 1] = "%#RickGutter#" .. string.rep(" ", from + width - at) end
  bars[key] = { rows = rows, tabs = hit_list }
  return table.concat(parts)
end

-- 'statusline': the bar under a window. Blank gutter, or the tabs of the
-- split below it. With Space tb's global status bar, the usual file and ruler.
function M.statusline()
  if vim.o.laststatus == 3 then return "%<%f %h%m%r%=%-14.(%l,%c%V%) %P" end
  local win = vim.g.statusline_winid or vim.api.nvim_get_current_win()
  if not vim.api.nvim_win_is_valid(win) then return "" end
  local g = geometry(win)
  return draw_bar(win, g.bottom, g.col, g.width)
end

-- The part of the top row right of the first split (added to bufferline's
-- right side): the other top splits' tabs.
local function top_right()
  local e = editors()
  if #e < 2 then return nil end
  local main = e[1]
  local from = main.col + main.width -- the separator column right of the first split
  local width = vim.o.columns - from + 1
  if width <= 0 then return nil end
  return draw_bar("top", main.top - 1, from, width, main.win), width
end

-- With the editor split, the top row's lit tab is the first split's file,
-- not whichever file has the cursor (that may be in another split).
local function patch_bufferline_models()
  local Buffer = require("bufferline.models").Buffer
  if Buffer.rick_split then return end
  Buffer.rick_split = true
  local current, visible = Buffer.current, Buffer.visible
  function Buffer:current()
    local main = M.is_split() and M.main()
    if not main then return current(self) end
    return vim.api.nvim_win_get_buf(main) == self.id
  end
  function Buffer:visible()
    local main = M.is_split() and M.main()
    if not main then return visible(self) end
    return vim.api.nvim_win_get_buf(main) == self.id
  end
end

local function patch_bufferline_offset()
  local offset = require("bufferline.offset")
  if offset.rick_split then return end
  offset.rick_split = true
  local get = offset.get
  offset.get = function(...)
    local res = get(...)
    local ok, str, width = pcall(top_right)
    if ok and str then
      res.right = str .. res.right
      res.right_size = res.right_size + width
      res.total_size = res.total_size + width
    else
      bars.top = nil
    end
    return res
  end
end

-- The split tab under screen position (row, col), for the hover path.
function M.tab_at(row, col)
  for _, bar in pairs(bars) do
    for _, h in ipairs(bar.tabs) do
      if h.row == row and col >= h.from and col <= h.to
        and vim.api.nvim_win_is_valid(h.win) and vim.api.nvim_buf_is_valid(h.buf) then
        return { id = h.buf, win = h.win, path = vim.api.nvim_buf_get_name(h.buf), row = row }
      end
    end
  end
end

-- Is screen position (row, col) on a split's tab row? Returns the window.
function M.row_at(row, col)
  for _, bar in pairs(bars) do
    for _, r in ipairs(bar.rows) do
      if r.row == row and col >= r.from and col <= r.to and vim.api.nvim_win_is_valid(r.win) then return r.win end
    end
  end
end

-- The wheel over a split's tab row scrolls it.
function M.scroll(win, step)
  if step > 0 and last[win] and last[win].all then return end -- the last tab is already in view
  local n = #M.tabs(win)
  starts[win] = math.max(1, math.min(n, (starts[win] or 1) + step))
  redraw()
end

---------------------------------------------------------------------------

local function gutter_hl()
  local fill = vim.api.nvim_get_hl(0, { name = "BufferLineFill", link = false }).bg
    or vim.api.nvim_get_hl(0, { name = "TabLineFill", link = false }).bg
  vim.api.nvim_set_hl(0, "RickGutter", { bg = fill or "#0e0e14" })
  local dim = vim.api.nvim_get_hl(0, { name = "Comment", link = false }).fg
  vim.api.nvim_set_hl(0, "RickGutterTitle", { bg = fill or "#0e0e14", fg = dim })
end

function M.setup()
  gutter_hl()
  vim.o.statusline = "%{%v:lua.require'config.split_tabs'.statusline()%}"
  local group = vim.api.nvim_create_augroup("rick_split_tabs", { clear = true })
  vim.api.nvim_create_autocmd({ "WinNew", "WinClosed", "WinResized", "BufWinEnter", "BufEnter", "TermOpen", "FileType" }, {
    group = group, callback = forget,
  })
  vim.api.nvim_create_autocmd("ColorScheme", { group = group, callback = function()
    icon_groups = {}
    vim.schedule(gutter_hl) -- after bufferline sets its colors
  end })
  vim.api.nvim_create_autocmd({ "BufWinEnter", "BufEnter", "WinEnter" }, {
    group = group,
    callback = function(ev)
      remember(vim.api.nvim_get_current_win(), ev.buf)
    end,
  })
  -- A new split starts with just the file it opens (decided once the split
  -- command is done, so :vsplit file doesn't also take the file it came from).
  vim.api.nvim_create_autocmd("WinNew", {
    group = group,
    callback = function()
      local win = vim.api.nvim_get_current_win()
      lists[win], fresh[win] = nil, true
      vim.schedule(function()
        fresh[win] = nil
        if is_editor(win) then
          lists[win] = nil
          list(win)
          redraw()
        end
      end)
    end,
  })
  -- The layout changed: forget where tab rows were; the redraw puts them back.
  vim.api.nvim_create_autocmd({ "WinResized", "VimResized" }, {
    group = group,
    callback = function()
      bars = {}
      redraw()
    end,
  })
  vim.api.nvim_create_autocmd("WinClosed", {
    group = group,
    callback = function(ev)
      local win = tonumber(ev.match)
      if win then lists[win], fresh[win], starts[win], bars[win], last[win] = nil, nil, nil, nil, nil end
      redraw()
    end,
  })
  vim.api.nvim_create_autocmd({ "BufDelete", "BufWipeout" }, {
    group = group,
    callback = function(ev)
      for _, l in pairs(lists) do
        local i = index_of(l, ev.buf)
        if i then table.remove(l, i) end
      end
    end,
  })
  pcall(patch_bufferline_offset)
  pcall(patch_bufferline_models)
end

return M
