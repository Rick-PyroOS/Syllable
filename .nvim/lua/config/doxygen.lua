-- Tidy doxygen comments in clangd's hover and completion docs.
--
-- clangd 21 shows a comment's text as written, so doc blocks in the \par
-- style (like Syllable's sources) come out as "\par Description:", "\sa ...",
-- "\author ..." and a row of asterisks. clangd 22 with doxygen mode on (.clangd: Documentation:
-- CommentFormat: Doxygen) lays out \brief, \param and \return itself, but
-- shows the rest as "**\par** `Description:`". This rewrites both into
-- headings and lists before the popup draws. Code blocks are left alone.

local M = {}

-- \return foo  ->  **Returns:** foo
local LABEL = {
  ["return"] = "Returns", returns = "Returns", result = "Returns",
  sa = "See also", see = "See also",
  author = "Author", authors = "Author",
  note = "Note", remark = "Note", remarks = "Note", attention = "Note",
  warning = "Warning", bug = "Bug", todo = "To do",
  since = "Since", deprecated = "Deprecated", version = "Version",
  pre = "Precondition", post = "Postcondition", throw = "Throws", throws = "Throws", exception = "Throws",
}
-- Grouping and bookkeeping commands that mean nothing in a popup.
local DROP = { ingroup = true, defgroup = true, addtogroup = true, weakgroup = true, internal = true,
  file = true, fn = true, f = true, class = true, struct = true, name = true, nosubgrouping = true }

local function unescape(s) return (s:gsub("\\(%p)", "%1")) end

-- Inline commands: \b bold, \p \c code, \e \a \em italic, \ref name.
local function inline(s)
  s = s:gsub("\\\\b%s+(%S+)", "**%1**")
  s = s:gsub("\\\\[pc]%s+(%S+)", function(w) return "`" .. unescape(w) .. "`" end)
  s = s:gsub("\\\\em?%s+(%S+)", "*%1*")
  s = s:gsub("\\\\a%s+(%S+)", "*%1*")
  s = s:gsub("\\\\ref%s+(%S+)", "%1")
  return s
end

-- A line that is only a run of * = - / # ~ (the "*****" end of a comment block).
-- clangd's own section separator is exactly "---", so needs 5 or more.
local function is_banner(line)
  local raw = unescape(line):gsub("%s", "")
  return #raw >= 5 and raw:match("^[%*=%-/#~_]+$") ~= nil
end

-- clangd 22's doxygen mode shows the commands it doesn't lay out itself
-- as "**\par** `Description:` text", and reads the @ in an email address as
-- one ("kurt**\atheos** .cx"). Put those back the way clangd 21 writes them.
local function from_doxygen_mode(md)
  local out, in_fence, internal = {}, false, false
  for line in (md .. "\n"):gmatch("(.-)\n") do
    if line:match("^```") then in_fence = not in_fence end
    -- "\internal" comes out as an empty code block and a stray "**@**".
    if internal and (in_fence or line == "```") then
      if not in_fence then internal = false end
      goto continue
    elseif line:match("^%*%*[\\@]?%*%*$") then
      goto continue
    elseif internal and line ~= "" then
      internal = false
    end
    local cmd, rest
    if not in_fence then
      line = line:gsub("(%w)%*%*[\\@]([%w_]+)%*%*%s?", "%1@%2")   -- kurt@atheos.cx
      line = line:gsub("(%S)%s*%*%*[\\@](%a+)%*%*.*$", function(before, c) -- "path.**\f** status_t ..."
        if DROP[c:lower()] then return before end
      end)
      line = line:gsub("^(%- `[^`]+` %-) %-%s+", "%1 ")           -- "- `int n` - - The n"
      line = line:gsub("^(%-? ?`[^`]+`) %-$", "%1")                -- an empty \return or \param
      cmd, rest = line:match("^%*%*[\\@](%a+)%*%*%s*(.*)$")
      if not cmd then line = line:gsub("`([%w_]+)([%.,;])`", "`%1`%2") end -- \p nFlags. -> `nFlags`.
    end
    if cmd == "internal" then
      internal = true
    elseif cmd then
      local arg, after = rest:match("^`([^`]*)`%s*(.*)$")
      if arg and (cmd == "par" or DROP[cmd:lower()]) then
        table.insert(out, "\\" .. cmd .. " " .. arg)
        if after ~= "" then table.insert(out, (after:gsub("`([%w_]+)([%.,;])`", "`%1`%2"))) end
      else
        table.insert(out, "\\" .. cmd .. " " .. rest:gsub("`([%w_]+)([%.,;])`", "`%1`%2"))
      end
    else
      table.insert(out, line)
    end
    ::continue::
  end
  return table.concat(out, "\n")
end

-- "**** msfs.filesystem/ *****..." -> "msfs.filesystem/": runs of four or
-- more asterisks (escaped or not) at either end of a line.
local function strip_stars(s)
  local function trim(run) return select(2, run:gsub("%*", "")) >= 4 and "" or run end
  s = s:gsub("^(%s*[%*\\]+)%s*", function(run) local t = trim(run); return t == "" and "" or nil end)
  s = s:gsub("%s*([%*\\]+)$", function(run) local t = trim(run); return t == "" and "" or nil end)
  return s
end

-- Remove what's left empty: a "### Brief" with nothing under it (and the rule
-- above it), and old template labels ("NAME:", "SEE ALSO:") left blank.
local function prune(out)
  local function next_text(i)
    for j = i + 1, #out do if out[j] ~= "" then return out[j], j end end
  end
  local res = {}
  for i, l in ipairs(out) do
    local nxt = next_text(i)
    if not (l:match("^%u[%u ]*:$") and (not nxt or nxt == "---" or nxt:match("^### ") or nxt:match("^%u[%u ]*:$"))) then
      table.insert(res, l)
    end
  end
  out, res = res, {}
  for i, l in ipairs(out) do
    local nxt = next_text(i)
    if i > 1 and l:match("^### ") and (not nxt or nxt == "---") then -- (the first is clangd's "### function")
      while res[#res] == "" or res[#res] == "---" do table.remove(res) end
    else
      table.insert(res, l)
    end
  end
  local tidy = {}
  for _, l in ipairs(res) do
    if not (l == "" and (tidy[#tidy] == "" or #tidy == 0)) then table.insert(tidy, l) end
  end
  return tidy
end

function M.render(md)
  if type(md) ~= "string" or not (md:find("\\\\%a") or md:find("%*%*[\\@]%a") or md:find("@%a")
      or md:find("\\%*\\%*\\%*") or md:find("%*%*%*%*%*")) then
    return md
  end
  md = from_doxygen_mode(md)
  local out, in_fence, in_code, last = {}, false, false, nil
  local pending -- a heading with nothing under it yet ("\par Description:", a bare "\sa")
  local function blank() if #out > 0 and out[#out] ~= "" and out[#out] ~= "---" then table.insert(out, "") end end
  local function add(s, kind)
    pending = nil
    if kind ~= last or kind == "section" then blank() end
    table.insert(out, s)
    last = kind
  end

  for line in (md .. "\n"):gmatch("(.-)\n") do
    local text = line:gsub("%s+$", "")   -- drop clangd's "  " hard breaks
    if not in_fence and not in_code and not text:match("^```") and not is_banner(text) then
      text = strip_stars(text)
      if text == "" and line:find("%*%*%*%*") then goto next_line end
    end
    if text:match("^```") then
      if not in_fence and text == "```" then line = "```cpp" end -- a \code block, as clangd 22 shows it
      in_fence = not in_fence
      table.insert(out, line)
      last = nil
    elseif in_fence then
      table.insert(out, line)
    elseif in_code then                  -- inside \code ... \endcode
      if text:match("^%s*[\\@]\\?endcode") then
        table.insert(out, "```"); in_code = false; last = nil
      else
        table.insert(out, unescape(text))
      end
    else
      local cmd, rest = text:match("^%s*\\?\\(%a+)%s*(.*)$")
      if not cmd then cmd, rest = text:match("^%s*@(%a+)%s*(.*)$") end
      local lc = cmd and cmd:lower()
      if lc == "code" then
        blank(); table.insert(out, "```cpp"); in_code = true
      elseif lc and DROP[lc] then
        -- skip
      elseif lc == "brief" or lc == "short" then
        add(inline(rest), "text")
      elseif lc == "par" then
        blank()
        pending = rest ~= "" and "**" .. inline(rest) .. "**" or nil
        last = "par"
      elseif lc == "param" or lc == "tparam" or lc == "prarm" then
        local dir, name, desc = rest:match("^(\\%[[^%]]*\\%])%s*(%S+)%s*(.*)$")
        if not dir then name, desc = rest:match("^(%S+)%s*(.*)$") end
        if name then
          desc = desc:gsub("^\\?%-+%s*", "")   -- "cFrame - The size" -> "The size"
          if last ~= "param" then add(lc == "tparam" and "**Template parameters:**" or "**Parameters:**", "section") end
          table.insert(out, "- `" .. unescape(name) .. "` " .. inline(desc))
          last = "param"
        end
      elseif lc == "retval" then
        local val, desc = rest:match("^(%S+)%s*(.*)$")
        if last ~= "retval" then add("**Return values:**", "section") end
        table.insert(out, "- `" .. unescape(val or "") .. "` " .. inline(desc or ""))
        last = "retval"
      elseif lc and LABEL[lc] then
        if rest ~= "" then
          add("**" .. LABEL[lc] .. ":** " .. inline(rest), "section")
        else
          blank()
          pending = "**" .. LABEL[lc] .. ":**"
        end
        last = "label"
      elseif is_banner(text) then
        -- skip
      elseif cmd then                      -- unknown command: keep its words
        add(inline(cmd .. (rest ~= "" and " " .. rest or "")), "text")
      elseif text == "" then
        if not pending then blank(); last = nil end
      else
        if pending then
          table.insert(out, pending); pending = nil
          table.insert(out, inline(text)); last = "text"
        elseif last == "par" then table.insert(out, inline(text)); last = "text"
        elseif last == "param" or last == "retval" or last == "label" then
          out[#out] = out[#out] .. " " .. inline(text)   -- a wrapped description
        else
          add(inline(text), "text")
        end
      end
    end
    ::next_line::
  end
  if in_code then table.insert(out, "```") end
  out = prune(out)
  -- A "### Details" left with nothing under it, and the rule above it.
  while out[#out] == "" or (out[#out] or ""):match("^### ") or out[#out] == "---" do table.remove(out) end
  return table.concat(out, "\n")
end

local function fix_markup(doc)
  if type(doc) == "table" and doc.kind == "markdown" then doc.value = M.render(doc.value) end
end

local fixers = {
  ["textDocument/hover"] = function(r) fix_markup(r.contents) end,
  ["completionItem/resolve"] = function(r) fix_markup(r.documentation) end,
  ["textDocument/completion"] = function(r)
    for _, item in ipairs(r.items or r) do fix_markup(item.documentation) end
  end,
  ["textDocument/signatureHelp"] = function(r)
    for _, sig in ipairs(r.signatures or {}) do
      fix_markup(sig.documentation)
      for _, p in ipairs(sig.parameters or {}) do fix_markup(p.documentation) end
    end
  end,
}

-- Call from a client's on_init: rewrites the docs in its replies before
-- blink.cmp or the hover window sees them.
function M.attach(client)
  local request = client.request
  client.request = function(self, method, params, handler, ...)
    local fix = fixers[method]
    if fix then
      local inner = handler or client.handlers[method] or vim.lsp.handlers[method]
      handler = function(err, result, ...)
        if result then pcall(fix, result) end
        if inner then return inner(err, result, ...) end
      end
    end
    return request(self, method, params, handler, ...)
  end
end

return M
