-- Go to definition that offers to create a missing one, like CLion's
-- "Create definition": when a function is only declared (clangd's
-- definition is the declaration itself, ending in ";"), Cmd+B / gd / Ctrl+click
-- go to the declaration as before and then offer to create the body in the
-- matching source file (clangd's header/source switch, else the same file).
-- It goes right after the definition of the member declared just before it
-- (or before the one declared just after it), else at the end of the file,
-- and is formatted with the project's .clang-format.

local M = {}

local SKIP_SPECIFIERS = { virtual = true, static = true, explicit = true, inline = true, friend = true }

local function node_text(node, buf) return vim.treesitter.get_node_text(node, buf) end

-- The declaration statement (field_declaration / declaration) whose
-- declarator holds the name at row, col in buf, and the function_declarator.
local function declaration_at(buf, row, col)
  local ok, parser = pcall(vim.treesitter.get_parser, buf)
  if not ok or not parser then return end
  parser:parse()
  local node = vim.treesitter.get_node({ bufnr = buf, pos = { row, col }, ignore_injections = true })
  local fdecl
  while node do
    local t = node:type()
    if t == "function_declarator" and not fdecl then fdecl = node end
    if t == "function_definition" then return end -- has a body: a real definition
    if (t == "field_declaration" or t == "declaration") and fdecl then return node, fdecl end
    node = node:parent()
  end
end

-- The enclosing classes and namespaces of node, outermost first.
local function scopes(node, buf)
  local classes, namespaces = {}, {}
  local p = node:parent()
  while p do
    local t = p:type()
    if t == "class_specifier" or t == "struct_specifier" then
      local name = p:field("name")[1]
      if name then table.insert(classes, 1, node_text(name, buf)) end
    elseif t == "namespace_definition" then
      local name = p:field("name")[1]
      if name then table.insert(namespaces, 1, node_text(name, buf)) end
    end
    p = p:parent()
  end
  return classes, namespaces
end

-- The definition's first line(s), from the declaration: no virtual/static/...,
-- no default arguments, no override/final or "= 0", the name qualified.
local function signature(decl, fdecl, buf, qualifier)
  local sr, sc = decl:range()
  local drop = {} -- byte ranges (absolute rows/cols) to leave out
  for child in decl:iter_children() do
    local t = child:type()
    if t == "virtual" or t == "explicit_function_specifier"
      or (t == "storage_class_specifier" and SKIP_SPECIFIERS[node_text(child, buf)]) then
      table.insert(drop, { child:range() })
    end
  end
  for child in fdecl:iter_children() do
    if child:type() == "virtual_specifier" then table.insert(drop, { child:range() }) end
  end
  local params = fdecl:field("parameters")[1]
  if params then
    for p in params:iter_children() do
      if p:type() == "optional_parameter_declaration" then
        local d, v = p:field("declarator")[1], p:field("default_value")[1]
        if d and v then
          local _, _, dr, dc = d:range()
          local _, _, vr, vc = v:range()
          table.insert(drop, { dr, dc, vr, vc })
        end
      end
    end
  end
  -- The text from the start of the declaration to the end of the declarator.
  local _, _, er, ec = fdecl:range()
  local lines = vim.api.nvim_buf_get_text(buf, sr, sc, er, ec, {})
  -- Apply the drops from last to first, in coordinates relative to (sr, sc).
  table.sort(drop, function(a, b) return a[1] > b[1] or (a[1] == b[1] and a[2] > b[2]) end)
  for _, d in ipairs(drop) do
    local r1, c1, r2, c2 = d[1] - sr, d[2] - (d[1] == sr and sc or 0), d[3] - sr, d[4] - (d[3] == sr and sc or 0)
    if r1 >= 0 and r2 < #lines then
      local before, after = lines[r1 + 1]:sub(1, c1), lines[r2 + 1]:sub(c2 + 1)
      for _ = r1 + 1, r2 + 1 do table.remove(lines, r1 + 1) end
      table.insert(lines, r1 + 1, before .. after)
    end
  end
  local text = table.concat(lines, "\n"):gsub("^%s+", ""):gsub("[ \t]+\n", "\n"):gsub("  +", " "):gsub("%s+$", "")
  -- Qualify the name: the first identifier-ish name right before "(".
  if qualifier ~= "" then
    local name = node_text(fdecl:field("declarator")[1], buf)
    local s, e = text:find(name, 1, true)
    if s then text = text:sub(1, s - 1) .. qualifier .. text:sub(s) end
  end
  return vim.split(text, "\n")
end

-- Where the body goes in the target buffer: the line after the definition
-- of a sibling declared before it, or before one declared after it.
local function placement(decl, buf, target, qualifier)
  local siblings, mine = {}, nil
  local parent = decl:parent()
  if parent then
    for child in parent:iter_children() do
      if child:equal(decl) then mine = #siblings + 1 end
      if child:type() == "field_declaration" or child:type() == "declaration" then
        local fd
        for d in child:iter_children() do
          if d:type() == "function_declarator" then fd = d end
        end
        if fd and fd:field("declarator")[1] then table.insert(siblings, node_text(fd:field("declarator")[1], buf)) end
      end
    end
  end
  local lines = vim.api.nvim_buf_get_lines(target, 0, -1, false)
  local function find_def(name)
    local pat = vim.pesc(qualifier .. name) .. "%s*%("
    for i, l in ipairs(lines) do
      if l:find(pat) and not l:find(";%s*$") then return i end
    end
  end
  local function def_end(row1)
    local ok, parser = pcall(vim.treesitter.get_parser, target)
    if not ok or not parser then return end
    parser:parse()
    local n = vim.treesitter.get_node({ bufnr = target, pos = { row1 - 1, #lines[row1] > 0 and #lines[row1] - 1 or 0 } })
    while n and n:type() ~= "function_definition" do n = n:parent() end
    if n then
      local _, _, er = n:range()
      return er + 1
    end
  end
  if mine then
    for i = mine - 1, 1, -1 do
      local row = find_def(siblings[i])
      if row then return def_end(row) or row, siblings[i], "after" end
    end
    for i = mine + 1, #siblings do
      local row = find_def(siblings[i])
      if row then return row - 1, siblings[i], "before" end
    end
  end
  return #lines, nil, "end"
end

-- clangd's matching header/source for buf, as a path (or nil).
local function other_file(buf, client, cb)
  if not client then return cb(nil) end
  client:request("textDocument/switchSourceHeader", { uri = vim.uri_from_bufnr(buf) }, function(err, uri)
    cb((not err and uri and uri ~= "") and vim.uri_to_fname(uri) or nil)
  end, buf)
end

local function create(buf, decl, fdecl, client)
  local classes, namespaces = scopes(decl, buf)
  local qualifier = #classes > 0 and table.concat(classes, "::") .. "::" or ""
  local name = node_text(fdecl:field("declarator")[1], buf)
  local header = vim.api.nvim_buf_get_name(buf):match("%.h[hpx]*$") ~= nil
  local function offer(path)
    local target_name = vim.fn.fnamemodify(path, ":t")
    local choices = { "Create " .. qualifier .. name .. "() in " .. target_name, "Not now" }
    vim.ui.select(choices, { prompt = qualifier .. name .. "() has no definition" }, function(choice)
      if choice ~= choices[1] then return end
      vim.cmd("normal! m'")
      vim.cmd.edit(vim.fn.fnameescape(path))
      local target = vim.api.nvim_get_current_buf()
      -- Namespaces: qualify too, unless the file opens them or uses them.
      local q = qualifier
      local text = table.concat(vim.api.nvim_buf_get_lines(target, 0, -1, false), "\n")
      for i = #namespaces, 1, -1 do
        local ns = namespaces[i]
        if not text:find("namespace%s+" .. vim.pesc(ns) .. "[%s;{]") then q = ns .. "::" .. q end
      end
      local sig = signature(decl, fdecl, buf, q)
      local row = select(1, placement(decl, buf, target, q))
      -- Braces the way the file already has them: on their own line, or not.
      local own_line = text:find("\n{%s*\n") ~= nil or not text:find("%)%s*[%w ]*{%s*\n")
      local indent = vim.bo[target].expandtab and string.rep(" ", vim.fn.shiftwidth()) or "\t"
      local body = vim.list_extend({ "" }, sig)
      if own_line then
        vim.list_extend(body, { "{", indent, "}" })
      else
        body[#body] = body[#body] .. " {"
        vim.list_extend(body, { indent, "}" })
      end
      local inside = row + #body - 1 -- the indented line between the braces
      local next_line = vim.api.nvim_buf_get_lines(target, row, row + 1, false)[1]
      if next_line and next_line:match("%S") then table.insert(body, "") end
      vim.api.nvim_buf_set_lines(target, row, row, false, body)
      -- The cursor inside the braces, typing.
      vim.api.nvim_win_set_cursor(0, { inside, #indent })
      vim.cmd("startinsert!")
    end)
  end
  if header then
    other_file(buf, client, function(path) vim.schedule(function() offer(path or vim.api.nvim_buf_get_name(buf)) end) end)
  else
    offer(vim.api.nvim_buf_get_name(buf))
  end
end

-- After the jump: is the cursor on a function that's only declared?
local function check(client)
  local buf = vim.api.nvim_get_current_buf()
  local pos = vim.api.nvim_win_get_cursor(0)
  local decl, fdecl = declaration_at(buf, pos[1] - 1, pos[2])
  if not decl then return end
  local text = node_text(decl, buf)
  if text:find("=%s*0%s*;%s*$") or text:find("=%s*default%s*;") or text:find("=%s*delete%s*;") then return end
  create(buf, decl, fdecl, client)
end

function M.definition()
  local buf = vim.api.nvim_get_current_buf()
  local client = vim.lsp.get_clients({ bufnr = buf, method = "textDocument/definition" })[1]
  if not client then return vim.lsp.buf.definition() end
  local params = vim.lsp.util.make_position_params(0, client.offset_encoding)
  client:request("textDocument/definition", params, function(err, result)
    if err or not result or (vim.islist(result) and #result == 0) then
      return vim.notify("No definition found", vim.log.levels.INFO)
    end
    local loc = vim.islist(result) and result[1] or result
    vim.cmd("normal! m'")
    vim.lsp.util.show_document(loc, client.offset_encoding, { focus = true })
    vim.schedule(function() check(client) end)
  end, buf)
end

return M
