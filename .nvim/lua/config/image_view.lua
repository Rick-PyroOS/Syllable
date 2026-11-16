-- Images open in the editor, like CLion's image viewer:
--   * terminals that can draw real images (kitty graphics: Ghostty, kitty,
--     WezTerm) get snacks.nvim's viewer, pixel for pixel, and inline images in
--     Markdown;
--   * other terminals (iTerm2) get a colour preview drawn with chafa
--     (brew install chafa), redrawn to fit when the window changes size;
--   * without either, or for PDFs, the file opens in Preview as before.
-- Both are centered in the window. In the viewer, Enter or o opens the file in
-- Preview, q closes the tab.
-- Space tI switches the in-editor view off (remembered): everything goes to Preview.

local M = {}

function M.kitty()
  local e = vim.env
  return e.KITTY_WINDOW_ID ~= nil or e.TERM == "xterm-kitty" or e.TERM_PROGRAM == "ghostty"
    or e.GHOSTTY_RESOURCES_DIR ~= nil or e.TERM_PROGRAM == "WezTerm" or e.WEZTERM_EXECUTABLE ~= nil
end

local function chafa() return vim.fn.executable("chafa") == 1 end

-- snacks.nvim's viewer draws the image at the top left; center it in the
-- window instead: blank lines above it, and its column moved right.
local patched = false
function M.center_snacks()
  if patched then return end
  patched = true
  local P = require("snacks.image.placement")
  local state = P.state
  P.state = function(self)
    local s = state(self)
    if self.opts.inline or not vim.api.nvim_buf_is_valid(self.buf) or vim.bo[self.buf].filetype ~= "image"
      or #s.wins == 0 or s.loc.width ~= s.loc.width then -- (NaN: size not known yet)
      return s
    end
    local win = s.wins[1]
    local top = math.max(0, math.floor((vim.api.nvim_win_get_height(win) - s.loc.height) / 2))
    local left = math.max(0, math.floor((vim.api.nvim_win_get_width(win) - s.loc.width) / 2))
    -- The image's own line holds blanks up to its column: an image one row high
    -- is drawn inline in the text, which can't start past the end of the line.
    local want = vim.split(string.rep("\n", top), "\n")
    want[#want] = string.rep(" ", left)
    local have = vim.api.nvim_buf_get_lines(self.buf, 0, -1, false)
    if #have ~= #want or have[#have] ~= want[#want] then
      vim.bo[self.buf].modifiable = true
      vim.api.nvim_buf_set_lines(self.buf, 0, -1, false, want)
      vim.bo[self.buf].modifiable, vim.bo[self.buf].modified = false, false
      vim.api.nvim_win_call(win, function() vim.fn.winrestview({ topline = 1, lnum = 1 }) end)
    end
    self.opts.pos = { top + 1, left }
    s.loc[1], s.loc[2] = top + 1, left
    return s
  end
end

-- How this image should open: "snacks", "chafa" or "preview".
function M.mode(path)
  local ext = vim.fn.fnamemodify(path, ":e"):lower()
  if ext == "pdf" or not require("config.prefs").get("images_inline") then return "preview" end
  if M.kitty() then return "snacks" end
  if chafa() and ext ~= "icns" and ext ~= "heic" then return "chafa" end
  return "preview"
end

-- Width and height from the file header (PNG, GIF, BMP, JPEG).
local function u16be(s, i) return s:byte(i) * 256 + s:byte(i + 1) end
local function u16le(s, i) return s:byte(i) + s:byte(i + 1) * 256 end
local function u32be(s, i) return ((s:byte(i) * 256 + s:byte(i + 1)) * 256 + s:byte(i + 2)) * 256 + s:byte(i + 3) end
function M.dimensions(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local s = f:read(65536) or ""
  f:close()
  if s:sub(1, 8) == "\137PNG\r\n\26\n" and #s >= 24 then return u32be(s, 17), u32be(s, 21) end
  if s:sub(1, 3) == "GIF" and #s >= 10 then return u16le(s, 7), u16le(s, 9) end
  if s:sub(1, 2) == "BM" and #s >= 26 then return u16le(s, 19) + u16le(s, 21) * 65536, math.abs(u16le(s, 23)) end
  if s:sub(1, 2) == "\255\216" then
    local i = 3
    while i + 9 <= #s do
      if s:byte(i) ~= 0xFF then return nil end
      local marker = s:byte(i + 1)
      if marker >= 0xC0 and marker <= 0xCF and marker ~= 0xC4 and marker ~= 0xC8 and marker ~= 0xCC then
        return u16be(s, i + 7), u16be(s, i + 5)
      end
      i = i + 2 + u16be(s, i + 2)
    end
  end
end

local function human(bytes)
  if bytes < 1024 then return bytes .. " B" end
  if bytes < 1024 * 1024 then return ("%.1f KB"):format(bytes / 1024) end
  return ("%.1f MB"):format(bytes / 1024 / 1024)
end

local function open_in_viewer(path)
  local _, err = vim.ui.open(path)
  if err then vim.notify("Couldn't open " .. vim.fn.fnamemodify(path, ":t") .. ": " .. err, vim.log.levels.WARN) end
end
M.open_in_viewer = open_in_viewer

-- Draw the chafa preview into buf's terminal, sized to the window showing it.
local function draw(buf)
  local b = vim.b[buf]
  if not b.rick_image_chan then return end
  local win = vim.fn.bufwinid(buf)
  if win == -1 then return end
  local path = b.rick_image_path
  local W, H = vim.api.nvim_win_get_width(win) - 1, vim.api.nvim_win_get_height(win) - 3
  local w, h = M.dimensions(path)
  local stat = vim.uv.fs_stat(path)
  local info = vim.fn.fnamemodify(path, ":t") .. "   " .. (w and (w .. " × " .. h .. " px   ") or "")
    .. vim.fn.fnamemodify(path, ":e"):upper() .. "   " .. (stat and human(stat.size) or "")
  local hint = "Enter: open in Preview   q: close"
  local out = vim.fn.system({ "chafa", "--format", "symbols", "--colors", "full", "--animate", "off",
    "--polite", "on", "--size", ("%dx%d"):format(math.max(W, 10), math.max(H, 4)), path })
  if vim.v.shell_error ~= 0 then out = "chafa couldn't read this image:\r\n" .. out end
  local pad = math.max(2, W - vim.fn.strdisplaywidth(info) - #hint)
  local header = "\27[1m" .. info .. "\27[0m" .. string.rep(" ", pad) .. "\27[2m" .. hint .. "\27[0m"
  -- Centered under the header: chafa's picture keeps the image's shape, so
  -- it's narrower or shorter than the space it was given.
  local lines = vim.split(out:gsub("\r", ""):gsub("\n$", ""), "\n")
  local width = 0
  for _, l in ipairs(lines) do width = math.max(width, vim.fn.strdisplaywidth((l:gsub("\27%[[%d;?]*%a", "")))) end
  local left = string.rep(" ", math.max(0, math.floor((W - width) / 2)))
  local top = string.rep("\r\n", math.max(0, math.floor((H - #lines) / 2)))
  out = top .. left .. table.concat(lines, "\r\n" .. left)
  vim.api.nvim_chan_send(b.rick_image_chan, "\27[H\27[2J\27[3J" .. header .. "\r\n\r\n" .. out)
end

function M.show(buf, path)
  vim.bo[buf].buftype = "nofile"
  vim.bo[buf].swapfile = false
  vim.bo[buf].modifiable = true
  vim.b[buf].rick_image_path = path
  vim.b[buf].rick_image_chan = vim.api.nvim_open_term(buf, {})
  vim.bo[buf].bufhidden = "hide"
  local function k(lhs, fn) vim.keymap.set("n", lhs, fn, { buffer = buf, nowait = true }) end
  k("<CR>", function() open_in_viewer(path) end)
  k("o", function() open_in_viewer(path) end)
  k("q", function() if Snacks then Snacks.bufdelete(buf) else vim.cmd("bdelete " .. buf) end end)
  k("i", "<Nop>") -- stay out of terminal mode; nothing to type here
  k("a", "<Nop>")
  local group = vim.api.nvim_create_augroup("rick_image_" .. buf, { clear = true })
  vim.api.nvim_create_autocmd({ "BufWinEnter", "WinResized", "VimResized" }, {
    group = group,
    callback = function()
      if not vim.api.nvim_buf_is_valid(buf) then return true end
      vim.schedule(function() if vim.api.nvim_buf_is_valid(buf) then draw(buf) end end)
    end,
  })
  vim.api.nvim_create_autocmd("BufWinEnter", {
    group = group, buffer = buf,
    callback = function()
      local win = vim.fn.bufwinid(buf)
      if win ~= -1 then
        vim.wo[win].number, vim.wo[win].relativenumber, vim.wo[win].signcolumn = false, false, "no"
        vim.wo[win].foldcolumn, vim.wo[win].statuscolumn, vim.wo[win].cursorline = "0", "", false
        vim.wo[win].list = false -- no · for the picture's trailing blanks
      end
    end,
  })
  vim.schedule(function()
    local win = vim.fn.bufwinid(buf)
    if win ~= -1 then vim.api.nvim_exec_autocmds("BufWinEnter", { buffer = buf }) end
    draw(buf)
  end)
end

return M
