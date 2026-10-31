-- Terminal panels (Space tt at the bottom, Claude on the right) keep their
-- share of the screen when the terminal window is resized: the bottom panel
-- stays about the same fraction of the height, the side one of the width,
-- instead of keeping their old size and squeezing the code. A panel you've
-- resized by dragging keeps the share you gave it. A panel that was scrolled
-- to the end stays at the end, and keeps following new output while you work
-- in another window (Neovim only follows when the cursor is on the very last
-- line, so after clear, or in a new terminal, output ran off the bottom).
-- Typing in a terminal panel always goes to the shell: clicking in it, or
-- moving into it from another window, puts you back in terminal mode (a click
-- used to leave you in Normal mode, so your typing ran as Vim commands).
-- Dragging still selects text to copy. A terminal you aren't typing in still
-- shows where its cursor is.

local M = {}

local DEFAULT_HEIGHT, DEFAULT_WIDTH = 0.3, 0.4 -- shares of the screen

local function panels()
  local out = {}
  for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if vim.api.nvim_win_get_config(w).relative == "" and vim.bo[vim.api.nvim_win_get_buf(w)].buftype == "terminal" then
      local wide = vim.api.nvim_win_get_width(w) >= vim.o.columns - 1
      out[#out + 1] = { win = w, side = wide and "bottom" or "side" }
    end
  end
  return out
end

local function editor_height() return vim.o.lines - vim.o.cmdheight - (vim.o.showtabline == 2 and 1 or 0) end

-- The share a panel has now (after you dragged it, say).
local function remember()
  for _, p in ipairs(panels()) do
    if p.side == "bottom" then
      vim.w[p.win].rick_share = vim.api.nvim_win_get_height(p.win) / editor_height()
    else
      vim.w[p.win].rick_share = vim.api.nvim_win_get_width(p.win) / vim.o.columns
    end
  end
end

local function at_end(win)
  local buf = vim.api.nvim_win_get_buf(win)
  return vim.api.nvim_win_get_cursor(win)[1] >= vim.api.nvim_buf_line_count(buf) - 1
end

local function restore()
  for _, p in ipairs(panels()) do
    local w = p.win
    local was_end = at_end(w)
    if p.side == "bottom" then
      local share = vim.w[w].rick_share or DEFAULT_HEIGHT
      vim.api.nvim_win_set_height(w, math.max(5, math.floor(editor_height() * share + 0.5)))
    else
      local share = vim.w[w].rick_share or DEFAULT_WIDTH
      vim.api.nvim_win_set_width(w, math.max(20, math.floor(vim.o.columns * share + 0.5)))
    end
    -- Back to the end (typing in it follows the end anyway).
    if was_end and not (w == vim.api.nvim_get_current_win() and vim.fn.mode() == "t") then
      local buf = vim.api.nvim_win_get_buf(w)
      pcall(vim.api.nvim_win_set_cursor, w, { vim.api.nvim_buf_line_count(buf), 0 })
    end
  end
end

-- Where the shell's cursor is in a terminal you aren't typing in: a steady
-- block, as Neovim draws no cursor there (since 0.11). It's where the cursor
-- was when you left, and after new output the end of the last line written.
local ncur = vim.api.nvim_create_namespace("rick_term_cursor")
local function draw_cursor(buf, row, col)
  vim.api.nvim_buf_clear_namespace(buf, ncur, 0, -1)
  if not row then return end
  local line = vim.api.nvim_buf_get_lines(buf, row, row + 1, false)[1]
  if not line then return end
  col = math.min(col, #line)
  local ch = vim.fn.strcharpart(line:sub(col + 1), 0, 1)
  pcall(vim.api.nvim_buf_set_extmark, buf, ncur, row, col, {
    virt_text = { { ch == "" and " " or ch, "RickTermCursorNC" } }, virt_text_pos = "overlay", priority = 300,
  })
end

-- Only the screen's worth of lines at the end: reading all the scrollback
-- after every bit of output slowed everything down.
local function cursor_after_output(buf)
  local first = math.max(0, vim.api.nvim_buf_line_count(buf) - 200)
  local lines = vim.api.nvim_buf_get_lines(buf, first, -1, false)
  for i = #lines, 1, -1 do
    if lines[i]:find("%S") then
      local row = first + i - 1
      local saved = vim.b[buf].rick_term_cursor
      local col = #lines[i]
      -- Still the prompt you left (its trailing space isn't in the buffer).
      if saved and saved[1] == row and saved[2] > col then col = saved[2] end
      return row, col
    end
  end
end

-- New output in a terminal scrolls every window showing it whose view reached
-- the old end; a window you scrolled back with the wheel stays put. The
-- window you're typing in follows by itself.
local following = {}
local function follow(buf)
  if following[buf] then return end
  following[buf] = true
  local pending
  vim.api.nvim_buf_attach(buf, false, {
    on_lines = function(_, b, _, _, last, new_last)
      if not following[b] then return true end
      if pending then return end
      local old_count = vim.api.nvim_buf_line_count(b) - (new_last - last)
      pending = {}
      for _, win in ipairs(vim.fn.win_findbuf(b)) do
        local info = vim.fn.getwininfo(win)[1]
        if info and info.botline >= old_count - 1 then pending[#pending + 1] = win end
      end
      vim.schedule(function()
        local wins = pending
        pending = nil
        if not vim.api.nvim_buf_is_valid(b) then return end
        local count = vim.api.nvim_buf_line_count(b)
        if not (vim.api.nvim_get_current_buf() == b and vim.fn.mode() == "t") then
          draw_cursor(b, cursor_after_output(b))
        end
        for _, win in ipairs(wins) do
          if vim.api.nvim_win_is_valid(win) and vim.api.nvim_win_get_buf(win) == b
            and not (win == vim.api.nvim_get_current_win() and vim.fn.mode() == "t") then
            pcall(vim.api.nvim_win_set_cursor, win, { count, 0 })
          end
        end
      end)
    end,
    on_detach = function(_, b) following[b] = nil end,
  })
end

function M.setup()
  local group = vim.api.nvim_create_augroup("rick_panels", { clear = true })
  local resizing = false
  vim.api.nvim_create_autocmd("VimResized", {
    group = group,
    callback = function()
      resizing = true
      restore()
      vim.schedule(function() resizing = false end)
    end,
  })
  -- A drag (or anything else that changes a panel's size) sets its new share.
  vim.api.nvim_create_autocmd("WinResized", {
    group = group,
    callback = function() if not resizing then remember() end end,
  })
  vim.api.nvim_create_autocmd("TermOpen", {
    group = group,
    callback = function(ev)
      vim.schedule(remember)
      follow(ev.buf)
      -- A click (not a drag, which ends in Visual mode) goes back to typing.
      vim.keymap.set("n", "<LeftRelease>", "<LeftRelease><Cmd>startinsert<CR>", { buffer = ev.buf, desc = "Back to typing" })
    end,
  })
  -- The same from any click that ends in Normal mode in a terminal panel, even
  -- if something else maps <LeftRelease> there (on_key sees every key).
  local RELEASE = vim.keycode("<LeftRelease>")
  vim.on_key(function(k, typed)
    if (typed ~= "" and typed or k) ~= RELEASE then return end
    vim.schedule(function()
      if vim.bo.buftype == "terminal" and vim.fn.mode() == "n" and vim.api.nvim_win_get_config(0).relative == "" then
        vim.cmd.startinsert()
      end
    end)
  end, vim.api.nvim_create_namespace("rick_panels_click"))
  -- Only once the move is over: code that passes through the terminal on its
  -- way somewhere else (or steps out and straight back) shouldn't start typing
  -- there, or the terminal flips in and out of typing.
  vim.api.nvim_create_autocmd("WinEnter", {
    group = group,
    callback = function()
      local win = vim.api.nvim_get_current_win()
      vim.schedule(function()
        if vim.api.nvim_get_current_win() == win and vim.bo.buftype == "terminal"
          and vim.api.nvim_get_mode().mode == "nt" and vim.api.nvim_win_get_config(0).relative == "" then
          vim.cmd.startinsert()
        end
      end)
    end,
  })
  vim.api.nvim_set_hl(0, "RickTermCursorNC", { link = "Cursor", default = true })
  vim.api.nvim_create_autocmd("TermLeave", {
    group = group,
    callback = function(ev)
      -- Normal mode can't sit past the last character, where a shell's cursor
      -- usually is: at the end of the line means after it.
      local pos = vim.api.nvim_win_get_cursor(0)
      local line = vim.api.nvim_buf_get_lines(ev.buf, pos[1] - 1, pos[1], false)[1] or ""
      local col = pos[2] >= #line - 1 and #line or pos[2]
      vim.b[ev.buf].rick_term_cursor = { pos[1] - 1, col }
      draw_cursor(ev.buf, pos[1] - 1, col)
    end,
  })
  vim.api.nvim_create_autocmd("TermEnter", {
    group = group,
    callback = function(ev) draw_cursor(ev.buf) end,
  })
  -- No scroll margin in terminals: with scrolloff the view jumps by a few
  -- lines whenever the cursor moves near the bottom, so the prompt shifts.
  vim.api.nvim_create_autocmd({ "TermOpen", "BufWinEnter" }, {
    group = group,
    callback = function(ev)
      if vim.bo[ev.buf].buftype == "terminal" then
        vim.opt_local.scrolloff = 0
        vim.opt_local.sidescrolloff = 0
      end
    end,
  })
  -- Typing in a terminal panel that was scrolled back (the wheel leaves it in
  -- Normal mode) goes to the shell and jumps to the end, as in CLion. Keys
  -- that finish a Normal command (after g, z, Space, Ctrl-W...), :, / and ?,
  -- and keys the terminal maps itself (snacks' q) keep their Normal meaning.
  local PREFIX = {}
  for _, k in ipairs({ "g", "z", "Z", "[", "]", '"', "'", "`", "m", "f", "t", "F", "T", "r", "q", "@", " ", "\23" }) do
    PREFIX[k] = true
  end
  local KEEP = { [":"] = true, ["/"] = true, ["?"] = true, [" "] = true }
  local ENTER, BS = vim.keycode("<CR>"), vim.keycode("<BS>")
  -- Once a key has gone to the shell, the rest of that burst of typing (a
  -- paste, or keys typed before the switch lands) follows it, Space included.
  local prev, typing
  vim.on_key(function(_, typed)
    if typed == "" then return end
    local after = prev
    prev = typed
    -- A right-click menu open over the terminal: its keys pick an entry.
    if vim.bo.buftype ~= "terminal" or vim.api.nvim_get_mode().mode ~= "nt"
      or vim.api.nvim_win_get_config(0).relative ~= "" or vim.fn.pumvisible() == 1 then
      prev = nil
      return
    end
    local send
    if typed == ENTER or typed == "\r" then
      send = "\r"
    elseif typed == BS then
      send = "\127"
    elseif not typed:find("\128", 1, true) and (typing or not (KEEP[typed] or typed:find("[%z\1-\31\127]"))) then
      send = typed
    end
    if not send then return end
    if not typing and ((after and PREFIX[after]) or vim.fn.maparg(typed, "n", false, true).buffer == 1) then return end
    local job = vim.b.terminal_job_id
    if not job then return end
    prev = nil
    if not typing then
      typing = {}
      vim.schedule(function()
        local keys = table.concat(typing)
        typing = nil
        vim.cmd.startinsert()
        pcall(vim.api.nvim_chan_send, job, keys)
      end)
    end
    table.insert(typing, send)
    return ""
  end, vim.api.nvim_create_namespace("rick_panels_type"))
end

return M
