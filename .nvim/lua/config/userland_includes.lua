-- Keeps clangd from adding kernel headers to userland code in the Syllable tree.
--
-- The kernel's dbprintf (kernel/kdebug.h) and userland's (atheos/kdebug.h) are
-- both extern "C" functions with the same name, so clangd's index treats them
-- as one symbol and offers whichever header is included most: the kernel's.
-- Accepting dbprintf in a GUI app then added #include <kernel/kdebug.h>, whose
-- declarations (dbprintf(int nPort, ...), kernel types) don't belong there.
--
-- In a file outside the kernel (the folders tools/gen_compile_commands.py
-- builds with the kernel's flags), an #include <kernel/X.h> clangd suggests is
-- changed to <atheos/X.h> when that header exists, and dropped when it
-- doesn't. That covers completion (the include added on accept), the quick
-- fixes on an error ("Include <...> for symbol ..."), and code actions such as
-- Optimize Includes. Kernel code is left alone. Projects that aren't the
-- Syllable tree (no system/sys/include/syllable/kernel) aren't touched.

local M = {}

local SYS_INC = "system/sys/include/syllable"
-- Same list as KERNEL_DIRS in tools/gen_compile_commands.py.
local KERNEL_DIRS = { "system/sys/kernel/", "misc/drivers/", SYS_INC .. "/kernel/" }

local function is_dir(p) local st = vim.uv.fs_stat(p); return st ~= nil and st.type == "directory" end
local function is_file(p) local st = vim.uv.fs_stat(p); return st ~= nil and st.type == "file" end

-- The Syllable root a file is under, or nil (remembered per folder).
local roots = {}
local function syllable_root(path)
  local dir = vim.fs.dirname(path)
  if roots[dir] == nil then
    roots[dir] = vim.fs.root(path, function(name, parent)
      return name == "system" and is_dir(vim.fs.joinpath(parent, SYS_INC, "kernel"))
    end) or false
  end
  return roots[dir] or nil
end

local function is_kernel(path, root)
  local rel = path:sub(#root + 2)
  for _, d in ipairs(KERNEL_DIRS) do
    if rel:sub(1, #d) == d then return true end
  end
  return false
end

-- For a file that needs fixing: { root = ..., buf = ... }; nil otherwise.
local function context_for(uri)
  if type(uri) ~= "string" or not uri:find("^file:") then return nil end
  local path = vim.fs.normalize(vim.uri_to_fname(uri))
  local root = syllable_root(path)
  if not root or is_kernel(path, root) then return nil end
  return { root = root, buf = vim.uri_to_bufnr(uri) }
end

local function already_included(ctx, header)
  if not vim.api.nvim_buf_is_loaded(ctx.buf) then return false end
  local pat = "^%s*#%s*include%s*[<\"]" .. vim.pesc(header) .. "[>\"]"
  for _, line in ipairs(vim.api.nvim_buf_get_lines(ctx.buf, 0, 300, false)) do
    if line:find(pat) then return true end
  end
  return false
end

-- The userland header for kernel/X.h, or nil to drop it.
local function replacement(ctx, name)
  local header = "atheos/" .. name
  if not is_file(vim.fs.joinpath(ctx.root, SYS_INC, header)) then return nil end
  return header
end

-- Rewrites one text: "kernel/X.h" -> "atheos/X.h". Returns the new text and
-- whether it named a kernel header that has no userland one (or one that's
-- already included), meaning the edit should go.
local function rewrite(ctx, text)
  local drop = false
  local out = text:gsub("([<\"])kernel/([%w_%-%./]+%.h)([>\"])", function(open, name, close)
    local header = replacement(ctx, name)
    if not header or already_included(ctx, header) then
      drop = true
      return nil
    end
    return "<" .. header .. ">"
  end)
  return out, drop
end

-- Only #include lines are changed; returns the edits to keep.
local function fix_edits(ctx, edits)
  if type(edits) ~= "table" then return edits end
  local kept = {}
  for _, e in ipairs(edits) do
    local text = e.newText
    if type(text) == "string" and text:find("#%s*include") and text:find("kernel/", 1, true) then
      local new, drop = rewrite(ctx, text)
      if not drop then
        e.newText = new
        kept[#kept + 1] = e
      end
    else
      kept[#kept + 1] = e
    end
  end
  return kept
end

local function fix_title(ctx, s)
  if type(s) ~= "string" or not s:find("kernel/", 1, true) then return s end
  return (rewrite(ctx, s))
end

-- A completion item: fix (or remove) the #include it adds on accept.
local function fix_item(ctx, item)
  if type(item) ~= "table" or not item.additionalTextEdits then return end
  local before = #item.additionalTextEdits
  item.additionalTextEdits = fix_edits(ctx, item.additionalTextEdits)
  if #item.additionalTextEdits < before then
    -- No #include any more: drop the "adds an include" dot (--header-insertion-decorators).
    if type(item.label) == "string" then item.label = item.label:gsub("^•", " ") end
    if #item.additionalTextEdits == 0 then item.additionalTextEdits = nil end
  end
  local doc = item.documentation
  if type(doc) == "table" then doc.value = fix_title(ctx, doc.value)
  elseif type(doc) == "string" then item.documentation = fix_title(ctx, doc) end
end

-- A WorkspaceEdit; returns false when nothing is left of it.
local function fix_workspace_edit(we)
  if type(we) ~= "table" then return true end
  local any = false
  for uri, edits in pairs(we.changes or {}) do
    local ctx = context_for(uri)
    if ctx then we.changes[uri] = fix_edits(ctx, edits) end
    if #we.changes[uri] > 0 then any = true end
  end
  for _, dc in ipairs(we.documentChanges or {}) do
    local ctx = dc.textDocument and context_for(dc.textDocument.uri)
    if ctx and dc.edits then dc.edits = fix_edits(ctx, dc.edits) end
    if not dc.edits or #dc.edits > 0 then any = true end
  end
  return any
end

-- A list of code actions; drops the ones left with nothing to do, and a
-- rewritten "Include <atheos/X.h>" that repeats the one clangd also offered.
local function fix_actions(ctx, actions)
  if type(actions) ~= "table" then return actions end
  local kept, seen = {}, {}
  for _, a in ipairs(actions) do
    if type(a) == "table" and a.edit then
      if fix_workspace_edit(a.edit) then
        a.title = fix_title(ctx, a.title)
        local key = type(a.title) == "string" and a.title:gsub("[<>\"]", "") or a
        if not seen[key] then
          seen[key] = true
          kept[#kept + 1] = a
        end
      end
    else
      kept[#kept + 1] = a
    end
  end
  return kept
end

local function fix_response(method, result, uri)
  local ctx = context_for(uri)
  if not ctx or result == nil then return result end
  if method == "textDocument/completion" then
    for _, item in ipairs(result.items or result) do fix_item(ctx, item) end
  elseif method == "completionItem/resolve" then
    fix_item(ctx, result)
  elseif method == "textDocument/codeAction" then
    result = fix_actions(ctx, result)
  end
  return result
end

local WATCHED = { ["textDocument/completion"] = true, ["completionItem/resolve"] = true, ["textDocument/codeAction"] = true }

-- Exposed for tests.
M._fix_response = fix_response
M._fix_diagnostics = function(res)
  local ctx = res and context_for(res.uri)
  if not ctx then return end
  for _, d in ipairs(res.diagnostics or {}) do
    if d.codeActions then d.codeActions = fix_actions(ctx, d.codeActions) end
  end
end

-- From lsp.lua's clangd on_init: filter what clangd sends back.
function M.attach(client)
  if client._rick_userland_includes then return end
  client._rick_userland_includes = true

  -- Requests (completion, code actions): fix the result before the handler
  -- sees it. Works for client:request(...) and the older client.request(...).
  local orig = client.request
  client.request = function(...)
    local n = select("#", ...)
    local args = { ... }
    local i = args[1] == client and 2 or 1
    local method, params = args[i], args[i + 1]
    if WATCHED[method] then
      local handler = args[i + 2]
      -- completionItem/resolve has no document; the buffer it's for is the current one.
      local uri = type(params) == "table" and params.textDocument and params.textDocument.uri
        or vim.uri_from_bufnr(args[i + 3] or vim.api.nvim_get_current_buf())
      args[i + 2] = function(err, result, hctx, config)
        if not err then
          local ok, fixed = pcall(fix_response, method, result, uri)
          if ok then result = fixed end
        end
        local h = handler or client.handlers[method] or vim.lsp.handlers[method]
        if h then return h(err, result, hctx, config) end
      end
      n = math.max(n, i + 2)
    end
    return orig(unpack(args, 1, n))
  end

  -- The quick fixes clangd sends with each error (codeActionsInline in lsp.lua).
  local m = "textDocument/publishDiagnostics"
  local prev = client.handlers[m]
  client.handlers[m] = function(err, res, hctx, config)
    pcall(M._fix_diagnostics, res)
    local h = prev or vim.lsp.handlers[m]
    if h then return h(err, res, hctx, config) end
  end
end

return M
