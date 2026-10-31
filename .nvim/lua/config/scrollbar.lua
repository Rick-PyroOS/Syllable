-- Scrollbars: a thin bar at the right edge of each file window, of the
-- project tree and of popups you can scroll (K's docs, the error popup)
-- whenever there's more than fits, and one along the bottom of files and the
-- tree when lines are wider than the window (word wrap off; parameter-name
-- hints count). Both are the same thickness. Drag either to scroll.

local M = {}

local bars = {} -- window -> { win = thumb float, buf = its buffer }
local hbars = {} -- the same for the bottom (horizontal) thumbs

local function picker_for(win)
  if vim.bo[vim.api.nvim_win_get_buf(win)].filetype ~= "snacks_picker_list" then return end
  for _, p in ipairs(Snacks.picker.get()) do
    if p.list and p.list.win.win == win then return p end
  end
end

-- What's scrollable in win: total rows, first row shown, rows that fit.
-- The project tree draws only the rows on screen, so ask it rather than the
-- buffer. With word wrap on, a long line takes several rows.
local function extent(win)
  local p = picker_for(win)
  if p then return p.list:count(), p.list.top, p.list.state.height, p end
  local buf = vim.api.nvim_win_get_buf(win)
  local top, height = vim.fn.line("w0", win), vim.api.nvim_win_get_height(win)
  if vim.wo[win].wrap then
    local above = top > 1 and vim.api.nvim_win_text_height(win, { end_row = top - 2 }).all or 0
    return vim.api.nvim_win_text_height(win, {}).all, above + 1, height
  end
  return vim.api.nvim_buf_line_count(buf), top, height
end

local thumb_wins = {} -- our own floats, so they don't get scrollbars of their own

-- Popups that take the cursor when clicked (K's docs, the error popup), but
-- not the completion menu and its docs, which have scrollbars of their own.
local function scrollable_popup(win, cfg)
  if cfg.focusable == false or thumb_wins[win] then return false end
  return not vim.bo[vim.api.nvim_win_get_buf(win)].filetype:find("^blink%-cmp")
end

-- File windows, the project tree's list (a float inside its panel) and popups.
local function wanted(win)
  if not vim.api.nvim_win_is_valid(win) or vim.api.nvim_win_get_width(win) <= 2 then return false end
  local cfg = vim.api.nvim_win_get_config(win)
  return cfg.relative == "" or picker_for(win) ~= nil or scrollable_popup(win, cfg)
end

-- Sideways bars only on files and the tree. Not terminals: their lines are
-- never wider than the window, and measuring thousands of lines of output
-- every time it scrolls slowed down typing.
local function wanted_sideways(win)
  if vim.bo[vim.api.nvim_win_get_buf(win)].buftype == "terminal" then return false end
  return vim.api.nvim_win_get_config(win).relative == "" or picker_for(win) ~= nil
end

local function hide(win, list)
  list = list or bars
  local b = list[win]
  if b then thumb_wins[b.win] = nil end
  if b and vim.api.nvim_win_is_valid(b.win) then pcall(vim.api.nvim_win_close, b.win, true) end
  list[win] = nil
end

-- A thumb float over win: reuse list[win]'s or open one.
local function place(list, win, cfg, text, hl)
  local b = list[win]
  if b and vim.api.nvim_win_is_valid(b.win) then
    vim.api.nvim_win_set_config(b.win, cfg)
  else
    b = { buf = vim.api.nvim_create_buf(false, true) }
    local under = vim.api.nvim_win_get_config(win).zindex
    cfg.style, cfg.focusable, cfg.mouse, cfg.noautocmd = "minimal", false, true, true
    cfg.border = "none" -- not winborder's
    cfg.zindex = under and under + 1 or 40 -- just above its window, below popups
    b.win = vim.api.nvim_open_win(b.buf, false, cfg)
    thumb_wins[b.win] = true
    vim.wo[b.win].winhighlight = "Normal:" .. hl
    list[win] = b
  end
  vim.api.nvim_buf_set_lines(b.buf, 0, -1, false, text)
end

local function draw(win)
  local total, top, height = extent(win)
  if total <= height or height < 2 then return hide(win) end
  local size = math.max(1, math.floor(height * height / total + 0.5))
  local row = math.floor((top - 1) * (height - size) / math.max(1, total - height) + 0.5)
  row = math.max(0, math.min(row, height - size))
  local cfg = { relative = "win", win = win, row = row, col = vim.api.nvim_win_get_width(win) - 1,
                width = 1, height = size }
  local lines = {}
  for i = 1, size do lines[i] = "▎" end -- a quarter of the column: as thick as the bottom bar's ▁
  place(bars, win, cfg, lines, "RickScrollbarThumb")
end

-- Widest line shown in win, in screen cells, counting parameter-name hints
-- and other text drawn inside lines. Cached until the text changes; hints
-- arrive later without changing the text, so it's also redone after a second.
local widths = {} -- buf -> { tick, time, width }
local function widest(win)
  local buf = vim.api.nvim_win_get_buf(win)
  local tick, now = vim.b[buf].changedtick, vim.uv.now()
  local c = widths[buf]
  if c and c.tick == tick and now - c.time < 1000 then return c.width end
  local w = vim.api.nvim_win_call(win, function()
    return vim.fn.max(vim.fn.map(vim.fn.range(1, vim.fn.line("$")), 'virtcol([v:val, "$"]) - 1'))
  end)
  widths[buf] = { tick = tick, time = now, width = w }
  return w
end

-- Sideways: widest line, first column shown, columns that fit, where the text starts.
local function hextent(win)
  local info = vim.fn.getwininfo(win)[1]
  local textoff = info and info.textoff or 0
  local width = vim.api.nvim_win_get_width(win) - textoff - 1 -- the last column is the vertical bar's
  local leftcol = vim.api.nvim_win_call(win, function() return vim.fn.winsaveview().leftcol end)
  return widest(win), leftcol, width, textoff
end

-- The bottom bar covers the window's last row: when the cursor gets there
-- (arrowing down to the end of a file), scroll one line so it stays in view.
local function keep_cursor_clear(win)
  local b = hbars[win]
  if not (b and b.win and vim.api.nvim_win_is_valid(b.win)) or win ~= vim.api.nvim_get_current_win() then return end
  if vim.fn.winline() < vim.api.nvim_win_get_height(win) then return end
  vim.fn.winrestview({ topline = vim.fn.line("w0") + 1 })
end

local function hdraw(win)
  if vim.wo[win].wrap or not wanted_sideways(win) then return hide(win, hbars) end
  local total, left, width, textoff = hextent(win)
  local height = vim.api.nvim_win_get_height(win)
  if total <= width + left and left == 0 or width < 4 or height < 2 then return hide(win, hbars) end
  total = math.max(total, left + width)
  local size = math.max(2, math.floor(width * width / total + 0.5))
  local col = math.floor(left * (width - size) / math.max(1, total - width) + 0.5)
  col = math.max(0, math.min(col, width - size))
  local cfg = { relative = "win", win = win, row = height - 1, col = textoff + col, width = size, height = 1 }
  place(hbars, win, cfg, { string.rep("▁", size) }, "RickScrollbarThumb")
  keep_cursor_clear(win)
end

local function update_all()
  local thumbs = {}
  for _, list in ipairs({ bars, hbars }) do
    for _, b in pairs(list) do thumbs[b.win] = true end
  end
  for _, list in ipairs({ bars, hbars }) do
    for win in pairs(list) do
      if not wanted(win) then hide(win, list) end
    end
  end
  for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if not thumbs[win] and wanted(win) then
      pcall(draw, win)
      pcall(hdraw, win)
    end
  end
end

local pending = false
function M.update()
  if pending then return end
  pending = true
  vim.schedule(function()
    pending = false
    update_all()
  end)
end

---------------------------------------------------------------------------
-- Dragging the thumb

local drag -- { win, grab (row or column inside the thumb), size, sideways }
local LEFT_MOUSE, LEFT_DRAG, LEFT_RELEASE = vim.keycode("<LeftMouse>"), vim.keycode("<LeftDrag>"), vim.keycode("<LeftRelease>")

local function owner_of(thumb)
  for win, b in pairs(bars) do
    if b.win == thumb then return win, false end
  end
  for win, b in pairs(hbars) do
    if b.win == thumb then return win, true end
  end
end

local function drag_sideways(screencol)
  local win = drag.win
  if not vim.api.nvim_win_is_valid(win) then return end
  local total, _, width, textoff = hextent(win)
  local win_left = vim.api.nvim_win_get_position(win)[2] -- 0-based
  local col = screencol - 1 - win_left - textoff - drag.grab
  local frac = math.max(0, math.min(1, col / math.max(1, width - drag.size)))
  local leftcol = math.floor(frac * math.max(0, total - width) + 0.5)
  local tree = picker_for(win) ~= nil
  vim.api.nvim_win_call(win, function()
    -- The cursor has to stay on screen, or Neovim scrolls back to it. In a
    -- file, put it on the nearest line long enough to reach the new first
    -- column; the tree's cursor may sit past the end of its row.
    local view = vim.fn.winsaveview()
    local lnum = view.lnum
    if tree then
      vim.wo.virtualedit = "all"
    else
      local top, bot = vim.fn.line("w0"), vim.fn.line("w$")
      local function reaches(l) return vim.fn.strdisplaywidth(vim.fn.getline(l)) > leftcol end
      if not reaches(lnum) then
        for d = 1, bot - top do
          if lnum - d >= top and reaches(lnum - d) then lnum = lnum - d break end
          if lnum + d <= bot and reaches(lnum + d) then lnum = lnum + d break end
        end
      end
      vim.api.nvim_win_set_cursor(0, { lnum, 0 })
    end
    vim.cmd("normal! " .. (leftcol + 1) .. "|")
    vim.fn.winrestview({ topline = view.topline, leftcol = leftcol })
  end)
  M.update()
end

local function drag_to(screenrow)
  local win = drag.win
  if not vim.api.nvim_win_is_valid(win) then return end
  local total, _, height, p = extent(win)
  local win_top = vim.api.nvim_win_get_position(win)[1] -- 0-based
  local row = screenrow - 1 - win_top - drag.grab
  local frac = math.max(0, math.min(1, row / math.max(1, height - drag.size)))
  local top = 1 + math.floor(frac * (total - height) + 0.5)
  if p then
    p.list:scroll(top, true)
  elseif vim.wo[win].wrap then
    -- Rows, not lines: find the line that starts at that row.
    vim.api.nvim_win_call(win, function()
      local function rows_above(l) return l > 1 and vim.api.nvim_win_text_height(win, { end_row = l - 2 }).all or 0 end
      local lo, hi = 1, vim.api.nvim_buf_line_count(0)
      while lo < hi do
        local mid = math.floor((lo + hi + 1) / 2)
        if rows_above(mid) <= top - 1 then lo = mid else hi = mid - 1 end
      end
      local view, so = vim.fn.winsaveview(), vim.wo.scrolloff
      if so < 0 then so = vim.o.scrolloff end
      so = math.min(so, math.floor((height - 1) / 2))
      local lnum = view.lnum
      if lnum < lo + so or vim.api.nvim_win_text_height(win, { start_row = lo - 1, end_row = lnum - 1 }).all > height - so then
        lnum = math.min(lo + so, vim.api.nvim_buf_line_count(0))
      end
      vim.fn.winrestview({ topline = lo, lnum = lnum })
    end)
  else
    vim.api.nvim_win_call(win, function()
      -- The cursor has to stay on screen, or Neovim scrolls back to it.
      local view, so = vim.fn.winsaveview(), vim.wo.scrolloff
      if so < 0 then so = vim.o.scrolloff end
      so = math.min(so, math.floor((height - 1) / 2))
      local lnum = math.max(top + so, math.min(view.lnum, top + height - 1 - so))
      vim.fn.winrestview({ topline = top, lnum = math.min(lnum, total) })
    end)
  end
  M.update()
end

local function on_key(k, typed)
  k = typed and typed ~= "" and typed or k
  if k == LEFT_MOUSE then
    local m = vim.fn.getmousepos()
    local win, sideways = owner_of(m.winid)
    if not win then return end
    if sideways then
      drag = { win = win, grab = m.wincol - 1, size = vim.api.nvim_win_get_width(m.winid), sideways = true }
    else
      drag = { win = win, grab = m.winrow - 1, size = vim.api.nvim_win_get_height(m.winid) }
    end
    return ""
  elseif drag and k == LEFT_DRAG then
    local m = vim.fn.getmousepos()
    vim.schedule(function()
      if not drag then return end
      if drag.sideways then drag_sideways(m.screencol) else drag_to(m.screenrow) end
    end)
    return ""
  elseif drag and k == LEFT_RELEASE then
    drag = nil
    return ""
  end
end

function M.setup()
  -- Both bars are thin lines (▎ and ▁) in the color of the menus' scrollbar.
  local function hcolor()
    local thumb = vim.api.nvim_get_hl(0, { name = "PmenuThumb", link = false })
    vim.api.nvim_set_hl(0, "RickScrollbarThumb", { fg = thumb.bg, default = true })
  end
  hcolor()
  vim.api.nvim_create_autocmd("ColorScheme", { group = vim.api.nvim_create_augroup("rick_scrollbar_hl", { clear = true }), callback = hcolor })
  local group = vim.api.nvim_create_augroup("rick_scrollbar", { clear = true })
  vim.api.nvim_create_autocmd({ "WinScrolled", "WinResized", "WinNew", "WinClosed", "BufWinEnter",
                                "TextChanged", "TextChangedI", "VimResized", "TabEnter", "OptionSet",
                                "CursorHold" }, {
    group = group,
    callback = function(ev)
      if ev.event == "WinClosed" then
        hide(tonumber(ev.match))
        hide(tonumber(ev.match), hbars)
      end
      M.update()
    end,
  })
  vim.api.nvim_create_autocmd({ "CursorMoved", "CursorMovedI" }, {
    group = group,
    callback = function() keep_cursor_clear(vim.api.nvim_get_current_win()) end,
  })
  -- The project tree scrolls by redrawing its rows, not by scrolling the window.
  local List = require("snacks.picker.core.list")
  local render = List.render
  List.render = function(self, ...)
    local ret = render(self, ...)
    M.update()
    return ret
  end
  vim.on_key(on_key, vim.api.nvim_create_namespace("rick_scrollbar"))
  M.update()
end

return M
