-- Notifications, like CLion's Notifications tool window: messages (a build
-- finished, a check failed, a file was reloaded...) go in the Notifications
-- tab of the bottom panel instead of popping up over the code. The newest is
-- on top. A new message also shows on one line at the bottom of the screen,
-- and the tab counts the ones you haven't seen (yellow when one is a warning,
-- red for an error). Space N (or clicking the tab) opens it; while you're
-- typing in a terminal, Space goes to the shell, so the line says to click
-- the tab instead. In it, y or
-- Enter copies the message under the cursor, dd removes it, and C (or Clear
-- All) removes them all.
-- Messages that update in place (download or indexing progress) keep one
-- entry, kept up to date.
--
-- Snacks still keeps every message for the Event Log (Space n). Space tn
-- (remembered) turns the tab off: messages pop up in the corner again.

local M = {}

local ns = vim.api.nvim_create_namespace("rick_notifications")
local items = {} -- oldest first: { msg, title, level, time, id }
local unread, worst = 0, 0
local buf
local at = {} -- line in the buffer -> the message on it
local MAX = 300

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

local function tw() return require("config.toolwindow") end
local function showing() return tw().is_showing("notifications") end

local function render()
  if not (buf and vim.api.nvim_buf_is_valid(buf)) then return end
  local lines, hls = {}, {}
  at = {}
  for i = #items, 1, -1 do
    local n = items[i]
    local lvl = LEVEL[n.level] or LEVEL[vim.log.levels.INFO]
    local icon, time = " " .. lvl[3] .. "  ", os.date("%H:%M", n.time)
    local title = n.title ~= "" and n.title or lvl[1]
    lines[#lines + 1] = icon .. time .. "  " .. title
    hls[#hls + 1] = { #lines - 1, 0, #icon, lvl[2] }
    hls[#hls + 1] = { #lines - 1, #icon, #icon + #time, "Comment" }
    hls[#hls + 1] = { #lines - 1, #icon + #time + 2, -1, "Bold" }
    at[#lines] = n
    for _, l in ipairs(vim.split(n.msg, "\n", { plain = true })) do
      lines[#lines + 1] = "     " .. l:gsub("\t", "    ")
      at[#lines] = n
    end
    lines[#lines + 1] = ""
  end
  if #lines == 0 then lines = { "  No notifications." } end
  vim.bo[buf].modifiable = true
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, lines)
  vim.bo[buf].modifiable = false
  vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
  for _, h in ipairs(hls) do
    pcall(vim.api.nvim_buf_set_extmark, buf, ns, h[1], h[2], { end_col = h[3] >= 0 and h[3] or #lines[h[1] + 1],
      hl_group = h[4] })
  end
end

-- The new message on one line at the bottom of the screen (nothing to close;
-- the next message or mode change replaces it). Several at once: the last,
-- with how many more there are in the tab.
local queued = {}
local function echo(n)
  queued[#queued + 1] = n
  if #queued > 1 then return end
  vim.schedule(function()
    local last, more = queued[#queued], #queued - 1
    queued = {}
    if showing() or vim.fn.mode() == "c" then return end
    local lvl = LEVEL[last.level] or LEVEL[vim.log.levels.INFO]
    local text = (last.title ~= "" and (last.title .. ": ") or "") .. (last.msg:gsub("\n.*", ""))
    local how = vim.bo.buftype == "terminal" and "click Notifications in the bottom panel" or "Space N"
    local tail = (more > 0 and ("  (+" .. more .. " more)") or "") .. "   " .. how
    local room = vim.o.columns - 4 - vim.fn.strdisplaywidth(tail)
    if vim.fn.strdisplaywidth(text) > room then text = vim.fn.strcharpart(text, 0, math.max(0, room - 1)) .. "…" end
    vim.api.nvim_echo({ { lvl[3] .. " ", lvl[2] }, { text }, { tail, "Comment" } }, false, {})
  end)
end

-- Snacks' filter: keep it here instead of as a corner popup (Snacks still
-- records it for the Event Log).
function M.filter(notif)
  if not M.enabled() then return true end
  local msg = notif.msg or ""
  if msg == "" then return false end
  local level = level_of(notif)
  local title = notif.title or ""
  -- Snacks numbers each new message; plugins name the ones they update in place.
  local id = type(notif.id) ~= "number" and notif.id or nil
  if id ~= nil then
    for _, it in ipairs(items) do
      if it.id == id then
        it.msg, it.title, it.level, it.time = msg, title, level, os.time()
        vim.schedule(render)
        return false
      end
    end
  end
  local n = { msg = msg, title = title, level = level, time = os.time(), id = id }
  items[#items + 1] = n
  if #items > MAX then table.remove(items, 1) end
  if not showing() then
    unread = unread + 1
    worst = math.max(worst, level)
  end
  vim.schedule(function()
    render()
    tw().refresh()
  end)
  echo(n)
  return false
end

-- The tab's unread count, in the editor tabs' background.
function M.badge()
  if unread == 0 then return "" end
  if worst < vim.log.levels.WARN then return tostring(unread) end
  local name = worst >= vim.log.levels.ERROR and "DiagnosticError" or "DiagnosticWarn"
  local base = tw().active() == "notifications" and "RickToolTabSel" or "RickToolTab"
  local fg = vim.api.nvim_get_hl(0, { name = name, link = false }).fg
  local bg = vim.api.nvim_get_hl(0, { name = base, link = false }).bg
  vim.api.nvim_set_hl(0, "RickToolBadge", { fg = fg, bg = bg, bold = true })
  return "● " .. unread, "RickToolBadge"
end

_G.RickNotesClear = function() M.clear() end
function M.extra()
  return #items > 0 and "%@v:lua.RickNotesClear@%#RickToolTab# Clear All %X" or ""
end

function M.on_show()
  unread, worst = 0, 0
  vim.api.nvim_echo({ { "" } }, false, {}) -- the one-line copy at the bottom
  render()
  tw().refresh()
end

function M.clear()
  items, unread, worst = {}, 0, 0
  render()
  tw().refresh()
end

local function under_cursor() return at[vim.api.nvim_win_get_cursor(0)[1]] end

local function copy()
  local n = under_cursor()
  if not n then return end
  vim.fn.setreg("+", n.msg)
  vim.fn.setreg('"', n.msg)
  vim.api.nvim_echo({ { "Copied the message" } }, false, {})
end

local function remove()
  local n = under_cursor()
  if not n then return end
  for i, it in ipairs(items) do
    if it == n then table.remove(items, i) break end
  end
  render()
  tw().refresh()
end

-- The tab's right-click entries (on a message: copy or remove it).
function M.menu()
  local entries = {}
  if under_cursor() then entries = { { "Copy Message", copy }, { "Remove", remove } } end
  if #items > 0 then entries[#entries + 1] = { "Clear All", M.clear } end
  return entries
end

function M.buffer()
  if buf and vim.api.nvim_buf_is_valid(buf) then
    render()
    return buf
  end
  buf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_buf_set_name(buf, "Notifications")
  vim.bo[buf].filetype = "rick_notifications"
  vim.bo[buf].bufhidden = "hide"
  vim.bo[buf].modifiable = false
  local function k(lhs, fn, desc) vim.keymap.set("n", lhs, fn, { buffer = buf, nowait = true, desc = desc }) end
  k("y", copy, "Copy this message")
  k("<CR>", copy, "Copy this message")
  k("dd", remove, "Remove this message")
  k("C", M.clear, "Remove all messages")
  k("q", function() tw().hide() end, "Hide the panel")
  render()
  return buf
end

function M.focus() tw().show("notifications", { focus = true }) end

function M.setup()
  vim.keymap.set("n", "<leader>N", M.focus, { desc = "Notifications" })
end

return M
