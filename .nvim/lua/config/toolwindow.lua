-- The bottom tool window, like CLion's: one panel across the bottom with tabs
-- for Problems, Run, Terminal and Notifications. Click a tab to switch; the ⌄ at the
-- right (or Shift+Esc in it) hides the panel. Each tool also has its key,
-- which shows that tab, or hides the panel when it's already showing:
--   Cmd+6 / Space tp   Problems        Cmd+4 / Space rl   Run
--   Cmd+0 / Space ro   Build output    Alt+F12 / Space tt Terminal
--   (the build's output is a view in the Run tab, config/run.lua)
--   Space N            Notifications (config/notifications.lua)
-- Space tW shows / hides the panel with whichever tab was last open.
-- The panel keeps its height when you switch tabs; drag its top edge to resize.
-- The tabs have a row to themselves; under them, a row for what belongs to
-- the open tab (Current File / All Files, the Run buttons and configuration,
-- Clear All, the terminal's title).
--
-- Problems lists the errors and warnings from clangd, cppcheck, semgrep and the
-- local model checks: the current file's, or (press a, or click All Files)
-- every open file's. Enter or a double-click goes to one.
-- The tabs have the editor tabs' colours: the open one lit, the others grey.

local M = {}

local tools, order = {}, {}
local state = { win = nil, head = nil, active = nil, last = "problems", height = nil }

-- A tool: { id, title, buf = function() -> buf or nil, menu = function() ->
-- its right-click entries { { label, fn } | { "-" } }, extra = function() ->
-- the open tab's row under the tabs (statusline format), badge = function() -> text, highlight (after
-- the title: Notifications' unread count), on_show(win) }.
function M.register(def)
  if not tools[def.id] then order[#order + 1] = def.id end
  tools[def.id] = def
end

local function valid() return state.win and vim.api.nvim_win_is_valid(state.win) end

function M.win() return valid() and state.win or nil end
function M.active() return valid() and state.active or nil end
function M.is_showing(id) return valid() and state.active == id end


-- The panel is two windows: a one-line head (the tabs drawn in its line,
-- the open tab's row in its status line) and the tool's window under it.
local hbuf
local hns = vim.api.nvim_create_namespace("rick_toolwindow_head")
local hits = {} -- clickable parts of the tabs line: { from, to (screen cells), id or "hide" }

local function head_valid() return state.head and vim.api.nvim_win_is_valid(state.head) end

_G.RickToolExtra = function()
  local t = tools[state.active]
  local extra = t and t.extra and t.extra() or ""
  return "%#RickToolFill# " .. extra .. "%#RickToolFill#"
end

local function draw_tabs()
  if not (head_valid() and hbuf and vim.api.nvim_buf_is_valid(hbuf)) then return end
  local text, hls = "", {}
  hits = {}
  local function add(str, hl)
    hls[#hls + 1] = { #text, #text + #str, hl }
    text = text .. str
  end
  for _, id in ipairs(order) do
    local hl = id == state.active and "RickToolTabSel" or "RickToolTab"
    local from = vim.fn.strdisplaywidth(text)
    add(" " .. tools[id].title .. " ", hl)
    local badge, bhl = "", nil
    if tools[id].badge then badge, bhl = tools[id].badge() end
    if badge and badge ~= "" then add(badge .. " ", bhl or hl) end
    hits[#hits + 1] = { from, vim.fn.strdisplaywidth(text), id }
  end
  local width = vim.api.nvim_win_get_width(state.head)
  text = text .. string.rep(" ", math.max(1, width - 4 - vim.fn.strdisplaywidth(text)))
  local from = vim.fn.strdisplaywidth(text)
  add(" ⌄ ", "RickToolTab")
  hits[#hits + 1] = { from, vim.fn.strdisplaywidth(text), "hide" }
  vim.bo[hbuf].modifiable = true
  vim.api.nvim_buf_set_lines(hbuf, 0, -1, false, { text })
  vim.bo[hbuf].modifiable = false
  vim.api.nvim_buf_clear_namespace(hbuf, hns, 0, -1)
  for _, h in ipairs(hls) do
    pcall(vim.api.nvim_buf_set_extmark, hbuf, hns, 0, h[1], { end_col = h[2], hl_group = h[3] })
  end
end

function M.refresh()
  if not valid() then return end
  draw_tabs()
  pcall(vim.api.nvim__redraw, { win = state.head, statusline = true })
end

local function open_head()
  if not (hbuf and vim.api.nvim_buf_is_valid(hbuf)) then
    hbuf = vim.api.nvim_create_buf(false, true)
    vim.bo[hbuf].bufhidden = "hide"
    vim.bo[hbuf].filetype = "rick_toolwindow_head"
  end
  state.head = vim.api.nvim_open_win(hbuf, false, { split = "above", win = state.win, height = 1 })
  -- Not fixed: dragging the panel's top edge moves the head's top, and the
  -- rows it gains go on to the panel (WinResized below).
  for opt, v in pairs({ winfixheight = false, number = false, relativenumber = false, signcolumn = "no", foldcolumn = "0",
    statuscolumn = "", list = false, wrap = false, cursorline = false, spell = false, winbar = "",
    statusline = "%{%v:lua.RickToolExtra()%}", fillchars = "eob: ",
    winhighlight = "Normal:RickToolFill,NormalNC:RickToolFill,StatusLine:RickToolFill,StatusLineNC:RickToolFill" }) do
    vim.wo[state.head][opt] = v
  end
  vim.w[state.head].rick_toolwindow = true
end

-- Opened with the tab's buffer in it (so nothing else shows there first).
local function open_window(buf)
  if valid() then
    if not head_valid() then open_head() end
    return state.win
  end
  local h = state.height or math.max(6, math.floor(vim.o.lines * 0.3) - 2)
  state.win = vim.api.nvim_open_win(buf, false, { split = "below", win = -1, height = h + 2 })
  vim.wo[state.win].winfixheight = true
  vim.wo[state.win].winbar = ""
  vim.w[state.win].rick_toolwindow = true
  open_head()
  pcall(vim.api.nvim_win_set_height, state.win, h)
  return state.win
end

-- Right-click anywhere in the panel: the open tab's menu, not the editor's
-- (explorer.lua's right-click sends it here, then shows ]RickTool). Copy
-- when text is selected; on a row (a problem, a message) the entries act on
-- that row; Hide Panel last.
function M.menu_clicked()
  local m = vim.fn.getmousepos()
  local w = state.win
  if not valid() then return end
  local items = {}
  if vim.fn.mode():find("^[vV\22]") and vim.api.nvim_get_current_win() == w then
    -- The menu ends Visual mode before an entry runs: take the text now.
    local text = table.concat(vim.fn.getregion(vim.fn.getpos("v"), vim.fn.getpos("."), { type = vim.fn.mode() }), "\n")
    items[#items + 1] = { "Copy", function()
      vim.fn.setreg("+", text)
      vim.fn.setreg('"', text)
    end }
    items[#items + 1] = { "-" }
  elseif m.winid == w and m.line > 0 and vim.bo[vim.api.nvim_win_get_buf(w)].buftype ~= "terminal" then
    vim.api.nvim_set_current_win(w)
    pcall(vim.api.nvim_win_set_cursor, w, { m.line, 0 })
  end
  local t = tools[state.active]
  local own = t and t.menu and t.menu() or {}
  for _, it in ipairs(own) do items[#items + 1] = it end
  if #own > 0 then items[#items + 1] = { "-" } end
  items[#items + 1] = { "Hide Panel", M.hide }
  require("config.menu").build("Tool", items)
end

-- What an empty tab shows (t.empty: one line saying how to fill it).
local placeholders = {}
local function placeholder(t)
  local b = placeholders[t.id]
  if not (b and vim.api.nvim_buf_is_valid(b)) then
    b = vim.api.nvim_create_buf(false, true)
    vim.bo[b].bufhidden = "hide"
    placeholders[t.id] = b
    vim.keymap.set("n", "q", function() M.hide() end, { buffer = b, nowait = true, desc = "Hide the panel" })
  end
  vim.bo[b].modifiable = true
  vim.api.nvim_buf_set_lines(b, 0, -1, false, { "", "  " .. t.empty })
  vim.bo[b].modifiable = false
  return b
end

-- Show a tool's tab (opening the panel). opts.focus moves the cursor into it.
function M.show(id, opts)
  opts = opts or {}
  local t = tools[id]
  if not t then return end
  local buf = t.buf()
  if not (buf and vim.api.nvim_buf_is_valid(buf)) then
    -- Nothing in it yet (Run before anything ran): the tab still opens, and
    -- says so, as in CLion, rather than refusing to switch.
    if not t.empty then return end
    buf = placeholder(t)
  end
  state.active, state.last = id, id
  state.buf = buf
  local w = open_window(buf)
  if vim.api.nvim_win_get_buf(w) ~= buf then vim.api.nvim_win_set_buf(w, buf) end
  for opt, v in pairs({ number = false, relativenumber = false, signcolumn = "no", foldcolumn = "0", spell = false,
    statuscolumn = "", list = false, wrap = false, fillchars = "eob: " }) do
    vim.wo[w][opt] = v
  end
  vim.wo[w].cursorline = vim.bo[buf].buftype ~= "terminal"
  M.refresh()
  if t.on_show then t.on_show(w) end
  if opts.focus then
    vim.api.nvim_set_current_win(w)
    if vim.bo[buf].buftype == "terminal" then vim.cmd.startinsert() end
  end
  return w
end

function M.hide()
  if not valid() then return end
  state.height = vim.api.nvim_win_get_height(state.win)
  -- The last window can't close: leave the panel then.
  local others = #vim.tbl_filter(function(w)
    return vim.api.nvim_win_get_config(w).relative == "" and w ~= state.win and w ~= state.head
  end, vim.api.nvim_tabpage_list_wins(0))
  if others > 0 then
    if head_valid() then pcall(vim.api.nvim_win_close, state.head, true) end
    pcall(vim.api.nvim_win_close, state.win, true)
  end
  state.win = valid() and state.win or nil
  state.head = head_valid() and state.head or nil
end

-- A tool's key: show its tab, or hide the panel when that tab is showing.
function M.toggle(id, opts)
  id = id or state.last
  if M.is_showing(id) then
    if vim.api.nvim_get_current_win() ~= state.win and opts and opts.focus then
      vim.api.nvim_set_current_win(state.win)
      if vim.bo.buftype == "terminal" then vim.cmd.startinsert() end
      return
    end
    return M.hide()
  end
  M.show(id, opts)
end

-- Problems -------------------------------------------------------------------

local pbuf, scope, editor_buf = nil, "file", nil
local pitems = {} -- line in the Problems buffer -> diagnostic
local pns = vim.api.nvim_create_namespace("rick_problems")
local SEV = {
  [vim.diagnostic.severity.ERROR] = { "E", "DiagnosticError" },
  [vim.diagnostic.severity.WARN] = { "W", "DiagnosticWarn" },
  [vim.diagnostic.severity.INFO] = { "I", "DiagnosticInfo" },
  [vim.diagnostic.severity.HINT] = { "H", "DiagnosticHint" },
}

local function render()
  if not (pbuf and vim.api.nvim_buf_is_valid(pbuf)) then return end
  local diags
  if scope == "file" then
    diags = editor_buf and vim.api.nvim_buf_is_valid(editor_buf) and vim.diagnostic.get(editor_buf) or {}
  else
    diags = vim.diagnostic.get()
  end
  table.sort(diags, function(a, b)
    if a.bufnr ~= b.bufnr then
      return vim.api.nvim_buf_get_name(a.bufnr) < vim.api.nvim_buf_get_name(b.bufnr)
    end
    if a.severity ~= b.severity then return a.severity < b.severity end
    return a.lnum < b.lnum
  end)
  local lines, hls, last = { "" }, {}, nil -- a blank line under the tabs first
  pitems = {}
  for _, d in ipairs(diags) do
    if d.bufnr ~= last then
      last = d.bufnr
      local n = #vim.tbl_filter(function(x) return x.bufnr == d.bufnr end, diags)
      lines[#lines + 1] = (" %s  %s"):format(vim.fn.fnamemodify(vim.api.nvim_buf_get_name(d.bufnr), ":~:."),
        n .. (n == 1 and " problem" or " problems"))
      hls[#hls + 1] = { #lines - 1, 0, -1, "Directory" }
    end
    local sev = SEV[d.severity] or SEV[vim.diagnostic.severity.HINT]
    local msg = (d.message or ""):gsub("\n.*", "")
    local src = d.source and d.source ~= "" and ("  " .. d.source) or ""
    local loc = ("%d:%d"):format(d.lnum + 1, d.col + 1)
    lines[#lines + 1] = ("   %s  %-7s %s%s"):format(sev[1], loc, msg, src)
    hls[#hls + 1] = { #lines - 1, 3, 4, sev[2] }
    hls[#hls + 1] = { #lines - 1, 6, 6 + math.max(7, #loc), "Comment" }
    if src ~= "" then hls[#hls + 1] = { #lines - 1, 7 + math.max(7, #loc) + #msg, -1, "Comment" } end
    pitems[#lines] = d
  end
  if #lines == 1 then
    lines = { "", scope == "file" and "  No problems in this file." or "  No problems in the open files." }
  end
  vim.bo[pbuf].modifiable = true
  vim.api.nvim_buf_set_lines(pbuf, 0, -1, false, lines)
  vim.bo[pbuf].modifiable = false
  -- The cursor starts on the first problem, not the blank line above it.
  local pw = M.is_showing("problems") and state.win
  if pw and vim.api.nvim_win_get_cursor(pw)[1] == 1 and #lines > 1 then pcall(vim.api.nvim_win_set_cursor, pw, { 2, 0 }) end
  vim.api.nvim_buf_clear_namespace(pbuf, pns, 0, -1)
  for _, h in ipairs(hls) do
    pcall(vim.api.nvim_buf_set_extmark, pbuf, pns, h[1], h[2], { end_col = h[3] >= 0 and h[3] or nil,
      end_row = h[3] < 0 and h[1] + 1 or nil, hl_group = h[4] })
  end
  M.refresh()
end

-- The editor window to open a problem in: the last one you were in.
local function editor_win()
  local prev = vim.fn.win_getid(vim.fn.winnr("#"))
  for _, w in ipairs({ prev, unpack(vim.api.nvim_tabpage_list_wins(0)) }) do
    if w ~= 0 and w ~= state.win and vim.api.nvim_win_is_valid(w) and vim.api.nvim_win_get_config(w).relative == ""
      and vim.bo[vim.api.nvim_win_get_buf(w)].buftype == "" then
      return w
    end
  end
end

local function open_problem()
  local d = pitems[vim.api.nvim_win_get_cursor(0)[1]]
  if not d then return end
  local w = editor_win()
  if not w then return end
  vim.api.nvim_set_current_win(w)
  if vim.api.nvim_win_get_buf(w) ~= d.bufnr then vim.api.nvim_win_set_buf(w, d.bufnr) end
  pcall(vim.api.nvim_win_set_cursor, w, { d.lnum + 1, d.col })
  vim.cmd("normal! zz")
end

local function copy_problem()
  local d = pitems[vim.api.nvim_win_get_cursor(0)[1]]
  if not d then return end
  vim.fn.setreg("+", d.message)
  vim.fn.setreg('"', d.message)
end

local function set_scope(s)
  scope = s
  render()
end
_G.RickProblemsScope = function(i) set_scope(i == 1 and "file" or "all") end

local function problems_buf()
  if pbuf and vim.api.nvim_buf_is_valid(pbuf) then return pbuf end
  pbuf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_buf_set_name(pbuf, "Problems")
  vim.bo[pbuf].filetype = "rick_problems"
  vim.bo[pbuf].bufhidden = "hide"
  vim.bo[pbuf].modifiable = false
  local function k(lhs, fn, desc) vim.keymap.set("n", lhs, fn, { buffer = pbuf, nowait = true, desc = desc }) end
  k("<CR>", open_problem, "Go to this problem")
  k("<2-LeftMouse>", open_problem, "Go to this problem")
  k("a", function() set_scope(scope == "file" and "all" or "file") end, "Current file / all files")
  k("q", M.hide, "Hide the panel")
  return pbuf
end

function M.problems_count()
  local n = 0
  for _, d in ipairs(editor_buf and vim.api.nvim_buf_is_valid(editor_buf) and vim.diagnostic.get(editor_buf) or {}) do
    if d.severity <= vim.diagnostic.severity.WARN then n = n + 1 end
  end
  return n
end

-- Terminal --------------------------------------------------------------------

local tbuf
local function terminal_buf()
  if tbuf and vim.api.nvim_buf_is_valid(tbuf) and vim.b[tbuf].terminal_job_id then return tbuf end
  tbuf = vim.api.nvim_create_buf(false, true)
  vim.bo[tbuf].bufhidden = "hide"
  vim.api.nvim_buf_call(tbuf, function()
    vim.fn.jobstart({ vim.o.shell }, { term = true, cwd = vim.fn.getcwd(), on_exit = function()
      vim.schedule(function()
        -- exit in the shell: drop the tab's terminal; the next visit starts a new one.
        if tbuf and vim.api.nvim_buf_is_valid(tbuf) then
          local was = M.is_showing("terminal")
          local old = tbuf
          tbuf = nil
          if was then M.hide() end
          pcall(vim.api.nvim_buf_delete, old, { force = true })
        end
      end)
    end })
  end)
  vim.b[tbuf].term_title = "Terminal"
  return tbuf
end

function M.setup()
  local function hl()
    -- The editor tabs' colours (bufferline): grey names, the open one bright.
    vim.api.nvim_set_hl(0, "RickToolTab", { link = "BufferLineBackground", default = true })
    vim.api.nvim_set_hl(0, "RickToolTabSel", { link = "BufferLineBufferSelected", default = true })
    vim.api.nvim_set_hl(0, "RickToolFill", { link = "BufferLineFill", default = true })
  end
  hl()
  local group = vim.api.nvim_create_augroup("rick_toolwindow", { clear = true })
  vim.api.nvim_create_autocmd("ColorScheme", { group = group, callback = hl })

  M.register({
    id = "problems", title = "Problems",
    buf = function()
      local b = problems_buf()
      render()
      return b
    end,
    extra = function()
      local f, a = scope == "file" and "RickToolTabSel" or "RickToolTab", scope == "all" and "RickToolTabSel" or "RickToolTab"
      -- At the right end of the row.
      return ("%%=%%1@v:lua.RickProblemsScope@%%#%s# Current File %%X%%2@v:lua.RickProblemsScope@%%#%s# All Files %%X"):format(f, a)
    end,
    menu = function()
      local items = {}
      if pitems[vim.api.nvim_win_get_cursor(0)[1]] then
        items = { { "Go to Problem", open_problem }, { "Copy Message", copy_problem }, { "-" } }
      end
      items[#items + 1] = scope == "file" and { "Show All Files", function() set_scope("all") end }
        or { "Show Current File", function() set_scope("file") end }
      return items
    end,
  })
  M.register({
    id = "run", title = "Run",
    menu = function() return require("config.run").menu() end,
    buf = function() return require("config.run").buffer() end,
    extra = function() return require("config.run").tabs_winbar() end,
    empty = "Nothing has run yet (Shift+F10 or Space rr runs)",
    on_show = function(w)
      local b = vim.api.nvim_win_get_buf(w)
      pcall(vim.api.nvim_win_set_cursor, w, { vim.api.nvim_buf_line_count(b), 0 })
    end,
  })
  M.register({
    id = "terminal", title = "Terminal", buf = terminal_buf,
    menu = function()
      local function send(keys)
        local job = tbuf and vim.api.nvim_buf_is_valid(tbuf) and vim.b[tbuf].terminal_job_id
        if job then pcall(vim.api.nvim_chan_send, job, keys) end
      end
      return {
        { "Paste", function() send(vim.fn.getreg("+")) end },
        -- Drop the scrollback too, then let the shell redraw its prompt (Ctrl-L).
        { "Clear", function()
          if not (tbuf and vim.api.nvim_buf_is_valid(tbuf)) then return end
          local sb = vim.bo[tbuf].scrollback
          vim.bo[tbuf].scrollback = 1
          vim.bo[tbuf].scrollback = sb
          send("\12")
        end },
      }
    end,
    -- The title the shell gives itself (usually where it is).
    extra = function()
      local title = tbuf and vim.api.nvim_buf_is_valid(tbuf) and vim.b[tbuf].term_title or ""
      return "%#Comment#" .. title:gsub("%%", "%%%%")
    end,
    -- Back at the end of the output, which went on while another tab was showing.
    on_show = function(w)
      local b = vim.api.nvim_win_get_buf(w)
      pcall(vim.api.nvim_win_set_cursor, w, { vim.api.nvim_buf_line_count(b), 0 })
    end,
  })
  local notes = require("config.notifications")
  M.register({
    id = "notifications", title = "Notifications", buf = notes.buffer, badge = notes.badge, extra = notes.extra,
    menu = notes.menu,
    on_show = notes.on_show,
  })

  -- Problems follows the file you're in and the checks as they finish.
  local pending = false
  local function later()
    if pending then return end
    pending = true
    vim.defer_fn(function()
      pending = false
      if M.is_showing("problems") then render() end
    end, 150)
  end
  vim.api.nvim_create_autocmd("DiagnosticChanged", { group = group, callback = later })
  vim.api.nvim_create_autocmd("BufEnter", {
    group = group,
    callback = function(ev)
      if vim.bo[ev.buf].buftype == "" and vim.api.nvim_win_get_config(0).relative == "" then
        editor_buf = ev.buf
        later()
      end
    end,
  })
  -- Closing the panel any other way (:q in it) forgets it.
  vim.api.nvim_create_autocmd("WinClosed", {
    group = group,
    callback = function(ev)
      local w = tonumber(ev.match)
      if w == state.win then
        state.height = vim.api.nvim_win_get_height(state.win)
        state.win = nil
        -- The head goes with it.
        local head = state.head
        state.head = nil
        if head and vim.api.nvim_win_is_valid(head) then
          vim.schedule(function() pcall(vim.api.nvim_win_close, head, true) end)
        end
      elseif w == state.head then
        state.head = nil
        -- Closed on its own (Ctrl-W o elsewhere keeps neither): put it back.
        vim.schedule(function() if valid() and not head_valid() then open_head(); M.refresh() end end)
      end
    end,
  })
  -- The head is for clicking: moving into it with the keyboard goes on
  -- through it (up to the code, or down into the panel).
  vim.api.nvim_create_autocmd("WinEnter", {
    group = group,
    callback = function()
      local head = state.head
      if not head or vim.api.nvim_get_current_win() ~= head then return end
      local prev = vim.fn.win_getid(vim.fn.winnr("#"))
      vim.schedule(function()
        if vim.api.nvim_get_current_win() ~= head then return end
        if prev == state.win then vim.cmd("wincmd k") end
        if vim.api.nvim_get_current_win() == head and valid() then vim.api.nvim_set_current_win(state.win) end
      end)
    end,
  })
  vim.api.nvim_create_autocmd({ "WinResized", "VimResized" }, {
    group = group,
    callback = function()
      if not valid() then return end
      if head_valid() then
        local extra = vim.api.nvim_win_get_height(state.head) - 1
        if extra ~= 0 then
          pcall(vim.api.nvim_win_set_height, state.win, math.max(1, vim.api.nvim_win_get_height(state.win) + extra))
          pcall(vim.api.nvim_win_set_height, state.head, 1)
        end
      end
      draw_tabs()
    end,
  })
  -- Clicks on the tabs line (it's text, not a bar, so it has no click areas
  -- of its own). The click doesn't move the cursor there.
  local PRESS, DRAG, RELEASE = vim.keycode("<LeftMouse>"), vim.keycode("<LeftDrag>"), vim.keycode("<LeftRelease>")
  local eating = false
  vim.on_key(function(key)
    if key == PRESS then
      local m = vim.fn.getmousepos()
      eating = head_valid() and m.winid == state.head and m.line >= 1
      if not eating then return end
      local col = m.wincol - 1
      for _, h in ipairs(hits) do
        if col >= h[1] and col < h[2] then
          vim.schedule(function()
            if h[3] == "hide" then M.hide() else M.show(h[3], { focus = true }) end
          end)
          break
        end
      end
      return ""
    elseif eating and (key == DRAG or key == RELEASE) then
      if key == RELEASE then eating = false end
      return ""
    end
  end, hns)
  -- A file opened while you're in the panel (clicking a file tab at the top
  -- from the terminal, gd, :e) goes to the editor, and the panel keeps its
  -- tab. Anything else put in the panel (:help) leaves it a plain window.
  vim.api.nvim_create_autocmd("BufWinEnter", {
    group = group,
    callback = function()
      if not valid() then return end
      local b = vim.api.nvim_win_get_buf(state.win)
      if b == state.buf then return end
      local w = vim.bo[b].buftype == "" and editor_win()
      if w and state.buf and vim.api.nvim_buf_is_valid(state.buf) then
        local panel, tool = state.win, state.buf
        vim.schedule(function()
          if not (vim.api.nvim_win_is_valid(panel) and vim.api.nvim_win_is_valid(w)
            and vim.api.nvim_buf_is_valid(b) and vim.api.nvim_buf_is_valid(tool)) then return end
          local cursor = vim.api.nvim_win_get_cursor(panel)
          vim.api.nvim_win_set_buf(panel, tool)
          vim.api.nvim_set_current_win(w)
          vim.api.nvim_win_set_buf(w, b)
          pcall(vim.api.nvim_win_set_cursor, w, cursor)
        end)
        return
      end
      vim.w[state.win].rick_toolwindow = nil
      state.win = nil
      if head_valid() then pcall(vim.api.nvim_win_close, state.head, true) end
      state.head = nil
    end,
  })

  local map = vim.keymap.set
  local function tool(id, focus) return function() M.toggle(id, { focus = focus }) end end
  map({ "n", "t" }, "<D-6>", tool("problems"), { desc = "Problems" })
  map("n", "<leader>tp", tool("problems"), { desc = "Problems" })
  map({ "n", "t" }, "<D-0>", function() require("config.build").toggle_panel() end, { desc = "Build output" })
  map({ "n", "t" }, "<D-4>", tool("run"), { desc = "Run panel" })
  map({ "n", "t" }, "<M-F12>", tool("terminal", true), { desc = "Terminal" })
  map({ "n", "t" }, "<F60>", tool("terminal", true), { desc = "Terminal" }) -- Alt+F12 as terminals send it
  map("n", "<leader>tt", tool("terminal", true), { desc = "Terminal" })
  map("n", "<leader>tW", function() M.toggle(nil) end, { desc = "Show / hide the tool window" })
  vim.api.nvim_create_autocmd("WinEnter", {
    group = group,
    callback = function()
      if vim.api.nvim_get_current_win() == state.win then
        vim.keymap.set({ "n", "t" }, "<S-Esc>", M.hide, { buffer = true, desc = "Hide the tool window" })
      end
    end,
  })
  vim.api.nvim_create_user_command("Problems", function() M.show("problems", { focus = true }) end, { desc = "Problems tool window" })
end

return M
