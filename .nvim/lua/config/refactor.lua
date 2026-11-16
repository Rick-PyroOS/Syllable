-- Refactor This, like CLion's (Ctrl+T, or right-click > Refactor This…): one
-- list of the refactorings that apply where the cursor is, or to the selected
-- code. Rename is always first; the rest come from the language server
-- (clangd): Extract Variable and Extract Function on a selection, and where
-- they apply Define Outline / Inline, Swap If Branches, Expand Macro,
-- Expand auto, Populate Switch, Add Using and so on. Pick one with the mouse
-- or arrows and Enter; Esc closes.

local M = {}

local function params(client, buf, visual)
  local enc = client.offset_encoding or "utf-16"
  local p
  if visual then
    local s, e = vim.fn.getpos("v"), vim.fn.getpos(".")
    if s[2] > e[2] or (s[2] == e[2] and s[3] > e[3]) then s, e = e, s end
    -- Linewise selection: whole lines.
    if vim.fn.mode() == "V" then
      s[3] = 1
      e[3] = #vim.fn.getline(e[2]) + 1
    end
    p = vim.lsp.util.make_given_range_params({ s[2], s[3] - 1 }, { e[2], e[3] - 1 }, buf, enc)
  else
    p = vim.lsp.util.make_range_params(0, enc)
  end
  p.context = { diagnostics = {}, only = { "refactor" }, triggerKind = 1 }
  return p
end

local function apply(client, action, buf)
  -- clangd names what it extracts "placeholder" (a variable) or "extracted"
  -- (a function): put the cursor on the new name and ask for a real one, as
  -- CLion does.
  local function name_it()
    local word = action.title:find("function") and "extracted" or "placeholder"
    vim.schedule(function()
      if vim.api.nvim_get_current_buf() ~= buf then return end
      if vim.fn.search("\\<" .. word .. "\\>", "bcw") > 0 then vim.lsp.buf.rename() end
    end)
  end
  local function run(a)
    if a.edit then vim.lsp.util.apply_workspace_edit(a.edit, client.offset_encoding or "utf-16") end
    local cmd = type(a.command) == "table" and a.command or (type(a.command) == "string" and a or nil)
    if cmd and cmd.command then client:exec_cmd(cmd, { bufnr = buf }) end
    if action.title:find("^Extract") then
      -- A command's edit comes back from the server a moment later.
      if cmd and cmd.command and not a.edit then vim.defer_fn(name_it, 300) else name_it() end
    end
  end
  if not action.edit and not action.command and client:supports_method("codeAction/resolve") then
    client:request("codeAction/resolve", action, function(err, resolved)
      if err then return vim.notify("Refactoring failed: " .. err.message, vim.log.levels.ERROR) end
      run(resolved)
    end, buf)
  else
    run(action)
  end
end

-- What the list shows for a code action title: Title Case, as in CLion.
local function label(title)
  return (title:gsub("^%l", string.upper))
end

function M.open()
  local buf = vim.api.nvim_get_current_buf()
  local visual = vim.fn.mode():match("^[vV\22]") ~= nil
  local clients = vim.lsp.get_clients({ bufnr = buf, method = "textDocument/codeAction" })
  local items = { { text = "Rename…", key = "Shift+F6", run = vim.lsp.buf.rename } }
  local function show()
    if visual then vim.cmd("normal! \27") end
    if #items == 1 and #clients > 0 then
      items[#items + 1] = { text = visual and "(nothing else applies to this selection)"
        or "(select code to extract a variable or function)", run = function() end }
    end
    vim.ui.select(items, {
      prompt = "Refactor This",
      format_item = function(it) return it.text .. (it.key and ("   " .. it.key) or "") end,
    }, function(it) if it then it.run() end end)
  end
  if #clients == 0 then return show() end
  local left = #clients
  for _, client in ipairs(clients) do
    client:request("textDocument/codeAction", params(client, buf, visual), function(_, result)
      for _, a in ipairs(result or {}) do
        -- clangd's "Show AST" style debugging tweaks are kind "info".
        if not (a.kind and a.kind:find("^info")) and not a.disabled then
          items[#items + 1] = { text = label(a.title), run = function() apply(client, a, buf) end }
        end
      end
      left = left - 1
      if left == 0 then vim.schedule(show) end
    end, buf)
  end
end

function M.setup()
  vim.keymap.set({ "n", "x" }, "<C-t>", M.open, { desc = "Refactor This" })
  vim.keymap.set({ "n", "x" }, "<leader>cR", M.open, { desc = "Refactor This" })
  vim.cmd([[anoremenu PopUp.Refactor\ This… <Cmd>lua require("config.refactor").open()<CR>]])
end

return M
