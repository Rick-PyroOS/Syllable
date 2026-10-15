-- Notifications like CLion's balloons: one small window in the bottom-right
-- corner with the newest message in full, "2 of 5", and clickable
-- < Previous, Next >,  Copy and Close. Info and hints fade after a few seconds;
-- warnings and errors stay until you close them. Click the balloon (or Space N)
-- to focus it: [ / ] step through, y copies, Esc or q closes.
--
-- Snacks still records every message for the Event Log (Space n); with the
-- balloon on (Space tn, remembered) its corner toasts are hidden.
-- Progress messages that update in place (LSP indexing, spinners) keep the toasts.

local M = {}
local ns = vim.api.nvim_create_namespace("rick_balloon")
local FADE_MS, MAX_W, MAX_H = 5000, 72, 12

local items = {} -- every message this session, oldest first
local idx -- the one shown
local win, buf, timer
local buttons = {} -- { label, from, to } byte columns on the button row
local LEVEL = {
  [vim.log.levels.ERROR] = { "Error", "DiagnosticError", "" },
  [vim.log.levels.WARN] = { "Warning", "DiagnosticWarn", "" },
  [vim.log.levels.INFO] = { "Info", "DiagnosticInfo", "" },
  [vim.log.levels.DEBUG] = { "Debug", "Comment", "" },
  [vim.log.levels.TRACE] = { "Trace", "Comment", "" },
}
local NAMES = { error = 4, warn = 3, info = 2, debug = 1, trace = 0 }

local function level_of(n)
  local l = n.level
  if type(l) == "string" then l = NAMES[l:lower()] end
  return l or vim.log.levels.INFO
end

function M.enabled() return require("config.prefs").get("balloon") end
function M.is_open() return win ~= nil and vim.api.nvim_win_is_valid(win) end

function M.close()
  if timer then timer:stop() end
  if M.is_open() then pcall(vim.api.nvim_win_close, win, true) end
  win = nil
end

-- Wrap text to width, keeping blank lines.
local function wrap(text, width)
  local out = {}
  for _, line in ipairs(vim.split(text, "\n", { plain = true })) do
    line = line:gsub("\t", "    ")
    if line == "" then out[#out + 1] = "" end
    while #line > 0 do
      if vim.fn.strdisplaywidth(line) <= width then out[#out + 1] = line; break end
      local cut = width
      local space = line:sub(1, width):match(".*()%s")
      if space and space > width / 2 then cut = space end
      out[#out + 1] = line:sub(1, cut):gsub("%s+$", "")
      line = line:sub(cut + 1):gsub("^%s+", "")
    end
  end
  while #out > 0 and out[#out] == "" do out[#out] = nil end
  return out
end

local function render()
  local n = items[idx]
  if not n then return M.close() end
  local lvl = LEVEL[level_of(n)] or LEVEL[vim.log.levels.INFO]
  local width = math.min(MAX_W, math.max(44, vim.o.columns - 10))
  local body = wrap(n.msg or "", width - 2)
  local w = 0
  for _, l in ipairs(body) do w = math.max(w, vim.fn.strdisplaywidth(l)) end

  -- Button row: < Previous  Next >  Copy  Close  ·  2 of 5
  local parts = {
    { "< Previous", "prev", idx > 1 }, { "Next >", "next", idx < #items }, { " Copy", "copy", true }, { "Close", "close", true },
  }
  local row, col = "", 0
  buttons = {}
  for i, p in ipairs(parts) do
    if i > 1 then row = row .. "  " end
    local from, cell = #row, vim.fn.strdisplaywidth(row)
    row = row .. " " .. p[1] .. " "
    -- from / to: bytes, for the highlight; cell_from / cell_to: screen cells, for clicks.
    buttons[#buttons + 1] = { action = p[2], from = from, to = #row, cell_from = cell,
      cell_to = vim.fn.strdisplaywidth(row), on = p[3] }
  end
  local count = idx .. " of " .. #items
  w = math.max(w, vim.fn.strdisplaywidth(row) + 2 + #count, 30)
  w = math.min(w + 2, width)
  row = row .. string.rep(" ", math.max(1, w - 1 - vim.fn.strdisplaywidth(row) - #count)) .. count
  local lines = {}
  for _, l in ipairs(body) do lines[#lines + 1] = " " .. l end
  local more = #lines > MAX_H
  if more then lines = vim.list_slice(lines, 1, MAX_H); lines[#lines] = " … (y copies the whole message)" end
  lines[#lines + 1] = ""
  lines[#lines + 1] = row
  for _, b in ipairs(buttons) do b.row = #lines - 1 end

  if not (buf and vim.api.nvim_buf_is_valid(buf)) then
    buf = vim.api.nvim_create_buf(false, true)
    vim.bo[buf].bufhidden = "hide"
    vim.bo[buf].filetype = "rick_balloon"
    local function k(lhs, fn) vim.keymap.set("n", lhs, fn, { buffer = buf, nowait = true }) end
    k("[", function() M.step(-1) end)
    k("]", function() M.step(1) end)
    k("y", function() M.copy() end)
    k("<Esc>", M.close)
    k("q", M.close)
  end
  vim.bo[buf].modifiable = true
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, lines)
  vim.bo[buf].modifiable = false
  vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
  for _, b in ipairs(buttons) do
    vim.api.nvim_buf_set_extmark(buf, ns, b.row, b.from, { end_col = b.to, hl_group = b.on and "RickBalloonButton" or "RickBalloonButtonOff" })
  end
  vim.api.nvim_buf_set_extmark(buf, ns, #lines - 1, #lines[#lines] - #count, { end_col = #lines[#lines], hl_group = "Comment" })

  local title = (n.title and n.title ~= "" and n.title or lvl[1])
  local cfg = {
    relative = "editor", anchor = "SE", width = w, height = #lines,
    row = vim.o.lines - vim.o.cmdheight - (vim.o.laststatus > 0 and 1 or 0), col = vim.o.columns - 1,
    border = "rounded", title = { { " " .. lvl[3] .. "  " .. title .. " ", lvl[2] } }, title_pos = "left",
    style = "minimal", focusable = true, zindex = 60,
  }
  if M.is_open() then
    vim.api.nvim_win_set_config(win, cfg)
  else
    cfg.noautocmd = true
    win = vim.api.nvim_open_win(buf, false, cfg)
    vim.wo[win].winhighlight = "NormalFloat:RickBalloon,FloatBorder:" .. lvl[2]
    vim.wo[win].wrap = false
  end
  vim.wo[win].winhighlight = "NormalFloat:RickBalloon,FloatBorder:" .. lvl[2]

  -- Info fades unless you are in the balloon; warnings and errors stay.
  if timer then timer:stop() end
  if level_of(n) < vim.log.levels.WARN then
    timer = timer or vim.uv.new_timer()
    timer:start(FADE_MS, 0, vim.schedule_wrap(function()
      if M.is_open() and vim.api.nvim_get_current_win() ~= win then M.close(); vim.cmd.redraw() end
    end))
  end
end

function M.step(d)
  if not items[1] then return end
  idx = math.max(1, math.min(#items, (idx or #items) + d))
  render()
end

function M.copy()
  local n = items[idx]
  if not n then return end
  vim.fn.setreg("+", n.msg or "")
  vim.fn.setreg('"', n.msg or "")
  vim.api.nvim_echo({ { "Copied the message" } }, false, {})
end

function M.focus()
  if not M.is_open() then
    if not items[1] then return vim.api.nvim_echo({ { "No messages yet" } }, false, {}) end
    idx = #items
    render()
  end
  vim.api.nvim_set_current_win(win)
end

-- Snacks' filter: show it here instead of as a toast (it still goes in the history).
function M.filter(notif)
  if not M.enabled() then return true end
  if notif.id and type(notif.id) ~= "number" then return true end -- progress that updates in place
  if (notif.msg or "") == "" then return true end
  items[#items + 1] = { msg = notif.msg, title = notif.title, level = notif.level }
  idx = #items
  vim.schedule(render)
  return false
end

-- Clicks on the button row, whichever window has focus.
local function on_click()
  local m = vim.fn.getmousepos()
  if not (M.is_open() and m.winid == win) then return end
  local col = m.wincol - 1 -- 0-based in the window (no border inside wincol)
  for _, b in ipairs(buttons) do
    if m.line - 1 == b.row and col >= b.cell_from and col < b.cell_to then
      if not b.on then return "" end
      vim.schedule(function()
        if b.action == "prev" then M.step(-1)
        elseif b.action == "next" then M.step(1)
        elseif b.action == "copy" then M.copy()
        else M.close() end
      end)
      return ""
    end
  end
end

function M.setup()
  local function hl()
    vim.api.nvim_set_hl(0, "RickBalloon", { link = "NormalFloat", default = true })
    vim.api.nvim_set_hl(0, "RickBalloonButton", { link = "PmenuSel", default = true })
    vim.api.nvim_set_hl(0, "RickBalloonButtonOff", { link = "Comment", default = true })
  end
  hl()
  vim.api.nvim_create_autocmd("ColorScheme", { group = vim.api.nvim_create_augroup("rick_balloon", { clear = true }), callback = hl })
  vim.api.nvim_create_autocmd("VimResized", { group = "rick_balloon", callback = function() if M.is_open() then render() end end })
  local press, release = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
  local swallow_release = false
  vim.on_key(function(key)
    if key == press then
      local r = on_click()
      swallow_release = r == ""
      return r
    elseif key == release and swallow_release then
      swallow_release = false
      return ""
    end
  end, ns)
  vim.keymap.set("n", "<leader>N", M.focus, { desc = "Focus the notification balloon" })
end

return M
