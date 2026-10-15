-- Quick documentation on mouse hover, like CLion: rest the mouse on a name in
-- the code for a moment (Settings > Hover delay) and clangd's hover (its type,
-- its value for a constant, its doc comment) pops up beside the pointer.
-- Moving off the name closes it; moving onto the popup keeps it open, and a
-- click there goes in to scroll or copy. F1 / K still show it for the name under the cursor.
-- Space tq turns it off and on (prefs.lua, on by default).

local M = {}

local MOUSE_MOVE = vim.keycode("<MouseMove>")
local timer = vim.uv.new_timer()
local shown -- { win, src_win, lnum, first, last }: the popup and the word it's for
local waiting -- the word a request is out for, as "win:lnum:first"

local function close()
  if shown and vim.api.nvim_win_is_valid(shown.win) then pcall(vim.api.nvim_win_close, shown.win, true) end
  shown = nil
end

-- The word (letters, digits, _) under the mouse: its line (1-based) and first
-- and last byte columns (1-based), or nil when the mouse isn't on one in a
-- file window with a language server that can hover.
local function word_at(m)
  if m.winid == 0 or m.line < 1 or m.column < 1 or not vim.api.nvim_win_is_valid(m.winid) then return end
  if vim.api.nvim_win_get_config(m.winid).relative ~= "" then return end
  if require("config.dialogs").busy() then return end -- never over Find or another dialog
  local info = vim.fn.getwininfo(m.winid)[1]
  if not info or m.wincol <= info.textoff then return end -- the margin
  local buf = vim.api.nvim_win_get_buf(m.winid)
  if vim.bo[buf].buftype ~= "" then return end
  local text = vim.api.nvim_buf_get_lines(buf, m.line - 1, m.line, false)[1] or ""
  if m.column > #text or not text:sub(m.column, m.column):match("[%w_]") then return end
  local first, last = m.column, m.column
  while first > 1 and text:sub(first - 1, first - 1):match("[%w_]") do first = first - 1 end
  while last < #text and text:sub(last + 1, last + 1):match("[%w_]") do last = last + 1 end
  if text:sub(first, first):match("%d") then return end -- a number
  if #vim.lsp.get_clients({ bufnr = buf, method = "textDocument/hover" }) == 0 then return end
  return { win = m.winid, buf = buf, lnum = m.line, first = first, last = last, col = m.column, text = text }
end

local function show(w)
  local key = w.win .. ":" .. w.lnum .. ":" .. w.first
  waiting = key
  vim.lsp.buf_request_all(w.buf, "textDocument/hover", function(client)
    return {
      textDocument = { uri = vim.uri_from_bufnr(w.buf) },
      position = { line = w.lnum - 1, character = vim.str_utfindex(w.text, client.offset_encoding, w.col - 1, false) },
    }
  end, function(results)
    if waiting ~= key then return end
    waiting = nil
    -- Still on the same word, and nothing else (a menu, insert mode) going on?
    local now = word_at(vim.fn.getmousepos())
    if not now or now.win ~= w.win or now.lnum ~= w.lnum or now.first ~= w.first or vim.fn.mode() ~= "n" then return end
    local lines = {}
    for _, r in pairs(results) do
      local contents = r.result and r.result.contents
      if contents then
        local md = vim.lsp.util.convert_input_to_markdown_lines(contents)
        if #md > 0 then
          if #lines > 0 then vim.list_extend(lines, { "", "---", "" }) end
          vim.list_extend(lines, md)
        end
      end
    end
    if #lines == 0 then return end
    close()
    local max_width = math.min(100, math.floor(vim.o.columns * 0.6))
    local _, win = vim.lsp.util.open_floating_preview(lines, "markdown", {
      border = "rounded",
      relative = "editor", -- sized to the screen, not the window the cursor is in
      focus = false,
      max_width = max_width,
      max_height = vim.o.lines - 4, -- trimmed to the room above or below the pointer next
    })
    -- Put it just below the pointer, or above when there's more room there.
    local mp = vim.fn.getmousepos()
    local width = require("config.doc_links").content_width(win, max_width)
    vim.api.nvim_win_set_width(win, width)
    local height = vim.api.nvim_win_text_height(win, {}).all
    local below, above = vim.o.lines - vim.o.cmdheight - mp.screenrow - 2, mp.screenrow - 3
    local down = height <= below or below >= above
    vim.api.nvim_win_set_config(win, {
      relative = "editor",
      anchor = down and "NW" or "SW",
      row = down and mp.screenrow or mp.screenrow - 1,
      col = math.max(0, math.min(mp.screencol - 1, vim.o.columns - width - 2)),
      height = math.max(1, math.min(height, down and below or above)),
    })
    shown = { win = win, src_win = w.win, lnum = w.lnum, first = w.first, last = w.last }
  end)
end

local function on_mouse_move()
  local m = vim.fn.getmousepos()
  if shown then
    if not vim.api.nvim_win_is_valid(shown.win) then
      shown = nil
    elseif m.winid == shown.win then
      return -- on the popup: keep it
    elseif m.winid == shown.src_win and m.line == shown.lnum and m.column >= shown.first and m.column <= shown.last then
      return -- still on the word
    elseif vim.api.nvim_get_current_win() ~= shown.win then
      close()
    end
  end
  timer:stop()
  waiting = nil
  if not require("config.prefs").get("mouse_docs") or vim.fn.mode() ~= "n" then return end
  local w = word_at(m)
  if not w then return end
  -- How long the mouse rests first: Settings > Hover delay (prefs.lua).
  timer:start(require("config.prefs").get("hover_delay"), 0, vim.schedule_wrap(function()
    local now = word_at(vim.fn.getmousepos())
    if now and now.win == w.win and now.lnum == w.lnum and now.first == w.first then show(now) end
  end))
end

function M.setup()
  -- Watched in on_key rather than mapped: a mapped <MouseMove> closes
  -- right-click menus as soon as the mouse moves.
  vim.o.mousemoveevent = true
  vim.on_key(function(k)
    if k == MOUSE_MOVE then vim.schedule(on_mouse_move) end
  end, vim.api.nvim_create_namespace("rick_quick_docs"))
  -- Typing or switching files closes it too (open_floating_preview already
  -- closes it when the cursor moves).
  vim.api.nvim_create_autocmd({ "InsertEnter", "BufLeave", "FocusLost" }, {
    group = vim.api.nvim_create_augroup("rick_quick_docs", { clear = true }),
    callback = function()
      timer:stop()
      -- (BufLeave also comes first when you click into the popup: keep it then.)
      if shown and vim.api.nvim_get_current_win() ~= shown.win and vim.fn.getmousepos().winid ~= shown.win then close() end
    end,
  })
end

M.close = close

return M
