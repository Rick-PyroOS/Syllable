-- Terminal panels (Space tt at the bottom, Claude on the right) keep their
-- share of the screen when the terminal window is resized: the bottom panel
-- stays about the same fraction of the height, the side one of the width,
-- instead of keeping their old size and squeezing the code. A panel you've
-- resized by dragging keeps the share you gave it. A panel that was scrolled
-- to the end stays at the end.
-- Typing in a terminal panel always goes to the shell: clicking in it, or
-- moving into it from another window, puts you back in terminal mode (a click
-- used to leave you in Normal mode, so your typing ran as Vim commands).
-- Dragging still selects text to copy.

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
      -- A click (not a drag, which ends in Visual mode) goes back to typing.
      vim.keymap.set("n", "<LeftRelease>", "<LeftRelease><Cmd>startinsert<CR>", { buffer = ev.buf, desc = "Back to typing" })
    end,
  })
  vim.api.nvim_create_autocmd("WinEnter", {
    group = group,
    callback = function()
      if vim.bo.buftype == "terminal" and vim.fn.mode() == "n" and vim.api.nvim_win_get_config(0).relative == "" then
        vim.cmd.startinsert()
      end
    end,
  })
end

return M
