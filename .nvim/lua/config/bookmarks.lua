-- Bookmarks, like CLion's: F3 marks or unmarks the current line (a bookmark
-- in the margin), Cmd+F3 or Space fB lists them across files. They move with
-- your edits and are remembered (stdpath("state")/bookmarks.json).
-- TODO list (Space ft): every TODO / FIXME / XXX / HACK comment in the project.

local M = {}
local ns = vim.api.nvim_create_namespace("rick_bookmarks")
local file = vim.fn.stdpath("state") .. "/bookmarks.json"
local SIGN = vim.g.rick_margin_letters and "B" or ""

local store = (function()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end)()

local function write()
  pcall(vim.fn.mkdir, vim.fs.dirname(file), "p")
  pcall(vim.fn.writefile, { vim.json.encode(store) }, file)
end

local function path_of(buf) local p = vim.api.nvim_buf_get_name(buf); return p ~= "" and p or nil end

local function mark(buf, row)
  vim.api.nvim_buf_set_extmark(buf, ns, row, 0, { sign_text = SIGN, sign_hl_group = "DiagnosticInfo", priority = 7 })
end

-- Rows from the extmarks, which follow edits; stored 1-based.
local function sync(buf)
  local path = path_of(buf)
  if not path or not vim.api.nvim_buf_is_loaded(buf) then return end
  local rows = {}
  for _, m in ipairs(vim.api.nvim_buf_get_extmarks(buf, ns, 0, -1, {})) do rows[#rows + 1] = m[2] + 1 end
  store[path] = #rows > 0 and rows or nil
end

function M.toggle()
  local buf, row = vim.api.nvim_get_current_buf(), vim.fn.line(".") - 1
  if not path_of(buf) then return end
  local here = vim.api.nvim_buf_get_extmarks(buf, ns, { row, 0 }, { row, -1 }, {})
  if #here > 0 then
    for _, m in ipairs(here) do vim.api.nvim_buf_del_extmark(buf, ns, m[1]) end
  else
    mark(buf, row)
  end
  sync(buf)
  write()
end

function M.list()
  for _, b in ipairs(vim.api.nvim_list_bufs()) do sync(b) end
  local items = {}
  for path, rows in pairs(store) do
    for _, r in ipairs(rows) do
      local line = ""
      local b = vim.fn.bufnr(path)
      if b > 0 and vim.api.nvim_buf_is_loaded(b) then
        line = (vim.api.nvim_buf_get_lines(b, r - 1, r, false)[1] or "")
      elseif vim.fn.filereadable(path) == 1 then
        line = vim.fn.readfile(path, "", r)[r] or ""
      end
      items[#items + 1] = { file = path, pos = { r, 0 }, text = vim.fn.fnamemodify(path, ":t") .. " " .. line,
        line = vim.trim(line) }
    end
  end
  if #items == 0 then return vim.notify("No bookmarks yet: F3 marks the current line") end
  table.sort(items, function(a, b) return a.file == b.file and a.pos[1] < b.pos[1] or a.file < b.file end)
  require("config.dialogs").open("bookmarks", function()
    return Snacks.picker({
      title = "Bookmarks",
      items = items,
      sort = { fields = { "idx" } },
      format = function(item)
        return {
          { vim.fn.fnamemodify(item.file, ":t") .. ":" .. item.pos[1], "SnacksPickerFile" },
          { "  " .. item.line },
        }
      end,
    })
  end)
end

function M.todo()
  require("config.dialogs").open("todo", function()
    return Snacks.picker.grep({ title = "TODO", search = [[\b(TODO|FIXME|XXX|HACK)\b]], regex = true, live = false })
  end)
end

function M.setup()
  local group = vim.api.nvim_create_augroup("rick_bookmarks", { clear = true })
  vim.api.nvim_create_autocmd("BufReadPost", {
    group = group,
    callback = function(ev)
      local rows = store[path_of(ev.buf) or ""]
      if not rows then return end
      local n = vim.api.nvim_buf_line_count(ev.buf)
      for _, r in ipairs(rows) do if r <= n then mark(ev.buf, r - 1) end end
    end,
  })
  vim.api.nvim_create_autocmd({ "BufWritePost", "BufUnload" }, { group = group, callback = function(ev) sync(ev.buf) write() end })
  vim.api.nvim_create_autocmd("VimLeavePre", {
    group = group,
    callback = function() for _, b in ipairs(vim.api.nvim_list_bufs()) do sync(b) end write() end,
  })
  vim.keymap.set("n", "<F3>", M.toggle, { desc = "Toggle bookmark" })
  vim.keymap.set("n", "<D-F3>", M.list, { desc = "Show bookmarks" })
  vim.keymap.set("n", "<leader>fB", M.list, { desc = "Show bookmarks" })
  vim.keymap.set("n", "<leader>ft", M.todo, { desc = "TODO list" })
end

return M
