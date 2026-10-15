-- Opening a binary file (an object file, an executable, an image...) asks
-- first: "foo.o looks like a binary file. Open it anyway?". Cancel and the
-- file isn't read at all; its tab goes away and you're back where you were.
-- A file counts as binary when its first 8 KB contain a zero byte.
-- Images and PDFs skip the question: images open in the editor where the
-- terminal can show them (config/image_view.lua), otherwise they and PDFs open
-- in the Mac's viewer (Preview) and never become a tab. SVGs are text and open as usual.

local M = {}

local viewer_exts = { png = true, jpg = true, jpeg = true, gif = true, bmp = true, tif = true, tiff = true,
  webp = true, ico = true, icns = true, heic = true, pdf = true }

local function is_binary(path)
  local stat = vim.uv.fs_stat(path)
  if not stat or stat.type ~= "file" or stat.size == 0 then return false end
  local f = io.open(path, "rb")
  if not f then return false end
  local head = f:read(8192) or ""
  f:close()
  return head:find("\0", 1, true) ~= nil
end

-- Put another buffer in the windows showing buf, then drop it.
local function drop(buf)
  if not vim.api.nvim_buf_is_valid(buf) then return end
  for _, win in ipairs(vim.fn.win_findbuf(buf)) do
    vim.api.nvim_win_call(win, function()
      local alt = vim.fn.bufnr("#")
      if alt > 0 and alt ~= buf and vim.fn.buflisted(alt) == 1 then
        vim.api.nvim_win_set_buf(win, alt)
      else
        vim.cmd("enew")
      end
    end)
  end
  pcall(vim.api.nvim_buf_delete, buf, { force = true })
end

function M.setup()
  -- BufAdd comes before the file is read, so a cancel can stop the read.
  vim.api.nvim_create_autocmd("BufAdd", {
    group = vim.api.nvim_create_augroup("rick_binary", { clear = true }),
    callback = function(ev)
      local path = vim.api.nvim_buf_get_name(ev.buf)
      if path == "" or not is_binary(path) then return end
      local name = vim.fn.fnamemodify(path, ":t")
      if viewer_exts[vim.fn.fnamemodify(path, ":e"):lower()] then
        local view = require("config.image_view")
        local mode = view.mode(path)
        if mode == "snacks" then return end -- snacks.nvim's image viewer reads it
        if mode == "chafa" then
          vim.api.nvim_create_autocmd("BufReadCmd", { buffer = ev.buf, once = true, callback = function() view.show(ev.buf, path) end })
          return
        end
        vim.api.nvim_create_autocmd("BufReadCmd", { buffer = ev.buf, once = true, callback = function() end })
        vim.schedule(function()
          drop(ev.buf)
          local _, err = vim.ui.open(path)
          if err then vim.notify("Couldn't open " .. name .. ": " .. err, vim.log.levels.WARN) end
        end)
        return
      end
      local choice = vim.fn.confirm(name .. " looks like a binary file. Open it anyway?", "&Open\n&Cancel", 2, "Question")
      if choice == 1 then return end
      -- Reading the buffer runs this instead of loading the file.
      vim.api.nvim_create_autocmd("BufReadCmd", { buffer = ev.buf, once = true, callback = function() end })
      vim.schedule(function() drop(ev.buf) end)
    end,
  })
end

return M
