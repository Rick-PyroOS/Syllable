-- Optimize Imports (Ctrl+Alt+O or Space cI), like CLion's: removes the
-- #includes clangd says are unused and adds the ones it says are missing
-- (MissingIncludes in .clangd), after showing the list. All the edits go in
-- as one change, so one undo puts them back.

local M = {}

local CODES = { ["unused-includes"] = "Remove", ["missing-includes"] = "Add" }

function M.run()
  local buf = vim.api.nvim_get_current_buf()
  local client = vim.lsp.get_clients({ bufnr = buf, name = "clangd" })[1]
  if not client then return vim.notify("Optimize Imports needs clangd in this file", vim.log.levels.WARN) end
  local edits, lines = {}, {}
  for _, d in ipairs(vim.diagnostic.get(buf)) do
    local lsp = d.user_data and d.user_data.lsp
    local verb = lsp and CODES[lsp.code]
    local action = verb and lsp.codeActions and lsp.codeActions[1]
    local changes = action and action.edit and (action.edit.changes or {})[vim.uri_from_bufnr(buf)]
    if changes then
      vim.list_extend(edits, changes)
      local what = verb == "Remove" and vim.trim(vim.api.nvim_buf_get_lines(buf, d.lnum, d.lnum + 1, false)[1] or "")
        or vim.trim((changes[1].newText or ""):gsub("\n", " "))
      lines[#lines + 1] = verb .. "  " .. what
    end
  end
  if #edits == 0 then return vim.notify("Includes are already tidy") end
  local choice = vim.fn.confirm("Optimize Imports:\n" .. table.concat(lines, "\n"), "&Apply\n&Cancel", 1, "Question")
  if choice ~= 1 then return end
  vim.lsp.util.apply_text_edits(edits, buf, client.offset_encoding)
end

return M
