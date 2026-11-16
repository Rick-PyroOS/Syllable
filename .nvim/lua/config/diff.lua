-- Diff viewer and merge tool, CLion style. Any tab showing a diff gets:
--   * a toolbar across the top (in place of the tabs while you're there):
--     ◀ Prev  Next ▶ | Accept Left  Accept Right  [Accept Both] | All Left  All Right | Close
--   * arrows in the margin at each change: » takes the left version, « the right one
--     (in a merge they sit at each conflict in the Result pane: « Yours, » Theirs)
--   * F7 / Shift+F7 for the next / previous change (conflict, in a merge)
-- Accept Left / Right act on the change (or conflict) under the cursor; All
-- Left / All Right on the whole file. Read-only sides (HEAD, a branch) are
-- never written: those buttons are dimmed.
--
-- Where diffs come from: git (Space gd this file vs HEAD; diffview.nvim:
-- Space gD every changed file, Space gc compare with a branch, Space gm the
-- merge tool for conflicts: Yours | Result | Theirs), Local History (Space gh),
-- Compare with Clipboard (Space dc) and Compare with File (Space df).
-- One file against another version (Space gd, Local History, Compare with
-- Clipboard / File) opens as a popup over the code: q, Esc on the read-only
-- side, ✕ Close, or a click outside it closes it. The diffview ones open in a
-- tab of their own, which q or ✕ Close closes.

local M = {}
local api = vim.api

-- Diff windows of a tab, left to right (the compare popup's, while it's open).
local function diff_wins(tab)
  local wins = vim.tbl_filter(function(w)
    return vim.wo[w].diff and (api.nvim_win_get_config(w).relative == "" or vim.w[w].rick_diff_popup)
  end, api.nvim_tabpage_list_wins(tab or 0))
  table.sort(wins, function(a, b) return api.nvim_win_get_position(a)[2] < api.nvim_win_get_position(b)[2] end)
  return wins
end

local function dv_view()
  local ok, lib = pcall(require, "diffview.lib")
  return ok and lib.get_current_view() or nil
end

-- The diffview merge view (Yours | Result | Theirs), if that's this tab.
local function merge_view()
  local v = dv_view()
  if v and v.cur_layout and v.cur_layout.name and v.cur_layout.name:find("^diff[34]") then return v end
end

local function editable(win)
  local b = api.nvim_win_get_buf(win)
  return vim.bo[b].modifiable and not vim.bo[b].readonly and not api.nvim_buf_get_name(b):match("^diffview://")
end

local last_win = {} -- tab -> the diff window you were last in
local function target_win()
  local tab = api.nvim_get_current_tabpage()
  local cur = api.nvim_get_current_win()
  if vim.wo[cur].diff then return cur end
  local w = last_win[tab]
  if w and api.nvim_win_is_valid(w) then return w end
  return diff_wins(tab)[1]
end

-- Conflicts in the Result buffer (merge view), cached per change.
local conflict_cache = {}
local function conflicts(buf)
  local tick = api.nvim_buf_get_changedtick(buf)
  local c = conflict_cache[buf]
  if c and c.tick == tick then return c.list end
  local ok, vcs = pcall(require, "diffview.vcs.utils")
  local list = ok and vcs.parse_conflicts(api.nvim_buf_get_lines(buf, 0, -1, false)) or {}
  conflict_cache[buf] = { tick = tick, list = list }
  return list
end

local function result_win(view)
  local main = view.cur_layout:get_main_win()
  return main and main.id
end

-- Replace the conflict around line `lnum` of the Result buffer.
local function resolve(buf, lnum, target)
  for _, c in ipairs(conflicts(buf)) do
    if lnum >= c.first and lnum <= c.last then
      local content = target == "ours" and (c.ours.content or {})
        or target == "theirs" and (c.theirs.content or {})
        or vim.list_extend(vim.deepcopy(c.ours.content or {}), c.theirs.content or {})
      api.nvim_buf_set_lines(buf, c.first - 1, c.last, false, content)
      return true
    end
  end
  return false
end

-- The :diffget / :diffput range for the change at line l of the current
-- window. Lines that only exist on the other side (shown here as a dashed
-- filler) need the lines around them in the range, or Vim does nothing.
local function range_at(l)
  if vim.fn.diff_hlID(l, 1) ~= 0 then return tostring(l) end
  if vim.fn.diff_filler(l) > 0 then return (l - 1) .. "," .. l end
  if l == vim.fn.line("$") and vim.fn.diff_filler(l + 1) > 0 then return l .. "," .. (l + 1) end
  return tostring(l)
end

-- Two-way: make the change under the cursor match `from`'s version.
local function take(from, to)
  local win = target_win()
  if not editable(to) then return vim.notify("That side is read-only", vim.log.levels.WARN) end
  local frombuf, tobuf = api.nvim_win_get_buf(from), api.nvim_win_get_buf(to)
  api.nvim_win_call(win, function()
    local r = range_at(vim.fn.line("."))
    if win == to then vim.cmd(r .. "diffget " .. frombuf) else vim.cmd(r .. "diffput " .. tobuf) end
  end)
end

local function take_all(from, to)
  if not editable(to) then return vim.notify("That side is read-only", vim.log.levels.WARN) end
  api.nvim_buf_set_lines(api.nvim_win_get_buf(to), 0, -1, false, api.nvim_buf_get_lines(api.nvim_win_get_buf(from), 0, -1, false))
end

M.actions = {
  prev = function()
    if merge_view() then return require("diffview.actions").prev_conflict() end
    api.nvim_win_call(target_win(), function() pcall(vim.cmd, "normal! [c") end)
  end,
  next = function()
    if merge_view() then return require("diffview.actions").next_conflict() end
    api.nvim_win_call(target_win(), function() pcall(vim.cmd, "normal! ]c") end)
  end,
  left = function() M.accept("ours") end,
  right = function() M.accept("theirs") end,
  both = function() M.accept("both") end,
  all_left = function() M.accept_all("ours") end,
  all_right = function() M.accept_all("theirs") end,
  close = function()
    if M.popup then return M.close_popup() end
    if dv_view() then return vim.cmd("DiffviewClose") end
    if #api.nvim_list_tabpages() > 1 then vim.cmd("tabclose") else vim.cmd("diffoff!") end
  end,
}

function M.accept(side)
  local view = merge_view()
  if view then
    local rw = result_win(view)
    local buf = api.nvim_win_get_buf(rw)
    if not resolve(buf, api.nvim_win_get_cursor(rw)[1], side) then
      vim.notify("Put the cursor on a conflict in the Result pane (F7 jumps to the next one)")
    end
    return
  end
  local wins = diff_wins()
  if #wins < 2 or side == "both" then return end
  local l, r = wins[1], wins[#wins]
  if side == "ours" then take(l, r) else take(r, l) end
end

function M.accept_all(side)
  local view = merge_view()
  if view then return require("diffview.actions").conflict_choose_all(side)() end
  local wins = diff_wins()
  if #wins < 2 then return end
  local l, r = wins[1], wins[#wins]
  if side == "ours" then take_all(l, r) else take_all(r, l) end
end

-- Margin arrows -------------------------------------------------------------

-- First lines of each change in a two-way diff window (cached per window).
local starts = {}
local function hunk_starts(win)
  local buf = api.nvim_win_get_buf(win)
  local key = api.nvim_buf_get_changedtick(buf) .. ":" .. vim.o.diffopt
  local c = starts[win]
  if c and c.key == key and c.other == M._other_tick then return c.set end
  local set = {}
  api.nvim_win_call(win, function()
    local prev = false
    local n = api.nvim_buf_line_count(buf)
    for l = 1, n do
      local here = vim.fn.diff_hlID(l, 1) ~= 0
      if (here and not prev) or vim.fn.diff_filler(l) > 0 then set[l] = true end
      prev = here
    end
    if vim.fn.diff_filler(n + 1) > 0 then set[n] = true end -- lines the other side adds at the end
  end)
  starts[win] = { key = key, set = set, other = M._other_tick }
  return set
end
M._other_tick = 0
vim.api.nvim_create_autocmd({ "TextChanged", "TextChangedI", "BufWritePost", "DiffUpdated" }, {
  group = vim.api.nvim_create_augroup("rick_diff_ticks", { clear = true }),
  callback = function() M._other_tick = M._other_tick + 1 end,
})

function _G.RickDiffStc()
  local win, lnum = vim.g.statusline_winid, vim.v.lnum
  local base = "%C%s%=%l "
  if vim.v.virtnum ~= 0 then return "%C%s%= " .. "  " end
  local view = merge_view()
  if view then
    local rw = result_win(view)
    if win == rw then
      for _, c in ipairs(conflicts(api.nvim_win_get_buf(rw))) do
        if c.first == lnum then
          return base .. "%#RickDiffArrow#%@v:lua.RickDiffOurs@«%X%@v:lua.RickDiffTheirs@»%X%* "
        end
      end
    end
    return base .. "   "
  end
  local wins = diff_wins(api.nvim_win_get_tabpage(win))
  if #wins < 2 or not hunk_starts(win)[lnum] then return base .. "  " end
  if win == wins[1] and editable(wins[#wins]) then return base .. "%#RickDiffArrow#%@v:lua.RickDiffPush@»%X%* " end
  if win == wins[#wins] and editable(wins[1]) then return base .. "%#RickDiffArrow#%@v:lua.RickDiffPush@«%X%* " end
  return base .. "  "
end

-- Unchanged stretches fold away (click the ▸ or zo to open one).
function _G.RickDiffFold()
  local n = vim.v.foldend - vim.v.foldstart + 1
  return ("  ··· %d unchanged line%s ···"):format(n, n == 1 and "" or "s")
end

local function clicked()
  local m = vim.fn.getmousepos()
  return m.winid, m.line
end

-- » / «: send this change to the other side.
function _G.RickDiffPush()
  local win, line = clicked()
  local wins = diff_wins(api.nvim_win_get_tabpage(win))
  local other = win == wins[1] and wins[#wins] or wins[1]
  if not editable(other) then return vim.notify("That side is read-only", vim.log.levels.WARN) end
  api.nvim_win_call(win, function()
    api.nvim_win_set_cursor(win, { line, 0 })
    vim.cmd(range_at(line) .. "diffput " .. api.nvim_win_get_buf(other))
  end)
end

local function pick_conflict(target)
  local win, line = clicked()
  resolve(api.nvim_win_get_buf(win), line, target)
end
function _G.RickDiffOurs() pick_conflict("ours") end
function _G.RickDiffTheirs() pick_conflict("theirs") end

-- The clickable %@ regions in 'statuscolumn' don't get the click here (the
-- config's own mouse handlers see it first), so the arrows are hit-tested on
-- the raw click instead: the arrow sits just left of the code.
local CLICK, RELEASE = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
local ate_click = false
vim.on_key(function(k)
  if k ~= CLICK and k ~= RELEASE then return end
  if k == RELEASE then
    if ate_click then ate_click = false return "" end
    return
  end
  local m = vim.fn.getmousepos()
  if m.winid == 0 or not api.nvim_win_is_valid(m.winid) or not vim.wo[m.winid].diff
    or vim.wo[m.winid].statuscolumn ~= "%!v:lua.RickDiffStc()" or m.line == 0 then return end
  local textoff = vim.fn.getwininfo(m.winid)[1].textoff
  if m.wincol > textoff or m.wincol < textoff - 2 then return end
  local view = merge_view()
  local fn
  if view then
    if m.winid ~= result_win(view) then return end
    for _, c in ipairs(conflicts(api.nvim_win_get_buf(m.winid))) do
      if c.first == m.line then
        local target = m.wincol == textoff - 2 and "ours" or "theirs"
        fn = function() resolve(api.nvim_win_get_buf(m.winid), m.line, target) end
      end
    end
  elseif hunk_starts(m.winid)[m.line] then
    fn = _G.RickDiffPush
  end
  if not fn then return end
  ate_click = true
  vim.schedule(fn)
  return ""
end, api.nvim_create_namespace("rick_diff_click"))

-- Toolbar (the tabline while a diff tab is current) --------------------------

local BUTTONS = {
  { "prev", " ◀ Prev " }, { "next", " Next ▶ " }, { "|" },
  { "left", " Accept Left " }, { "right", " Accept Right " }, { "both", " Accept Both ", merge = true }, { "|" },
  { "all_left", " All Left " }, { "all_right", " All Right " }, { "|" },
  { "close", " ✕ Close " },
}

local function is_diff_tab() return #diff_wins(0) >= 2 end

local function toolbar()
  local wins = diff_wins(0)
  local merge = merge_view() ~= nil
  local left_ok = merge or (wins[1] and editable(wins[1]))
  local right_ok = merge or (wins[#wins] and editable(wins[#wins]))
  local out = { "%#RickDiffBar# " }
  for i, b in ipairs(BUTTONS) do
    if b[1] == "|" then
      out[#out + 1] = "%#RickDiffBar# │ "
    elseif not b.merge or merge then
      -- "Accept Left" writes into the right side, so it needs that side editable.
      local ok = true
      if b[1] == "left" or b[1] == "all_left" then ok = right_ok end
      if b[1] == "right" or b[1] == "all_right" then ok = left_ok end
      out[#out + 1] = ("%%#%s#%%%d@v:lua.RickDiffButton@%s%%X%%#RickDiffBar# "):format(ok and "RickDiffButton" or "RickDiffButtonOff", i, b[2])
    end
  end
  local what = merge and "Yours | Result | Theirs" or "Diff"
  out[#out + 1] = "%<%=%#RickDiffBar#" .. what .. "   F7 / Shift+F7 "
  return table.concat(out)
end

function _G.RickDiffButton(minwid)
  local b = BUTTONS[minwid]
  if b and M.actions[b[1]] then vim.schedule(M.actions[b[1]]) end
end

local orig_tabline
function _G.RickTabline()
  if is_diff_tab() then return toolbar() end
  if orig_tabline and orig_tabline:sub(1, 2) == "%!" then return vim.api.nvim_eval(orig_tabline:sub(3)) end
  return orig_tabline or ""
end

local function install_tabline()
  if vim.o.tabline ~= "%!v:lua.RickTabline()" then
    orig_tabline = vim.o.tabline
    vim.o.tabline = "%!v:lua.RickTabline()"
  end
end

-- Attach to every diff window: the arrow column and the labels.
local function attach(win)
  if not api.nvim_win_is_valid(win) or not vim.wo[win].diff then return end
  vim.wo[win].statuscolumn = "%!v:lua.RickDiffStc()"
  vim.wo[win].foldcolumn = "1"
  vim.wo[win].foldtext = "v:lua.RickDiffFold()"
  vim.wo[win].signcolumn = "no" -- no checker or git marks: the colours already show the changes
  install_tabline()
  if vim.o.showtabline == 0 then vim.o.showtabline = 2 end
end

local function detach(win)
  if api.nvim_win_is_valid(win) and vim.wo[win].statuscolumn == "%!v:lua.RickDiffStc()" then
    vim.wo[win].statuscolumn = ""
    vim.wo[win].foldcolumn = "1"
    vim.wo[win].foldtext = ""
    vim.wo[win].signcolumn = "yes:3"
  end
end

-- Compare with Clipboard / File ----------------------------------------------

local function scratch(lines, ft, name)
  local b = api.nvim_create_buf(false, true)
  api.nvim_buf_set_lines(b, 0, -1, false, lines)
  vim.bo[b].bufhidden = "wipe"
  vim.bo[b].modifiable = false -- the clipboard / an old version / a selection copy: changes go into the file side
  pcall(api.nvim_buf_set_name, b, name)
  pcall(vim.treesitter.start, b, vim.treesitter.language.get_lang(ft) or ft)
  return b
end

-- The compare popup: { wins = { left, right }, prev = the window it came from, maps = { {buf, key}, ... } }.
M.popup = nil

function M.close_popup()
  local p = M.popup
  M.popup = nil
  if not p then return end
  for _, m in ipairs(p.maps) do
    if api.nvim_buf_is_valid(m[1]) then pcall(vim.keymap.del, "n", m[2], { buffer = m[1] }) end
  end
  -- Back to the window it came from (not a click outside, which already left).
  local inside = vim.tbl_contains(p.wins, api.nvim_get_current_win())
  if inside and p.prev and api.nvim_win_is_valid(p.prev) then pcall(api.nvim_set_current_win, p.prev) end
  for _, w in ipairs(p.wins) do
    if api.nvim_win_is_valid(w) then pcall(api.nvim_win_close, w, true) end
  end
  vim.cmd.redrawtabline()
end

-- Opens a popup over the code: `left` (lines or a buffer) against `right` (a
-- buffer), side by side, each named in its frame.
function M.compare(left, right, left_label, right_label)
  M.close_popup()
  local ft = vim.bo[right].filetype
  local lbuf = type(left) == "number" and left or scratch(left, ft, left_label)
  local prev = api.nvim_get_current_win()
  -- Below the toolbar (the tab line), down to just above the command line.
  local top = 1
  local height = vim.o.lines - vim.o.cmdheight - top - 3
  local half = math.floor((math.min(vim.o.columns - 2, 260) - 4) / 2) -- each side's width inside its frame
  local col = math.floor((vim.o.columns - (2 * half + 4)) / 2)
  local function open(buf, c, title, enter, footer)
    local w = api.nvim_open_win(buf, enter, {
      relative = "editor", row = top, col = c, width = half, height = height, zindex = 45,
      border = "rounded", title = " " .. title .. " ", title_pos = "center",
      footer = footer, footer_pos = footer and "right" or nil,
    })
    vim.w[w].rick_diff_popup = true
    vim.wo[w].winbar = ""
    vim.wo[w].wrap = false
    return w
  end
  local lwin = open(lbuf, col, left_label, false)
  local rwin = open(right, col + half + 2, right_label, true, " q closes ")
  local p = { wins = { lwin, rwin }, prev = prev, maps = {} }
  M.popup = p
  for _, w in ipairs(p.wins) do
    api.nvim_win_call(w, function() vim.cmd("diffthis") end)
    attach(w)
    local buf = api.nvim_win_get_buf(w)
    -- Esc closes it only from a side you can't edit, where it can't be a
    -- leftover from typing.
    for _, key in ipairs(vim.bo[buf].modifiable and { "q" } or { "q", "<Esc>" }) do
      vim.keymap.set("n", key, M.close_popup, { buffer = buf, nowait = true, desc = "Close the diff" })
      table.insert(p.maps, { buf, key })
    end
    -- Ctrl+W h / l / w move between the two sides (not out to the code below).
    local other = w == lwin and rwin or lwin
    for _, key in ipairs({ "<C-w>h", "<C-w>l", "<C-w>w", "<C-w><C-w>" }) do
      vim.keymap.set("n", key, function() if api.nvim_win_is_valid(other) then api.nvim_set_current_win(other) end end,
        { buffer = buf, desc = "Other side of the diff" })
      table.insert(p.maps, { buf, key })
    end
  end
  vim.cmd.redrawtabline()
end

-- Space gd: this file against its last commit (HEAD).
function M.compare_head()
  local buf = api.nvim_get_current_buf()
  local path = api.nvim_buf_get_name(buf)
  if path == "" or vim.bo[buf].buftype ~= "" then return vim.notify("Not a file") end
  local r = vim.system({ "git", "-C", vim.fs.dirname(path), "show", "HEAD:./" .. vim.fs.basename(path) }, { text = true }):wait()
  if r.code ~= 0 then return vim.notify("This file has no committed version in git", vim.log.levels.WARN) end
  local name = vim.fs.basename(path)
  M.compare(vim.split((r.stdout:gsub("\n$", "")), "\n", { plain = true }), buf, name .. " (HEAD)", name .. " (yours)")
end

function M.compare_clipboard()
  local clip = vim.fn.getreg("+")
  if clip == "" then clip = vim.fn.getreg('"') end
  if clip == "" then return vim.notify("The clipboard is empty") end
  local buf = api.nvim_get_current_buf()
  local mode = vim.fn.mode()
  local right = buf
  local name = vim.fn.fnamemodify(api.nvim_buf_get_name(buf), ":t")
  if mode:match("[vV\22]") then
    local l1, l2 = vim.fn.line("v"), vim.fn.line(".")
    if l1 > l2 then l1, l2 = l2, l1 end
    api.nvim_feedkeys(vim.keycode("<Esc>"), "nx", false)
    right = scratch(api.nvim_buf_get_lines(buf, l1 - 1, l2, false), vim.bo[buf].filetype, "Selection of " .. name)
    -- Changes to the selection side go straight back into the file.
    vim.bo[right].modifiable = true
    local count = l2 - l1 + 1
    api.nvim_buf_attach(right, false, {
      on_lines = function()
        vim.schedule(function()
          if not (api.nvim_buf_is_valid(right) and api.nvim_buf_is_valid(buf)) then return end
          local lines = api.nvim_buf_get_lines(right, 0, -1, false)
          api.nvim_buf_set_lines(buf, l1 - 1, l1 - 1 + count, false, lines)
          count = #lines
        end)
      end,
    })
    name = ("Selection (%s, lines %d-%d)"):format(name, l1, l2)
  end
  M.compare(vim.split(clip:gsub("\n$", ""), "\n", { plain = true }), right, "Clipboard", name)
end

function M.compare_file()
  local buf = api.nvim_get_current_buf()
  Snacks.picker.files({
    title = "Compare with File",
    confirm = function(picker, item)
      picker:close()
      if not item then return end
      vim.schedule(function()
        local other = vim.fn.bufadd(Snacks.picker.util.path(item))
        vim.fn.bufload(other)
        vim.bo[other].buflisted = true
        M.compare(other, buf, vim.fn.fnamemodify(api.nvim_buf_get_name(other), ":~:."),
          vim.fn.fnamemodify(api.nvim_buf_get_name(buf), ":~:."))
      end)
    end,
  })
end

function M.compare_branch()
  Snacks.picker.git_branches({
    title = "Compare with Branch",
    confirm = function(picker, item)
      picker:close()
      if item and item.branch then vim.schedule(function() vim.cmd("DiffviewOpen " .. item.branch) end) end
    end,
  })
end

function M.setup()
  local function hl()
    local set = api.nvim_set_hl
    set(0, "RickDiffBar", { link = "TabLineFill", default = true })
    set(0, "RickDiffButton", { link = "TabLineSel", default = true })
    set(0, "RickDiffButtonOff", { link = "Comment", default = true })
    set(0, "RickDiffArrow", { link = "DiagnosticInfo", default = true })
    set(0, "RickDiffLabel", { link = "Title", default = true })
  end
  hl()
  local group = api.nvim_create_augroup("rick_diff", { clear = true })
  api.nvim_create_autocmd("ColorScheme", { group = group, callback = hl })
  api.nvim_create_autocmd("OptionSet", {
    group = group,
    pattern = "diff",
    callback = function()
      local win = api.nvim_get_current_win()
      if vim.v.option_new == true or vim.v.option_new == "1" or vim.wo[win].diff then attach(win) else detach(win) end
      vim.cmd.redrawtabline()
    end,
  })
  api.nvim_create_autocmd({ "WinEnter", "BufWinEnter" }, {
    group = group,
    callback = function()
      local win = api.nvim_get_current_win()
      if vim.wo[win].diff then
        last_win[api.nvim_get_current_tabpage()] = win
        attach(win)
      end
    end,
  })
  api.nvim_create_autocmd("User", {
    group = group,
    pattern = { "DiffviewDiffBufWinEnter", "DiffviewViewOpened", "DiffviewViewEnter" },
    callback = function()
      vim.schedule(function()
        for _, w in ipairs(diff_wins(0)) do attach(w) end
        vim.cmd.redrawtabline()
      end)
    end,
  })
  api.nvim_create_autocmd("TabEnter", { group = group, callback = function() vim.cmd.redrawtabline() end })
  -- Clicking the code outside the compare popup closes it (other popups,
  -- like a picker or a hint, keep it); so does closing either side.
  api.nvim_create_autocmd("WinEnter", {
    group = group,
    callback = function()
      local cur = api.nvim_get_current_win()
      if M.popup and not vim.tbl_contains(M.popup.wins, cur) and api.nvim_win_get_config(cur).relative == "" then
        M.close_popup()
      end
    end,
  })
  api.nvim_create_autocmd("WinClosed", {
    group = group,
    callback = function(ev)
      if M.popup and vim.tbl_contains(M.popup.wins, tonumber(ev.match)) then vim.schedule(M.close_popup) end
    end,
  })

  -- A file with conflict markers says how to resolve it.
  api.nvim_create_autocmd("BufReadPost", {
    group = group,
    callback = function(ev)
      if vim.bo[ev.buf].buftype ~= "" then return end
      for _, l in ipairs(api.nvim_buf_get_lines(ev.buf, 0, math.min(5000, api.nvim_buf_line_count(ev.buf)), false)) do
        if l:match("^<<<<<<< ") then
          vim.schedule(function() vim.notify("This file has merge conflicts: Space gm opens the merge tool") end)
          return
        end
      end
    end,
  })

  local map = vim.keymap.set
  map("n", "<F7>", function() if vim.wo.diff then M.actions.next() end end, { desc = "Next change (diff)" })
  map("n", "<S-F7>", function() if vim.wo.diff then M.actions.prev() end end, { desc = "Previous change (diff)" })
  map("n", "<leader>gd", M.compare_head, { desc = "Diff this file vs HEAD" })
  map("n", "<leader>gD", "<cmd>DiffviewOpen<cr>", { desc = "Diff all changed files" })
  map("n", "<leader>gc", M.compare_branch, { desc = "Compare with branch" })
  map("n", "<leader>gm", "<cmd>DiffviewOpen<cr>", { desc = "Merge tool (resolve conflicts)" })
  map({ "n", "x" }, "<leader>dc", M.compare_clipboard, { desc = "Compare with clipboard" })
  map("n", "<leader>df", M.compare_file, { desc = "Compare with file" })
end

return M
