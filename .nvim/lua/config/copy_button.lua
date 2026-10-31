-- The copy icon in a popup's top-right corner (hover docs, K / F1, parameter
-- info): click it to copy what the popup says. Every popup that offers
-- copying does it with this icon (the problem popup has its own  Copy
-- buttons).

local M = {}

local LABEL = "  Copy "
local CLICK, RELEASE = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
local ate_click = false

-- The popup's text without the markdown marks (fences, backticks, bold).
local function text_of(win)
  local out = {}
  for _, l in ipairs(vim.api.nvim_buf_get_lines(vim.api.nvim_win_get_buf(win), 0, -1, false)) do
    if not l:match("^%s*```") then
      out[#out + 1] = (l:gsub("`", ""):gsub("%*%*", ""):gsub("^#+%s*", ""))
    end
  end
  return vim.trim(table.concat(out, "\n"))
end

-- Put the icon on a bordered popup.
function M.add(win)
  local cfg = vim.api.nvim_win_get_config(win)
  if cfg.relative == "" or not cfg.border or cfg.border == "none" then return end
  vim.api.nvim_win_set_config(win, { title = { { LABEL, "RickCopyButton" } }, title_pos = "right" })
  vim.w[win].rick_copy = true
end

-- Is the mouse on the icon? It sits on the top border, just left of the corner.
local function on_icon(m)
  if m.winid == 0 or not vim.api.nvim_win_is_valid(m.winid) or not vim.w[m.winid].rick_copy or m.winrow ~= 1 then
    return false
  end
  local total = vim.api.nvim_win_get_width(m.winid) + 2
  local w = vim.fn.strdisplaywidth(LABEL)
  return m.wincol >= total - w and m.wincol <= total - 1
end

function M.setup()
  vim.api.nvim_set_hl(0, "RickCopyButton", { link = "DiagnosticInfo", default = true })
  vim.on_key(function(k)
    if k == RELEASE and ate_click then
      ate_click = false
      return ""
    end
    if k ~= CLICK then return end
    local m = vim.fn.getmousepos()
    if not on_icon(m) then return end
    ate_click = true
    local win = m.winid
    vim.schedule(function()
      if not vim.api.nvim_win_is_valid(win) then return end
      vim.fn.setreg("+", text_of(win))
      vim.notify("Copied")
    end)
    return ""
  end, vim.api.nvim_create_namespace("rick_copy_button"))
end

return M
