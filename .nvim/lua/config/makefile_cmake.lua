-- Convert a Syllable Makefile into a CMakeLists.txt in Syllable's CMake DSL
-- (cmake/Syllable.cmake: syllable_program, syllable_driver, syllable_library,
-- syllable_static_library, syllable_plugin, syllable_resources,
-- syllable_catalogs, syllable_attribute).
--
-- Right-click a Makefile (or a folder with one) in the project tree, or right-click in an open
-- Makefile, > Convert to CMakeLists.txt; or :SyllableMakefileToCMake [file].
-- The menu entries show for Makefiles inside a Syllable checkout (a folder
-- above has cmake/Syllable.cmake). init.lua sets it up, not the checkout's
-- .nvim.lua, so changing it doesn't make Neovim ask to trust .nvim.lua again.
-- A new CMakeLists.txt is written next to the Makefile and opened. If one is
-- already there, you choose: compare it with the converted one (the converted
-- one is on the right; :w there replaces CMakeLists.txt), replace it, or cancel.
--
-- How it reads the Makefile: variables (=, :=, +=, ?=, $(addprefix),
-- $(patsubst), $(VAR:.o=.c) and the other common functions), ifdef/ifndef/
-- ifeq, rules and their recipes, starting from the default goal plus install
-- and dist. From the recipes:
--   - link commands ($(CC)/$(CXX) ... -o x without -c) give the targets:
--     -kernel -> syllable_driver, -plugin -> syllable_plugin, -shared ->
--     syllable_library (SONAME from -soname, else the file name), ar ->
--     syllable_static_library (KERNEL if compiled with -kernel), anything
--     else -> syllable_program;
--   - objects give the SOURCES (an explicit rule's source, else the file
--     next to the Makefile with the same name);
--   - compile flags give INCLUDES, DEFINES and OPTIONS, -l gives LIBS;
--   - cp/install into $(IMAGE)/... or /... give INSTALL (and OUTPUT_NAME when
--     the copy renames it), strip gives STRIP, ln -s gives LINKS or a symlink
--     at install time, other files copied give install(FILES/DIRECTORY);
--   - rescopy gives syllable_resources, catcomp syllable_catalogs, addattrib
--     syllable_attribute, SUBDIRS / $(MAKE) -C dir add_subdirectory.
-- What it can't translate becomes a "# TODO(make):" line at the top, so the
-- result is a starting point to check, not a finished file.

local M = {}

local SRC_EXTS = { "cpp", "c", "cc", "cxx", "C", "s", "S", "asm" }
local IS_SRC = {}
for _, e in ipairs(SRC_EXTS) do IS_SRC[e] = true end

-- -lfoo -> the target that provides it in Syllable's CMake build.
local LIB_MAP = {
  syllable = "libsyllable", os = "libsyllable", media = "libmedia", pthread = "libpthread", dl = "libdl",
  codeview = "codeview", kcrypto = "kcrypto",
  freetype = "syllable::freetype", z = "syllable::z", png = "syllable::png", jpeg = "syllable::jpeg",
  gif = "syllable::gif", tiff = "syllable::tiff", curl = "syllable::curl", sqlite3 = "syllable::sqlite3",
  cups = "syllable::cups", rsvg = "syllable::librsvg", ["rsvg-2"] = "syllable::librsvg",
}
-- Linked anyway by Syllable's GCC.
local LIB_DROP = { c = true, gcc = true, ["stdc++"] = true, atheos = false }

-- Compile flags the DSL's helpers already give (or that make no sense in CMake).
local FLAG_DROP = {
  ["-c"] = true, ["-kernel"] = true, ["-fno-PIC"] = true, ["-fno-pic"] = true, ["-fPIC"] = true,
  ["-fpic"] = true, ["-pipe"] = true, ["-g"] = true, ["-MD"] = true, ["-MMD"] = true, ["-M"] = true,
  ["-shared"] = true, ["-plugin"] = true, ["-nodefaultlibs"] = true,
}

-- Rules whose recipes don't describe the build.
local SKIP_RULE = { clean = true, distclean = true, deps = true, upgrade = true, uninstall = true,
  ["catalog-clean"] = true, doc = true, docs = true, test = true, check = true }

---------------------------------------------------------------------------
-- Reading the Makefile

local function trim(s) return (s:gsub("^%s+", ""):gsub("%s+$", "")) end

local function words(s)
  local out = {}
  for w in (s or ""):gmatch("%S+") do out[#out + 1] = w end
  return out
end

-- Split a shell command line into words, keeping quoted strings together
-- (quotes removed).
local function shell_words(s)
  local out, cur, q, has = {}, {}, nil, false
  local i = 1
  while i <= #s do
    local ch = s:sub(i, i)
    if not q and (s:sub(i, i + 6) == "<shell:" or s:sub(i, i + 6) == "<undef:" or s:sub(i, i + 5) == "<glob:") then
      local j = s:find(">", i, true) or #s
      cur[#cur + 1] = s:sub(i, j)
      i = j
    elseif q then
      if ch == q then q = nil
      elseif ch == "\\" and q == '"' and i < #s then i = i + 1; cur[#cur + 1] = s:sub(i, i)
      else cur[#cur + 1] = ch end
    elseif ch == '"' or ch == "'" then q = ch; has = true
    elseif ch:match("%s") then
      if #cur > 0 or has then out[#out + 1] = table.concat(cur); cur = {}; has = false end
    elseif ch == "\\" and i < #s then i = i + 1; cur[#cur + 1] = s:sub(i, i)
    else cur[#cur + 1] = ch end
    i = i + 1
  end
  if #cur > 0 or has then out[#out + 1] = table.concat(cur) end
  return out
end

-- Split a recipe line into commands at ; && || and |, outside quotes and
-- parentheses; ( cd x && make ) loses its parentheses.
local function split_commands(s)
  local out, cur, q, i = {}, {}, nil, 1
  local function push()
    local c = trim(table.concat(cur)):gsub("^%(", ""):gsub("%)$", "")
    c = trim(c)
    if c ~= "" then out[#out + 1] = c end
    cur = {}
  end
  while i <= #s do
    local ch, two = s:sub(i, i), s:sub(i, i + 1)
    if q then
      if ch == q then q = nil end
      cur[#cur + 1] = ch
    elseif ch == '"' or ch == "'" then q = ch; cur[#cur + 1] = ch
    elseif two == "&&" or two == "||" then push(); i = i + 1
    elseif ch == ";" or ch == "|" or ch == "(" or ch == ")" then push()
    else cur[#cur + 1] = ch end
    i = i + 1
  end
  push()
  return out
end

-- Logical lines: comments dropped, backslash-newlines joined. Recipe lines
-- (starting with a tab) are marked.
local function logical_lines(text)
  local out, buf, recipe = {}, nil, false
  for line in (text .. "\n"):gmatch("(.-)\r?\n") do
    if buf then
      buf = buf .. " " .. line:gsub("^%s+", "")
    else
      buf, recipe = line, line:sub(1, 1) == "\t"
    end
    if buf:match("\\$") and not buf:match("\\\\$") then
      buf = buf:sub(1, -2)
    else
      local l = buf
      if not recipe then
        -- a # starts a comment unless escaped
        local j = 1
        while true do
          local h = l:find("#", j, true)
          if not h then break end
          if h > 1 and l:sub(h - 1, h - 1) == "\\" then
            l = l:sub(1, h - 2) .. l:sub(h)
            j = h
          else
            l = l:sub(1, h - 1)
            break
          end
        end
      end
      out[#out + 1] = { text = l, recipe = recipe }
      buf = nil
    end
  end
  return out
end

-- The comment block at the top of the Makefile, kept for the CMakeLists.txt.
local function leading_comments(text)
  local out = {}
  for line in (text .. "\n"):gmatch("(.-)\r?\n") do
    if line:match("^%s*#") then
      local c = line:gsub("^%s*#+%s?", "")
      if not c:match("^[%-=#%s]*$") then out[#out + 1] = trim(c) end
    elseif line:match("^%s*$") then
      if #out > 0 then break end
    else
      break
    end
  end
  return out
end

---------------------------------------------------------------------------
-- Variables and expansion

local Make = {}
Make.__index = Make

local function new_make(dir)
  return setmetatable({
    dir = dir,
    vars = {
      -- What the old build passed in or make defines, as far as it matters here.
      CC = { value = "gcc" }, CXX = { value = "g++" }, AR = { value = "ar" }, LD = { value = "ld" },
      RM = { value = "rm -f" }, MAKE = { value = "make" }, IMAGE = { value = "" }, DIST_DIR = { value = "" },
      INSTALL = { value = "install" }, STRIP = { value = "strip" }, SHELL = { value = "/bin/sh" },
    },
    rules = {}, order = {}, pattern_rules = {}, includes = {}, vpath = {}, todo = {}, todo_seen = {},
  }, Make)
end

function Make:note(msg)
  if not self.todo_seen[msg] then
    self.todo_seen[msg] = true
    self.todo[#self.todo + 1] = msg
  end
end

-- Find the ) or } that closes the reference opened at s[i] (the $ is at i-1).
local function closing(s, i)
  local open = s:sub(i, i)
  local close = open == "(" and ")" or "}"
  local depth = 0
  for j = i, #s do
    local ch = s:sub(j, j)
    if ch == open then depth = depth + 1
    elseif ch == close then
      depth = depth - 1
      if depth == 0 then return j end
    end
  end
end

-- Split function arguments at top-level commas.
local function split_args(s, n)
  local out, depth, start = {}, 0, 1
  for j = 1, #s do
    local ch = s:sub(j, j)
    if ch == "(" or ch == "{" then depth = depth + 1
    elseif ch == ")" or ch == "}" then depth = depth - 1
    elseif ch == "," and depth == 0 and (not n or #out < n - 1) then
      out[#out + 1] = s:sub(start, j - 1)
      start = j + 1
    end
  end
  out[#out + 1] = s:sub(start)
  return out
end

-- % patterns, as make's patsubst: one % matches anything.
local function pat_match(pat, w)
  local pre, post = pat:match("^(.-)%%(.*)$")
  if not pre then return w == pat and "" or nil end
  if #w >= #pre + #post and w:sub(1, #pre) == pre and (post == "" or w:sub(-#post) == post) then
    return w:sub(#pre + 1, #w - #post)
  end
end

local function pat_subst(from, to, w)
  if not from:find("%%") then
    -- $(VAR:.c=.o): suffix replacement
    if from ~= "" and w:sub(-#from) == from then return w:sub(1, #w - #from) .. to end
    return w
  end
  local stem = pat_match(from, w)
  if not stem then return w end
  return (to:gsub("%%", function() return stem end, 1))
end

local function map_words(s, f)
  local out = {}
  for _, w in ipairs(words(s)) do
    local r = f(w)
    if r and r ~= "" then out[#out + 1] = r end
  end
  return table.concat(out, " ")
end

function Make:call(name, argstr, auto, depth)
  local function ex(s) return self:expand(s, auto, depth + 1) end
  local a
  if name == "addprefix" or name == "addsuffix" or name == "filter" or name == "filter-out" then
    a = split_args(argstr, 2)
    local x, list = ex(a[1] or ""), ex(a[2] or "")
    if name == "addprefix" then return map_words(list, function(w) return x .. w end) end
    if name == "addsuffix" then return map_words(list, function(w) return w .. x end) end
    local pats = words(x)
    return map_words(list, function(w)
      local hit = false
      for _, p in ipairs(pats) do if pat_match(p, w) then hit = true end end
      if (name == "filter") == hit then return w end
    end)
  elseif name == "patsubst" then
    a = split_args(argstr, 3)
    local from, to = trim(ex(a[1] or "")), trim(ex(a[2] or ""))
    return map_words(ex(a[3] or ""), function(w) return pat_subst(from, to, w) end)
  elseif name == "subst" then
    a = split_args(argstr, 3)
    local from, to, text = ex(a[1] or ""), ex(a[2] or ""), ex(a[3] or "")
    if from == "" then return text end
    return (text:gsub(vim.pesc(from), (to:gsub("%%", "%%%%"))))
  elseif name == "strip" then return table.concat(words(ex(argstr)), " ")
  elseif name == "sort" then
    local list, seen = {}, {}
    for _, w in ipairs(words(ex(argstr))) do if not seen[w] then seen[w] = true; list[#list + 1] = w end end
    table.sort(list)
    return table.concat(list, " ")
  elseif name == "notdir" then return map_words(ex(argstr), function(w) return w:match("[^/]*$") end)
  elseif name == "dir" then return map_words(ex(argstr), function(w) return w:match("^(.*/)") or "./" end)
  elseif name == "basename" then return map_words(ex(argstr), function(w) return (w:gsub("%.[^./]*$", "")) end)
  elseif name == "suffix" then return map_words(ex(argstr), function(w) return w:match("(%.[^./]*)$") end)
  elseif name == "wildcard" then
    -- Keep the pattern: the CMakeLists.txt globs it (see glob_files).
    return map_words(ex(argstr), function(w) return "<glob:" .. w .. ">" end)
  elseif name == "foreach" then
    a = split_args(argstr, 3)
    local var, list = trim(ex(a[1] or "")), ex(a[2] or "")
    local out = {}
    for _, w in ipairs(words(list)) do
      local saved = self.vars[var]
      self.vars[var] = { value = w, simple = true }
      out[#out + 1] = ex(a[3] or "")
      self.vars[var] = saved
    end
    return table.concat(out, " ")
  elseif name == "if" then
    a = split_args(argstr, 3)
    if trim(ex(a[1] or "")) ~= "" then return ex(a[2] or "") end
    return ex(a[3] or "")
  elseif name == "shell" then
    return "<shell:" .. trim(ex(argstr)) .. ">"
  elseif name == "origin" or name == "error" or name == "warning" or name == "info" or name == "eval" or name == "call" then
    return ""
  end
end

-- Expand $(...) references. auto: the automatic variables of a rule.
function Make:expand(s, auto, depth)
  depth = depth or 0
  if depth > 30 or not s:find("%$") then return s end
  local out, i = {}, 1
  while i <= #s do
    local ch = s:sub(i, i)
    if ch ~= "$" then
      local j = s:find("%$", i) or (#s + 1)
      out[#out + 1] = s:sub(i, j - 1)
      i = j
    else
      local nx = s:sub(i + 1, i + 1)
      if nx == "$" then
        out[#out + 1] = "$"; i = i + 2
      elseif nx == "(" or nx == "{" then
        local j = closing(s, i + 1)
        if not j then out[#out + 1] = s:sub(i); break end
        local inner = s:sub(i + 2, j - 1)
        local fname, rest = inner:match("^([%a%-]+)%s+(.*)$")
        local r = fname and self:call(fname, rest, auto, depth)
        if r == nil then
          local ename = self:expand(inner, auto, depth + 1)
          local var, from, to = ename:match("^([^:]+):(.-)=(.*)$")
          if var then
            r = map_words(self:var(var, auto, depth), function(w) return pat_subst(from, to, w) end)
          elseif auto and auto[ename] then
            r = auto[ename]
          else
            r = self:var(ename, auto, depth)
          end
        end
        out[#out + 1] = r
        i = j + 1
      else
        out[#out + 1] = (auto and auto[nx]) or self:var(nx, auto, depth)
        i = i + 2
      end
    end
  end
  return table.concat(out)
end

function Make:var(name, auto, depth)
  local v = self.vars[name]
  if not v then
    if name:match("^[%u_][%u%d_]*$") and not ({ ["@"] = 1, ["<"] = 1, ["^"] = 1 })[name] then
      self.undefined = self.undefined or {}
      self.undefined[name] = true
      if self.mark_undef then return "<undef:" .. name .. ">" end
    end
    return ""
  end
  if v.simple then return v.value end
  return self:expand(v.value, auto, (depth or 0) + 1)
end

function Make:defined(name)
  local v = self.vars[name]
  return v ~= nil and trim(v.value) ~= ""
end

---------------------------------------------------------------------------
-- Parsing

function Make:parse(text)
  local cond = {} -- stack of { active, taken }
  local function active()
    for _, c in ipairs(cond) do if not c.active then return false end end
    return true
  end
  local rule
  local in_define = false
  for _, l in ipairs(logical_lines(text)) do
    local line = l.text
    local stripped = trim(line)
    local kw, arg = stripped:match("^(%a+)%s*(.*)$")
    if in_define then
      if stripped:match("^endef") then in_define = false end
    elseif not l.recipe and (kw == "define" or stripped:match("^override%s+define")) then
      in_define = true
      rule = nil
    elseif not l.recipe and (kw == "ifdef" or kw == "ifndef" or kw == "ifeq" or kw == "ifneq") then
      local val
      if kw == "ifdef" or kw == "ifndef" then
        val = self:defined(trim(self:expand(arg)))
        if kw == "ifndef" then val = not val end
      else
        local a, b = arg:match("^%((.-),(.*)%)$")
        if not a then a, b = arg:match("^[\"'](.-)[\"']%s+[\"'](.-)[\"']$") end
        if a then
          val = trim(self:expand(a)) == trim(self:expand(b))
          if kw == "ifneq" then val = not val end
        else
          val = false
          self:note("couldn't read `" .. stripped .. "`; its block was left out")
        end
      end
      cond[#cond + 1] = { active = val, taken = val }
      rule = nil
    elseif not l.recipe and kw == "else" then
      local c = cond[#cond]
      if c then
        local nested = arg:match("^(ifn?def)%s+(.*)$")
        if nested and not c.taken then
          local v = self:defined(trim(self:expand(arg:match("^ifn?def%s+(.*)$"))))
          if nested == "ifndef" then v = not v end
          c.active, c.taken = v, v
        else
          c.active = not c.taken
          c.taken = true
        end
      end
    elseif not l.recipe and kw == "endif" then
      table.remove(cond)
    elseif not active() then
      -- skipped
    elseif l.recipe then
      if rule then rule.recipe[#rule.recipe + 1] = line:sub(2) end
    elseif stripped == "" then
      -- a blank line doesn't end a rule's recipe in make
    elseif kw == "include" or kw == "-include" or kw == "sinclude" or stripped:match("^%-include%s") then
      local what = stripped:match("^%-?s?include%s+(.*)$")
      for _, w in ipairs(words(self:expand(what or ""))) do self.includes[#self.includes + 1] = w end
      rule = nil
    elseif kw == "vpath" then
      local _, dirs = arg:match("^(%S+)%s+(.*)$")
      for _, d in ipairs(words((self:expand(dirs or "")):gsub(":", " "))) do self.vpath[#self.vpath + 1] = d end
      rule = nil
    elseif stripped:match("^%.PHONY") or stripped:match("^%.SUFFIXES")
        or kw == "unexport" or kw == "define" then
      rule = nil
    else
      local body = stripped:gsub("^override%s+", ""):gsub("^export%s+", "")
      local name, op, value = body:match("^([%w_%.%-/]+)%s*([:?+!]?:?=)%s*(.*)$")
      local colon = body:find(":")
      local eq = body:find("=")
      if name and (not colon or (eq and eq < colon) or op:find(":")) then
        if op == "+=" then
          local v = self.vars[name]
          if v then
            if v.simple then v.value = v.value .. " " .. self:expand(value)
            else v.value = v.value .. " " .. value end
          else
            self.vars[name] = { value = value }
          end
        elseif op == "?=" then
          if not self.vars[name] then self.vars[name] = { value = value } end
        elseif op == ":=" or op == "::=" then
          self.vars[name] = { value = self:expand(value), simple = true }
        elseif op == "!=" then
          self.vars[name] = { value = "<shell:" .. value .. ">", simple = true }
        else
          self.vars[name] = { value = value }
        end
        rule = nil
      elseif colon then
        -- targets : prerequisites [; recipe]   (targets may be quoted)
        local tpart, rest = body:match("^(.-)%s*::?%s*(.*)$")
        local inline
        rest, inline = rest:match("^([^;]*);?(.*)$")
        local targets = shell_words(self:expand(tpart))
        local prereqs = shell_words(self:expand((rest:gsub("|", " "))))
        rule = { targets = targets, prereqs = prereqs, recipe = {} }
        if inline and trim(inline) ~= "" then rule.recipe[1] = trim(inline) end
        for _, t in ipairs(targets) do
          if t:find("%%") then
            self.pattern_rules[#self.pattern_rules + 1] = { target = t, prereqs = prereqs, rule = rule }
          elseif t:match("^%.%w+%.o$") or t:match("^%.%w+$") then
            -- suffix rule: .cpp.o:
            self.pattern_rules[#self.pattern_rules + 1] = { suffix = t, rule = rule }
          else
            local r = self.rules[t]
            if r then
              vim.list_extend(r.prereqs, prereqs)
              if #rule.recipe > 0 or #r.recipe == 0 then r.recipe = rule.recipe end
              rule = r
            else
              self.rules[t] = rule
              self.order[#self.order + 1] = t
            end
          end
        end
      else
        self:note("couldn't read `" .. stripped .. "`")
        rule = nil
      end
    end
  end
end

---------------------------------------------------------------------------
-- Commands

-- Every command of the rules reachable from the default goal, install and
-- dist, with the rule's automatic variables expanded.
function Make:commands()
  local goal
  for _, t in ipairs(self.order) do
    if not t:match("^%.[%u%w]*$") and not t:match("^%.%w+%.%w+$") and not t:find("%%") then goal = t; break end
  end
  local start = { goal, "all", "install", "dist" }
  local seen, cmds, visited = {}, {}, {}
  local function visit(t, phase)
    if not t or seen[t] or SKIP_RULE[t] then return end
    seen[t] = true
    local r = self.rules[t]
    if not r then return end
    for _, p in ipairs(r.prereqs) do visit(p, phase) end
    if visited[r] then return end
    visited[r] = true
    local auto = { ["@"] = t, ["<"] = r.prereqs[1] or "", ["^"] = table.concat(r.prereqs, " "),
      ["*"] = (t:gsub("%.[^./]*$", "")) }
    local raw = {}
    for _, line in ipairs(r.recipe) do
      raw[#raw + 1] = self:expand(line, auto)
    end
    cmds[#cmds + 1] = { rule = t, phase = phase, text = table.concat(raw, "\n"), lines = raw }
  end
  for _, t in ipairs(start) do
    visit(t, (t == "install" or t == "dist") and "install" or "build")
  end
  return cmds
end

-- One recipe's text, its for-loops pulled out: { loops = { {var, list, body} }, cmds = { words... } }.
local function analyse_recipe(text)
  local loops = {}
  -- for X in a b c; do ...; done
  text = text:gsub("for%s+([%w_]+)%s+in%s+(.-);?%s*do%s+(.-)%s*;?%s*done", function(var, list, body)
    loops[#loops + 1] = { var = var, list = words((list:gsub(";", " "))), body = body }
    return " ; "
  end)
  local cmds = {}
  local cwd = nil
  for _, line in ipairs(vim.split(text, "\n")) do
    for _, c in ipairs(split_commands(line)) do
      c = c:gsub("^[@%-+]+", "")
      local w = shell_words(c)
      while w[1] == "then" or w[1] == "else" or w[1] == "do" or w[1] == "exec" or w[1] == "elif" do
        table.remove(w, 1)
        c = c:gsub("^%S+%s*", "")
      end
      if w[1] == "if" or w[1] == "fi" or w[1] == "done" or w[1] == "while" or w[1] == "exit" or w[1] == "set" then
        w = {}
      end
      if w[1] == "cd" then cwd = w[2] end
      if #w > 0 then cmds[#cmds + 1] = { words = w, cwd = cwd, text = c } end
    end
  end
  return loops, cmds
end

local function base(w) return (w or ""):match("([^/]+)$") or w end
local function is_compiler(w)
  w = base(w or "")
  return w:match("gcc") or w:match("g%+%+") or w == "cc" or w == "c++" or w:match("%-cc$")
    or w:match("clang")
end

---------------------------------------------------------------------------
-- Files and paths

function Make:exists(rel) return vim.uv.fs_stat(self.dir .. "/" .. rel) ~= nil end

-- Normalize a path in the Makefile's directory: drop ./ and a trailing /.
local function norm(p)
  p = p:gsub("^%./", ""):gsub("/%./", "/"):gsub("/+$", "")
  return p
end

-- The source an object is built from.
function Make:source_for(obj)
  obj = norm(obj)
  local r = self.rules[obj]
  if r then
    for _, p in ipairs(r.prereqs) do
      local ext = p:match("%.([%w]+)$")
      if ext and IS_SRC[ext] then return norm(p) end
    end
  end
  local stem = obj:gsub("%.o$", "")
  -- pattern rules: $(OBJDIR)/%.o : %.c, or src/%.o : src/%.cpp
  for _, pr in ipairs(self.pattern_rules) do
    if pr.target then
      local s = pat_match(norm(pr.target), obj)
      if s then
        for _, p in ipairs(pr.prereqs) do
          local cand = norm((p:gsub("%%", function() return s end)))
          if self:exists(cand) then return cand end
        end
      end
    end
  end
  local objdir = norm(self:var("OBJDIR"))
  local cands = { stem }
  if objdir ~= "" and stem:sub(1, #objdir + 1) == objdir .. "/" then table.insert(cands, 1, stem:sub(#objdir + 2)) end
  local first_strip = stem:match("^[^/]+/(.+)$")
  if first_strip then cands[#cands + 1] = first_strip end
  for _, c in ipairs(cands) do
    for _, e in ipairs(SRC_EXTS) do
      if self:exists(c .. "." .. e) then return c .. "." .. e end
    end
  end
  -- last resort: one file of that name somewhere below
  self.all_files = self.all_files or vim.fs.find(function(n) return IS_SRC[n:match("%.([%w]+)$") or ""] end,
    { path = self.dir, type = "file", limit = 5000 })
  local hits = {}
  for _, f in ipairs(self.all_files) do
    local n = base(f):gsub("%.[%w]+$", "")
    if n == base(stem) then hits[#hits + 1] = f:sub(#self.dir + 2) end
  end
  if #hits == 1 then return hits[1] end
  for _, c in ipairs(cands) do
    -- obj/x.o built from src/x.cpp, or found through VPATH
    local subs = { "src", "source", "sources" }
    vim.list_extend(subs, words((self:var("VPATH")):gsub(":", " ")))
    vim.list_extend(subs, self.vpath)
    for _, sub in ipairs(subs) do
      for _, e in ipairs(SRC_EXTS) do
        local p = sub .. "/" .. base(c) .. "." .. e
        if self:exists(p) then return p end
      end
    end
  end
end

-- Strip where the old build put the image: $(IMAGE) (empty here), $(DIST_DIR)/atheos, a leading /.
local function image_path(p)
  p = p:gsub("^/+", "/")
  p = p:gsub("^/atheos/", "/")
  p = p:gsub("^/boot/", "/")
  return (p:gsub("^/+", ""):gsub("/+$", ""))
end

local KNOWN_DIRS = { programs = 1, bin = 1, libraries = 1, lib = 1, headers = 1, include = 1, translators = 1,
  video = 1, input = 1, decorators = 1, eth = 1, net = 1, dev = 1, fs = 1, dock = 1, media = 1, extensions = 1,
  preferences = 1, applications = 1, system = 1, drivers = 1, keymaps = 1, fonts = 1, icons = 1, config = 1,
  scripts = 1, doc = 1, documentation = 1, development = 1, resources = 1, ["plug-ins"] = 1, plugins = 1 }

---------------------------------------------------------------------------
-- Analysis

local function cmake_target_name(out)
  local n = base(out):gsub("%.so[%.%d]*$", ""):gsub("%.a$", "")
  n = n:gsub("[^%w_%-%.+]", "_"):gsub("_+", "_"):gsub("^_", ""):gsub("_$", "")
  return n:lower()
end

function Make:analyse()
  self.mark_undef = true
  local cmds = self:commands()
  local targets, by_out = {}, {}
  local flags = {}        -- compile flags, in order
  local flag_seen = {}
  local installs = {}     -- { srcs, dest, rec }
  local links = {}        -- { target, link }
  local strips = {}
  local resources = {}    -- target -> { files, add }
  local catalogs = {}
  local attrs = {}
  local subdirs, sub_seen = {}, {}
  local kernel_cflags = false
  local others = {}

  local function add_flag(f)
    if not flag_seen[f] then flag_seen[f] = true; flags[#flags + 1] = f end
  end
  local function add_subdir(d)
    d = norm(d)
    if d ~= "" and d ~= "." and not sub_seen[d] then sub_seen[d] = true; subdirs[#subdirs + 1] = d end
  end

  -- Compile flags from compile commands, wherever they are (pattern, suffix
  -- and explicit rules), or from the variables make's own rules use.
  local function scan_compile(w)
    local skip = false
    for i = 2, #w do
      local f = w[i]
      if skip then skip = false
      elseif f == "-o" or f == "-x" or f == "-MF" or f == "-MT" then skip = true
      elseif f:match("^%-") or f:match("^<shell:") then add_flag(f)
      end
      if f == "-kernel" then kernel_cflags = true end
    end
  end
  local compiled = false
  local function compile_rules(rule, auto)
    for _, line in ipairs(rule.recipe) do
      for _, c in ipairs(split_commands(self:expand(line, auto))) do
        local w = shell_words((c:gsub("^[@%-+]+", "")))
        if is_compiler(w[1]) and vim.tbl_contains(w, "-c") then compiled = true; scan_compile(w) end
      end
    end
  end
  local rules_done = {}
  for _, pr in ipairs(self.pattern_rules) do
    if not rules_done[pr.rule] then rules_done[pr.rule] = true; compile_rules(pr.rule, { ["<"] = "x.c", ["@"] = "x.o" }) end
  end
  for _, t in ipairs(self.order) do
    local r = self.rules[t]
    if t:match("%.o$") and not rules_done[r] then rules_done[r] = true; compile_rules(r, { ["<"] = r.prereqs[1] or "", ["@"] = t }) end
  end
  local uses_rules = false
  for _, inc in ipairs(self.includes) do
    if inc:match("makefile%.rules$") then uses_rules = true end
  end
  if uses_rules or not compiled then
    for _, v in ipairs({ "CFLAGS", "CXXFLAGS", "CPPFLAGS", "AFLAGS" }) do
      for _, f in ipairs(shell_words(self:var(v))) do
        if f == "-kernel" then kernel_cflags = true end
        add_flag(f)
      end
    end
  end

  for _, rec in ipairs(cmds) do
    local loops, list = analyse_recipe(rec.text)
    for _, lp in ipairs(loops) do
      -- the body runs for each word: catalogs and subdirectories
      local body = lp.body
      if body:match("catcomp") then catalogs.langs = lp.list end
      if body:match("rescopy") and body:match("%.catalog") then catalogs.embed = true end
      if body:match("%$%(?MAKE%)?") or body:match("make%s") then
        if body:match("%-C%s+%$%$?{?" .. lp.var) or body:match("cd%s+%$%$?{?" .. lp.var) then
          for _, d in ipairs(lp.list) do add_subdir(d) end
        end
      end
      if (body:match("cp%s") or body:match("install%s")) and body:match("%.catalog") then
        local dest = body:match("([^%s;]+)/%$%$?{?" .. lp.var .. "}?/?[^%s;]*$")
        if dest then catalogs.install = image_path(dest) end
      end
    end
    for _, c in ipairs(list) do
      local w = c.words
      local cmd = base(w[1])
      if is_compiler(w[1]) or cmd == "ld" or cmd == "ar" or cmd:match("%-ar$") then
        local is_ar = cmd == "ar" or cmd:match("%-ar$")
        local out, inputs, libs, opts, soname = nil, {}, {}, {}, nil
        local compile = vim.tbl_contains(w, "-c")
        if is_ar then
          -- ar rcs libfoo.a a.o b.o
          local i = 2
          if w[i] and not w[i]:match("%.a$") then i = i + 1 end
          out = w[i]
          for j = i + 1, #w do inputs[#inputs + 1] = w[j] end
        elseif not compile then
          local i = 2
          while i <= #w do
            local f = w[i]
            if f == "-o" then out = w[i + 1]; i = i + 1
            elseif f:match("^%-l") then libs[#libs + 1] = f:sub(3)
            elseif f == "-Xlinker" then
              local x = w[i + 1] or ""
              i = i + 1
              if x:match("^%-soname=") then soname = x:match("=(.*)$")
              elseif x == "-soname" then soname = w[i + 2]; i = i + 2
              else opts[#opts + 1] = x end
            elseif f:match("^%-Wl,") then
              local s = f:match("%-soname[=,]([^,]+)")
              if s then soname = s else opts[#opts + 1] = f end
            elseif f:match("^%-") then opts[#opts + 1] = f
            elseif f:match("^<shell:") then opts[#opts + 1] = f
            else inputs[#inputs + 1] = f end
            i = i + 1
          end
        end
        if out then
          local objs, srcs, extra = {}, {}, {}
          for _, f in ipairs(inputs) do
            local ext = f:match("%.([%w]+)$")
            if ext == "o" then objs[#objs + 1] = f
            elseif ext and IS_SRC[ext] then srcs[#srcs + 1] = norm(f)
            elseif ext == "a" or ext == "so" then extra[#extra + 1] = f
            end
          end
          if #objs > 0 or #srcs > 0 or (#inputs > 0 and is_ar) then
            local kind
            local has = {}
            for _, o in ipairs(opts) do has[o] = true end
            if is_ar then kind = "static"
            elseif has["-kernel"] then kind = "driver"
            elseif has["-plugin"] then kind = "plugin"
            elseif has["-shared"] then kind = "library"
            else kind = "program" end
            if #srcs > 0 then
              -- compiled and linked in one: its flags are compile flags too
              for _, o in ipairs(opts) do
                if o:match("^%-[IDfOWm]") or o:match("^%-std") then add_flag(o) end
              end
            end
            local t = by_out[out] or { out = out }
            t.kind, t.objs, t.srcs, t.libs, t.opts, t.soname, t.extra = kind, objs, srcs, libs, opts, soname, extra
            if not by_out[out] then by_out[out] = t; targets[#targets + 1] = t end
          end
        end
      elseif cmd == "cp" or cmd == "install" or cmd == "mv" or cmd == "rfs" then
        if rec.phase == "install" or cmd ~= "mv" then
          local args, recursive = {}, false
          local i = 2
          while i <= #w do
            local f = w[i]
            if f:match("^%-") then
              if f:match("r") or f:match("R") or f == "-a" then recursive = true end
              if cmd == "install" and (f == "-m" or f == "-o" or f == "-g") then i = i + 1 end
              if cmd == "install" and f == "-s" then strips.__install_s = true end
            else
              args[#args + 1] = f
            end
            i = i + 1
          end
          if #args >= 2 then
            local dest = table.remove(args)
            installs[#installs + 1] = { srcs = args, dest = dest, recursive = recursive, phase = rec.phase,
              strip = cmd == "install" and vim.tbl_contains(w, "-s"), cwd = c.cwd }
          end
        end
      elseif cmd == "ln" then
        local args = {}
        for i = 2, #w do if not w[i]:match("^%-") then args[#args + 1] = w[i] end end
        if #args == 2 then links[#links + 1] = { target = args[1], link = args[2] }
        else others[#others + 1] = rec.rule .. ": " .. c.text end
      elseif cmd == "strip" or cmd:match("%-strip$") then
        for i = 2, #w do if not w[i]:match("^%-") then strips[w[i]] = true end end
      elseif cmd == "rescopy" then
        -- rescopy [-r|-a] file res... (the mode may also come after the file)
        local file, rest = nil, {}
        for i = 2, #w do
          if w[i]:match("^%-") then -- -r / -a
          elseif not file then file = w[i]
          else rest[#rest + 1] = w[i] end
        end
        if file then
          resources[file] = resources[file] or { files = {}, add = {} }
          for _, f in ipairs(rest) do
            if f:match("%.catalog$") or f:match("%.catalog=") then
              catalogs.embed = true
            elseif f:find("=") then
              table.insert(resources[file].add, f)
            else
              table.insert(resources[file].files, f)
            end
          end
        end
      elseif cmd == "catcomp" then
        local name
        for i = 2, #w do
          if w[i] == "-c" and w[i + 1] then name = w[i + 1] end
        end
        if name then
          local d = c.cwd and norm(c.cwd) or "."
          if name:find("/") then
            d = norm((d == "." and "" or d .. "/") .. name:match("^(.*)/"))
            name = name:match("([^/]+)$")
          end
          catalogs.name = name:gsub("%.[^.]+$", "")
          catalogs.ext = name:match("%.([^.]+)$")
          catalogs.dir = d
        end
      elseif cmd == "addattrib" then
        if w[2] and w[3] then attrs[#attrs + 1] = { file = w[2], name = w[3], value = table.concat(w, " ", 4) } end
      elseif cmd == "make" or w[1] == self:var("MAKE") then
        for i = 2, #w do
          if w[i] == "-C" and w[i + 1] then add_subdir(w[i + 1]) end
        end
        if c.cwd and c.cwd ~= "" and not c.cwd:find("%$") then add_subdir(c.cwd) end
      elseif cmd == "mkdir" or cmd == "echo" or cmd == "cd" or cmd == "rm" or cmd == "true" or cmd == "touch"
          or cmd == "chmod" or cmd == "printf" or cmd == "ranlib" or cmd:match("%-ranlib$") or cmd == "sync" or cmd == ":" or cmd == "test" or cmd == "[" then
        -- nothing to carry over (mkdir: install makes its directories)
      else
        others[#others + 1] = rec.rule .. ": " .. c.text
      end
    end
  end

  -- SUBDIRS with recurse.rules (or a for loop over them, above)
  for _, inc in ipairs(self.includes) do
    if inc:match("rules$") or inc:match("subdirs") then
      for _, d in ipairs(words(self:var("SUBDIRS"))) do add_subdir(d) end
    end
  end
  if #targets == 0 then
    for _, d in ipairs(words(self:var("SUBDIRS"))) do
      if vim.uv.fs_stat(self.dir .. "/" .. d) then add_subdir(d) end
    end
  end

  return {
    targets = targets, flags = flags, installs = installs, links = links, strips = strips,
    resources = resources, catalogs = catalogs, attrs = attrs, subdirs = subdirs,
    kernel_cflags = kernel_cflags, others = others,
  }
end

---------------------------------------------------------------------------
-- Writing the CMakeLists.txt

-- Join words into lines of at most ~100 columns, breaking before KEYWORDs.
local KEYWORDS = { SOURCES = 1, INCLUDES = 1, DEFINES = 1, OPTIONS = 1, LIBS = 1, LINKS = 1, INSTALL = 1,
  OUTPUT_NAME = 1, SONAME = 1, STRIP = 1, KERNEL = 1, FILES = 1, ADD = 1, NAME = 1, DIR = 1, LANGS = 1,
  EMBED = 1, EXT = 1, MAIN = 1, DESTINATION = 1, RENAME = 1, GLOB = 1 }

local function call(fn, args)
  local lines, cur = {}, fn .. "("
  local width = 100
  local glued = {}
  local k = 1
  while k <= #args do
    local a = args[k]
    if KEYWORDS[a] and args[k + 1] and not KEYWORDS[args[k + 1]] then
      a = a .. " " .. args[k + 1]
      k = k + 1
    end
    glued[#glued + 1] = a
    k = k + 1
  end
  for i, a in ipairs(glued) do
    local sep = (i == 1) and "" or " "
    if #cur + #sep + #a + 1 > width and cur:match("%S") and not cur:match("%($") then
      lines[#lines + 1] = cur
      cur = "  " .. a
    else
      cur = cur .. sep .. a
    end
  end
  lines[#lines + 1] = cur .. ")"
  return table.concat(lines, "\n")
end

-- Quote a CMake argument if it needs it.
local function q(s)
  if s == "" or s:find("[%s;\"()#]") then return '"' .. s:gsub('\\', '\\\\'):gsub('"', '\\"') .. '"' end
  return s
end

-- A source path -> a CMake path relative to the CMakeLists.txt: ${CMAKE_CURRENT_SOURCE_DIR}/x for includes.
local function inc_path(self, p)
  p = norm(p)
  if p == "" or p == "." then return nil end
  if p:find("<undef:") then
    local u = p:match("<undef:([^>]*)>")
    return nil, "-I" .. p:gsub("<undef:([^>]*)>", "$(%1)") .. ": $(" .. u .. ") is set outside the Makefile"
  end
  if p:find("<shell:") or p:find("%$") then return nil, "include " .. p end
  if p:sub(1, 1) == "/" then
    if p:match("^/system/development/headers") or p:match("^/usr/include") or p:match("^/usr/local/include")
        or p:match("^/resources/") then
      return nil, "-I" .. p .. ": a header folder on Syllable itself; in this build it's the staging sysroot (${SYLLABLE_STAGING}/include) or a target's include directories"
    end
    return nil, "-I" .. p .. " is an absolute path"
  end
  if not self:exists(p) then return "${CMAKE_CURRENT_SOURCE_DIR}/" .. p, "-I" .. p .. " doesn't exist here" end
  return "${CMAKE_CURRENT_SOURCE_DIR}/" .. p
end

-- Glob files the Makefile names with wildcards: { cmake lines, the variable }.
local glob_n = 0
local function files_arg(self, pre, list)
  local plain, globs = {}, {}
  for _, f in ipairs(list) do
    f = f:gsub("^<glob:(.*)>$", "%1")
    f = norm(f)
    if f:find("[*?%[]") then globs[#globs + 1] = f else plain[#plain + 1] = q(f) end
  end
  if #globs == 0 then return plain end
  glob_n = glob_n + 1
  local var = "_files" .. (glob_n > 1 and glob_n or "")
  local g = {}
  for _, p in ipairs(globs) do g[#g + 1] = "${CMAKE_CURRENT_SOURCE_DIR}/" .. p end
  pre[#pre + 1] = call("file", vim.list_extend({ "GLOB", var, "RELATIVE", "${CMAKE_CURRENT_SOURCE_DIR}" }, g))
  plain[#plain + 1] = "${" .. var .. "}"
  return plain
end

-- Names other CMakeLists.txt in the checkout already use for targets.
local function existing_targets(root)
  local names = {}
  if not root then return names end
  for _, cm in ipairs(vim.fs.find("CMakeLists.txt", { path = root, type = "file", limit = math.huge })) do
    local ok, lines = pcall(vim.fn.readfile, cm)
    if ok then
      local text = table.concat(lines, "\n")
      for name in text:gmatch("syllable_[%w_]+%s*%(%s*([%w_%-%.+]+)") do names[name] = cm end
      for name in text:gmatch("add_executable%s*%(%s*([%w_%-%.+]+)") do names[name] = cm end
      for name in text:gmatch("add_library%s*%(%s*([%w_%-%.+]+)") do names[name] = cm end
      for name in text:gmatch("add_custom_target%s*%(%s*([%w_%-%.+]+)") do names[name] = cm end
    end
  end
  return names
end

-- The Syllable checkout above dir: the folder with cmake/Syllable.cmake.
function M.checkout(dir)
  dir = vim.fs.normalize(vim.fn.fnamemodify(dir, ":p")):gsub("/$", "")
  while dir and dir ~= "" do
    if vim.uv.fs_stat(dir .. "/cmake/Syllable.cmake") then return dir end
    local up = vim.fs.dirname(dir)
    if not up or up == dir then break end
    dir = up
  end
end

-- Convert the Makefile at path. Returns the CMakeLists.txt text and notes:
-- { todo = {...}, subdirs_missing = {...}, parent = path or nil, parent_missing = name or nil }.
function M.convert(path)
  path = vim.fn.fnamemodify(path, ":p")
  local dir = vim.fs.dirname(path)
  local f = assert(io.open(path, "r"))
  local text = f:read("*a")
  f:close()
  glob_n = 0

  local head = text:sub(1, 600):lower()
  if head:find("automake") or head:find("generated automatically") or head:find("generated by configure") then
    local lines = {
      "# " .. base(dir) .. ": this Makefile was generated by configure/automake, so there's no",
      "# Makefile to convert. Build it as a port instead (syllable_port() in cmake/SyllablePorts.cmake),",
      "# or convert its Makefile.am by hand.",
    }
    return table.concat(lines, "\n") .. "\n", { todo = { "generated by configure" }, subdirs_missing = {}, generated = true }
  end
  if text:match("\ninclude%s+%.%./Rules") or text:match("\ninclude%s+%.%./Makeconfig") then
    return "# " .. base(dir) .. ": part of glibc's own make system (../Rules), which this conversion\n"
      .. "# doesn't read. glibc is built as a whole, not folder by folder.\n",
      { todo = { "glibc" }, subdirs_missing = {}, generated = true }
  end
  local mk = new_make(dir)
  mk:parse(text)
  local a = mk:analyse()
  local root = M.checkout(dir)
  local taken = existing_targets(root)
  local own_cm = dir .. "/CMakeLists.txt"

  local out, pre, post = {}, {}, {}
  local todo = mk.todo

  -- Classify compile flags once.
  local includes, defines, options, freetype = {}, {}, {}, false
  local i = 1
  local flags = a.flags
  while i <= #flags do
    local fl = flags[i]
    if fl:match("^<shell:") then
      if fl:match("freetype%-config") or fl:match("pkg%-config.*freetype") then freetype = true
      else todo[#todo + 1] = "compile flags from `" .. fl:match("^<shell:(.*)>$") .. "`" end
    elseif fl == "-I" or fl == "-D" then
      flags[i + 1] = fl .. (flags[i + 1] or "")
    elseif fl:match("^%-I") then
      local p, why = inc_path(mk, fl:sub(3))
      if p then includes[#includes + 1] = p end
      if why then todo[#todo + 1] = why end
    elseif fl:match("^%-D") then
      local d = fl:sub(3)
      if d ~= "__KERNEL__" then defines[#defines + 1] = q(d) end
    elseif fl:match("^%-U") then
      options[#options + 1] = fl
    elseif FLAG_DROP[fl] or fl:match("^%-g%d?$") or fl:match("^%-ggdb") or fl == "-fexceptions"
        or fl == "assembler-with-cpp" or fl == "-x" then
      -- the helpers give these
    elseif fl:match("^%-L") or fl:match("^%-l") or fl:match("^%-o") then
      -- link flags in CFLAGS
    elseif fl:match("^%-") then
      options[#options + 1] = fl
    end
    i = i + 1
  end
  if freetype then
    vim.list_extend(includes, { "${SYLLABLE_STAGING}/include/freetype2", "${SYLLABLE_STAGING}/include" })
  end

  -- Installs: which target each copy is for.
  local function same_file(a1, b1)
    a1, b1 = norm(a1), norm(b1)
    return a1 == b1 or base(a1) == base(b1)
  end

  local objdir = norm(mk:var("OBJDIR"))
  local target_of = {} -- output path -> target
  local own = {}       -- target names made here
  for _, t in ipairs(a.targets) do
    target_of[norm(t.out)] = t
    local name
    if t.kind == "library" then
      t.soname = t.soname or base(t.out)
      name = cmake_target_name(t.soname)
    elseif t.kind == "static" then
      name = cmake_target_name(t.out):gsub("^lib", "")
      if a.kernel_cflags then name = name .. "-kernel" end
    else
      name = cmake_target_name(t.out)
    end
    if taken[name] and vim.fs.normalize(taken[name]) ~= vim.fs.normalize(own_cm) then
      local parent = base(dir):lower():gsub("[^%w_%-]", "_")
      if parent ~= name then name = parent .. "_" .. name end
    end
    t.name = name
    if t.kind == "library" or t.kind == "static" then
      own[cmake_target_name(t.out):gsub("^lib", "")] = name
      own[cmake_target_name(t.soname or t.out):gsub("^lib", "")] = name
    end
  end

  local function find_target(file)
    file = norm(file)
    if target_of[file] then return target_of[file] end
    for _, t in ipairs(a.targets) do
      if same_file(t.out, file) then return t end
    end
    if file:find("[*?]") then
      local pat = "^" .. vim.pesc(file):gsub("%%%*", "[^/]*"):gsub("%%%?", "[^/]") .. "$"
      for _, t in ipairs(a.targets) do
        if norm(t.out):match(pat) then return t end
      end
    end
  end

  -- Destinations
  local dirs_made = {}
  for _, rec in ipairs(a.installs) do
    local dest = image_path(rec.dest)
    if dest:find("<") then dest = "" end
    for _, s in ipairs(rec.srcs) do
      local t = find_target(s)
      if t and dest ~= "" and not t.install then
        local last = base(dest)
        if rec.dest:match("/$") or KNOWN_DIRS[last] or last == "" then
          t.install = dest
        elseif last == base(t.out) then
          t.install = vim.fs.dirname(dest)
        elseif #rec.srcs > 1 then
          t.install = dest
        else
          -- the copy gives it a new name
          t.install = vim.fs.dirname(dest)
          t.output_name = last
        end
        if rec.strip then t.strip = true end
      elseif not t and dest ~= "" then
        -- some other file: a header, a script, data
        local sf = norm(s)
        if not sf:find("%.o$") and not (objdir ~= "" and sf:sub(1, #objdir + 1) == objdir .. "/")
            and not sf:find("%.catalog$") and not sf:find("^<") then
          local key = sf .. "->" .. dest
          if not dirs_made[key] then
            dirs_made[key] = true
            local d = dest
            if not rec.dest:match("/$") and not KNOWN_DIRS[base(dest)] and #rec.srcs == 1
                and base(dest) ~= base(sf) and base(dest):find("%.") then
              -- copied under a new name
              post[#post + 1] = call("install", { "FILES", q(sf), "DESTINATION", q(vim.fs.dirname(d)), "RENAME", q(base(d)) })
            elseif rec.recursive and (sf == "*" or sf:match("/%*$")) then
              local from = sf == "*" and "./" or sf:gsub("%*$", "")
              post[#post + 1] = call("install", { "DIRECTORY", q(from), "DESTINATION", q(d), "USE_SOURCE_PERMISSIONS",
                "PATTERN", "Makefile", "EXCLUDE", "PATTERN", "CMakeLists.txt", "EXCLUDE", "PATTERN", "CVS", "EXCLUDE" })
            elseif rec.recursive and not sf:find("[*?]") and not sf:match("%.[%w]+$") then
              post[#post + 1] = call("install", { "DIRECTORY", q(sf:gsub("/%*$", "") .. (sf:match("/%*$") and "/" or "")),
                "DESTINATION", q(base(dest) == base(sf) and vim.fs.dirname(d) or d) })
            else
              local d2 = (base(dest) == base(sf) and not KNOWN_DIRS[base(dest)]) and vim.fs.dirname(d) or d
              local pre2 = {}
              local fa = files_arg(mk, pre2, { sf })
              vim.list_extend(post, pre2)
              post[#post + 1] = call("install", vim.list_extend(vim.list_extend({ "FILES" }, fa), { "DESTINATION", q(d2) }))
            end
          end
        end
      end
    end
  end
  for file in pairs(a.strips) do
    local t = file ~= "__install_s" and find_target(file)
    if t then t.strip = true end
    -- strip of the installed copy: /system/programs/aclock
    if not t and file ~= "__install_s" then
      for _, tt in ipairs(a.targets) do
        if base(file) == (tt.output_name or base(tt.out)) then tt.strip = true end
      end
    end
  end

  -- Each target's call.
  local resources_done = false
  for _, t in ipairs(a.targets) do
    local args = { t.name }
    local outname = t.output_name or base(t.out)
    if t.kind == "library" then
      vim.list_extend(args, { "SONAME", q(t.soname) })
    elseif t.kind == "static" then
      if a.kernel_cflags then args[#args + 1] = "KERNEL" end
      local want = "lib" .. t.name:gsub("%-kernel$", "") .. ".a"
      if outname ~= want then vim.list_extend(args, { "OUTPUT_NAME", q(outname:gsub("^lib", ""):gsub("%.a$", "")) }) end
    elseif outname ~= t.name then
      if outname:find("%s") then
        t.spaced_name = ("set_target_properties(%s PROPERTIES OUTPUT_NAME %s)"):format(t.name, q(outname))
      else
        vim.list_extend(args, { "OUTPUT_NAME", q(outname) })
      end
    end
    -- sources
    local srcs = {}
    for _, s in ipairs(t.srcs) do srcs[#srcs + 1] = q(s) end
    for _, o in ipairs(t.objs) do
      local s = mk:source_for(o)
      if s then srcs[#srcs + 1] = q(s)
      else
        local guess = norm(o):gsub("%.o$", ".c")
        if objdir ~= "" then guess = guess:gsub("^" .. vim.pesc(objdir) .. "/", "") end
        srcs[#srcs + 1] = guess
        todo[#todo + 1] = "no source found for " .. o .. " (guessed " .. guess .. ")"
      end
    end
    local dedup, seen = {}, {}
    for _, s in ipairs(srcs) do if not seen[s] then seen[s] = true; dedup[#dedup + 1] = s end end
    args[#args + 1] = "SOURCES"
    vim.list_extend(args, dedup)
    for _, src in ipairs(dedup) do
      if src:match("%.[sS]$") then
        -- preprocessed assembler, as makefile.rules builds .s files
        pre[#pre + 1] = ("set_source_files_properties(%s PROPERTIES LANGUAGE C COMPILE_OPTIONS \"-x;assembler-with-cpp\")"):format(src)
      end
    end
    if #includes > 0 then args[#args + 1] = "INCLUDES"; vim.list_extend(args, includes) end
    if #defines > 0 then args[#args + 1] = "DEFINES"; vim.list_extend(args, defines) end
    local opts = {}
    for _, o in ipairs(options) do
      -- syllable_static_library(KERNEL) adds -ffreestanding itself
      if not (o == "-ffreestanding" and t.kind == "static" and a.kernel_cflags) then opts[#opts + 1] = o end
    end
    for _, o in ipairs(t.opts) do
      if o:match("^%-L") then
        -- library search paths: CMake links targets by name
      elseif o:match("^<shell:") then
        if not o:match("freetype") then todo[#todo + 1] = "link flags from `" .. o:match("^<shell:(.*)>$") .. "`" end
      elseif not FLAG_DROP[o] and not o:match("^%-[IDO]") and not o:match("^%-W[^l]") and o ~= "-Bsymbolic"
          and not o:match("^%-g") and o ~= "-nostdlib" and o ~= "-static" and o ~= "-s" and not o:match("^%-f")
          and not o:match("^%-m") and not o:match("^%-std") then
        todo[#todo + 1] = t.name .. ": link option " .. o
      end
      if o == "-s" then t.strip = true end
    end
    if #opts > 0 then args[#args + 1] = "OPTIONS"; vim.list_extend(args, opts) end
    local libs = {}
    for _, l in ipairs(t.libs) do
      if own[l] and own[l] ~= t.name then libs[#libs + 1] = own[l]
      elseif LIB_MAP[l] then libs[#libs + 1] = LIB_MAP[l]
      elseif not LIB_DROP[l] then libs[#libs + 1] = l end
      if l == "freetype" then freetype = true end
    end
    for _, e in ipairs(t.extra) do
      local n = cmake_target_name(e):gsub("^lib", "")
      if own[n] then libs[#libs + 1] = own[n] else todo[#todo + 1] = t.name .. " links " .. e end
    end
    if #libs > 0 and t.kind ~= "static" then args[#args + 1] = "LIBS"; vim.list_extend(args, libs) end
    -- LINKS: ln -s soname libfoo.so in the library's folder
    if t.kind == "library" then
      local ls = {}
      for _, l in ipairs(a.links) do
        if base(l.target) == t.soname then
          ls[#ls + 1] = q(base(l.link))
          l.done = true
        end
      end
      if #ls > 0 then args[#args + 1] = "LINKS"; vim.list_extend(args, ls) end
    end
    if t.strip then args[#args + 1] = "STRIP" end
    if t.install then
      vim.list_extend(args, { "INSTALL", q(t.install) })
    elseif t.kind ~= "static" then
      todo[#todo + 1] = t.name .. ": where it's installed (no cp/install of it found); add INSTALL <folder>"
    end
    local fn = ({ program = "syllable_program", driver = "syllable_driver", library = "syllable_library",
      static = "syllable_static_library", plugin = "syllable_plugin" })[t.kind]
    out[#out + 1] = call(fn, args)
    if t.spaced_name then out[#out + 1] = t.spaced_name end
    if freetype then out[#out + 1] = ("add_dependencies(%s freetype)"):format(t.name) end

    -- resources and catalogs (on the target the Makefile rescopies into)
    local res = a.resources[t.out] or a.resources[base(t.out)] or a.resources[outname]
    if not res then
      for file, r in pairs(a.resources) do
        if find_target(file) == t then res = r end
      end
    end
    if res and (#res.files > 0 or #res.add > 0) then
      local rargs = { t.name }
      if #res.files > 0 then
        rargs[#rargs + 1] = "FILES"
        vim.list_extend(rargs, files_arg(mk, out, res.files))
      end
      if #res.add > 0 then
        rargs[#rargs + 1] = "ADD"
        for _, s in ipairs(res.add) do rargs[#rargs + 1] = q(s) end
      end
      out[#out + 1] = call("syllable_resources", rargs)
      resources_done = true
    end
    local cat = a.catalogs
    if cat.name and not cat.done and (res or #a.targets == 1) then
      cat.done = true
      local cargs = { t.name, "NAME", q(cat.name), "DIR", q(cat.dir) }
      if cat.ext and cat.ext ~= "cd" then vim.list_extend(cargs, { "EXT", cat.ext }) end
      if cat.langs and #cat.langs > 0 then
        cargs[#cargs + 1] = "LANGS"
        for _, l in ipairs(cat.langs) do cargs[#cargs + 1] = q(l) end
      end
      if cat.embed then cargs[#cargs + 1] = "EMBED" end
      if cat.install then vim.list_extend(cargs, { "INSTALL", q(cat.install) }) end
      out[#out + 1] = call("syllable_catalogs", cargs)
      -- translations the Makefile didn't build
      local have = {}
      for _, l in ipairs(cat.langs or {}) do have[l] = true end
      local extra = {}
      for n, ty in vim.fs.dir(dir .. "/" .. cat.dir) do
        if ty == "directory" and not have[n] and vim.uv.fs_stat(("%s/%s/%s/%s.%s"):format(dir, cat.dir, n, cat.name, cat.ext or "cd")) then
          extra[#extra + 1] = n
        end
      end
      table.sort(extra)
      if #extra > 0 then
        todo[#todo + 1] = ("%s/ also has translations the Makefile didn't build: %s (add them to LANGS to build them)")
          :format(cat.dir, table.concat(extra, " "))
      end
    end
    -- attributes, on the installed file
    for _, at in ipairs(a.attrs) do
      if find_target(at.file) == t or base(at.file) == outname then
        if t.install then
          out[#out + 1] = call("syllable_attribute", { q(t.install .. "/" .. outname), q(at.name), q(at.value) })
        else
          todo[#todo + 1] = "addattrib " .. at.name .. " on " .. outname .. ": needs its INSTALL folder"
        end
        at.done = true
      end
    end
    out[#out + 1] = ""
  end
  local _ = resources_done
  if a.catalogs.name and not a.catalogs.done then
    todo[#todo + 1] = "catalogs " .. a.catalogs.name .. " (couldn't tell which target embeds them)"
  end
  for _, at in ipairs(a.attrs) do
    if not at.done then
      local dest
      for _, rec in ipairs(a.installs) do
        for _, s in ipairs(rec.srcs) do if base(s) == base(at.file) then dest = image_path(rec.dest) end end
      end
      if dest then
        local p = KNOWN_DIRS[base(dest)] and (dest .. "/" .. base(at.file)) or dest
        post[#post + 1] = call("syllable_attribute", { q(p), q(at.name), q(at.value) })
      else
        todo[#todo + 1] = "addattrib " .. at.file .. " " .. at.name .. " " .. at.value
      end
    end
  end

  -- Symlinks made at install time (other than a library's LINKS).
  for _, l in ipairs(a.links) do
    if not l.done then
      local link = image_path(l.link)
      local tgt = l.target
      if tgt:find("^<") or link == "" then
        todo[#todo + 1] = "ln -s " .. l.target .. " " .. l.link
      else
        if KNOWN_DIRS[base(link)] and not l.link:match("/[^/]*%.[^/]*$") then link = link .. "/" .. base(tgt) end
        local ldir = vim.fs.dirname(link)
        post[#post + 1] = ([[install(CODE "file(MAKE_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/%s)
  file(CREATE_LINK \"%s\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/%s\" SYMBOLIC)")]]):format(ldir, tgt, link)
      end
    end
  end

  if #a.targets == 0 and #a.subdirs == 0 and #post == 0 then
    todo[#todo + 1] = "no compile or link commands found: this Makefile may build nothing, or build in a way this conversion doesn't read"
  end

  -- Subdirectories
  local subs, missing = {}, {}
  for _, d in ipairs(a.subdirs) do
    if vim.uv.fs_stat(dir .. "/" .. d) then
      local has = vim.uv.fs_stat(dir .. "/" .. d .. "/CMakeLists.txt") ~= nil
      if not has then missing[#missing + 1] = d end
      subs[#subs + 1] = "add_subdirectory(" .. q(d) .. ")" .. (has and "" or "  # no CMakeLists.txt yet: convert its Makefile too")
    else
      todo[#todo + 1] = "subdirectory " .. d .. " (not found)"
    end
  end

  for _, o in ipairs(a.others) do todo[#todo + 1] = "command not converted: " .. o end
  if mk.undefined then
    local u = vim.tbl_keys(mk.undefined)
    table.sort(u)
    local skip = { CFLAGS = 1, CXXFLAGS = 1, CPPFLAGS = 1, AFLAGS = 1, LDFLAGS = 1, OBJDIR = 1, TOP = 0, ATHEOS_INCLUDE_PATH = 1,
      SUBDIRS = 1, prefix = 1, DESTDIR = 1, LIB2 = 1 }
    local list = {}
    for _, n in ipairs(u) do if skip[n] ~= 1 then list[#list + 1] = "$(" .. n .. ")" end end
    if #list > 0 then todo[#todo + 1] = "set outside the Makefile, so left empty here: " .. table.concat(list, " ") end
  end

  -- Assemble.
  local lines = {}
  local name = base(dir)
  local comments = leading_comments(text)
  if #comments > 0 then
    for _, c in ipairs(comments) do lines[#lines + 1] = "# " .. c end
  else
    local what = a.targets[1] and ({ program = "program", driver = "kernel driver", library = "shared library",
      static = "static library", plugin = "add-on" })[a.targets[1].kind] or (#subs > 0 and "folders" or "build")
    lines[#lines + 1] = ("# %s: %s"):format(name, what)
  end
  lines[#lines + 1] = "# Converted from its Makefile (Neovim: Convert to CMakeLists.txt). Check it, then"
  lines[#lines + 1] = "# add_subdirectory() it from the CMakeLists.txt above if that doesn't yet."
  local dedup_todo, seen_todo = {}, {}
  for _, t in ipairs(todo) do if not seen_todo[t] then seen_todo[t] = true; dedup_todo[#dedup_todo + 1] = t end end
  for _, t in ipairs(dedup_todo) do lines[#lines + 1] = "# TODO(make): " .. t end
  lines[#lines + 1] = ""
  for _, l in ipairs(pre) do lines[#lines + 1] = l end
  for _, l in ipairs(out) do lines[#lines + 1] = l end
  while lines[#lines] == "" do lines[#lines] = nil end
  if #post > 0 then
    lines[#lines + 1] = ""
    local seen_post = {}
    for _, l in ipairs(post) do
      if not seen_post[l] then seen_post[l] = true; lines[#lines + 1] = l end
    end
  end
  if #subs > 0 then
    lines[#lines + 1] = ""
    for _, l in ipairs(subs) do lines[#lines + 1] = l end
  end

  -- Is this folder in the build? The parent's CMakeLists.txt should name it.
  local parent_cm = vim.fs.dirname(dir) .. "/CMakeLists.txt"
  local parent_missing
  if vim.uv.fs_stat(parent_cm) then
    local ptext = table.concat(vim.fn.readfile(parent_cm), "\n")
    if not ptext:find("add_subdirectory%s*%(%s*" .. vim.pesc(name) .. "[%s%)]")
        and not ptext:find("%f[%w_]" .. vim.pesc(name) .. "%f[^%w_%-%.]") then
      parent_missing = name
    end
  end

  return table.concat(lines, "\n") .. "\n", {
    todo = dedup_todo, subdirs_missing = missing, parent = parent_cm, parent_missing = parent_missing,
    checkout = root, targets = a.targets,
  }
end

---------------------------------------------------------------------------
-- Neovim side

local function is_makefile(path)
  local n = base(path or "")
  return n == "Makefile" or n == "makefile" or n == "GNUmakefile" or n:match("%.mk$") ~= nil
    or n:match("^Makefile%.") ~= nil
end
M.is_makefile = is_makefile

local function summary(notes, target)
  local parts = { "Wrote " .. vim.fn.fnamemodify(target, ":~:.") }
  if #notes.todo > 0 then
    parts[#parts + 1] = ("%d TODO%s to check (at the top)"):format(#notes.todo, #notes.todo == 1 and "" or "s")
  end
  if notes.parent_missing then
    parts[#parts + 1] = ("%s doesn't add_subdirectory(%s) yet"):format(vim.fn.fnamemodify(notes.parent, ":~:."), notes.parent_missing)
  end
  if #notes.subdirs_missing > 0 then
    parts[#parts + 1] = "subfolders to convert too: " .. table.concat(notes.subdirs_missing, ", ")
  end
  if not notes.checkout then parts[#parts + 1] = "(not in a Syllable checkout: cmake/Syllable.cmake wasn't found above it)" end
  return table.concat(parts, "\n")
end

local function write(target, text)
  local f = assert(io.open(target, "w"))
  f:write(text)
  f:close()
end

-- Open the existing CMakeLists.txt and the converted one side by side in a
-- diff; :w in the converted one replaces the file.
local function compare(target, text, notes)
  vim.cmd("tabnew " .. vim.fn.fnameescape(target))
  vim.cmd("diffthis")
  vim.cmd("rightbelow vnew")
  local buf = vim.api.nvim_get_current_buf()
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, vim.split(text:gsub("\n$", ""), "\n"))
  vim.bo[buf].buftype = "acwrite"
  vim.bo[buf].bufhidden = "wipe"
  vim.bo[buf].filetype = "cmake"
  vim.bo[buf].modified = false
  pcall(vim.api.nvim_buf_set_name, buf, target .. " (converted from Makefile)")
  vim.api.nvim_create_autocmd("BufWriteCmd", {
    buffer = buf,
    callback = function()
      write(target, table.concat(vim.api.nvim_buf_get_lines(buf, 0, -1, false), "\n") .. "\n")
      vim.bo[buf].modified = false
      for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
        local b = vim.api.nvim_win_get_buf(w)
        if vim.api.nvim_buf_get_name(b) == target then
          vim.api.nvim_buf_call(b, function() vim.cmd("checktime") ; vim.cmd("edit!") end)
        end
      end
      vim.notify(summary(notes, target), vim.log.levels.INFO, { title = "Makefile to CMake" })
    end,
  })
  vim.cmd("diffthis")
  vim.notify("Converted CMakeLists.txt on the right, the current one on the left. :w on the right replaces it.",
    vim.log.levels.INFO, { title = "Makefile to CMake" })
end

-- Convert the Makefile at path (default: the current buffer) and open the result.
function M.run(path)
  path = path and path ~= "" and vim.fn.fnamemodify(path, ":p") or vim.api.nvim_buf_get_name(0)
  if path == "" or vim.fn.filereadable(path) ~= 1 then
    vim.notify("Convert to CMakeLists.txt: open or pick a Makefile first", vim.log.levels.WARN, { title = "Makefile to CMake" })
    return
  end
  -- unsaved edits in the Makefile count
  local mbuf = vim.fn.bufnr(path)
  if mbuf > 0 and vim.bo[mbuf].modified then
    vim.api.nvim_buf_call(mbuf, function() vim.cmd("silent write") end)
  end
  local ok, text, notes = pcall(M.convert, path)
  if not ok then
    vim.notify("Couldn't convert " .. vim.fn.fnamemodify(path, ":~:.") .. ":\n" .. tostring(text), vim.log.levels.ERROR,
      { title = "Makefile to CMake" })
    return
  end
  local target = vim.fs.dirname(path) .. "/CMakeLists.txt"
  local function open()
    -- in the code window, not the tree
    local cur = vim.api.nvim_get_current_win()
    if vim.api.nvim_win_get_config(cur).relative ~= "" or vim.bo.filetype:match("^snacks") then
      for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
        if vim.api.nvim_win_get_config(w).relative == "" and not vim.bo[vim.api.nvim_win_get_buf(w)].filetype:match("^snacks") then
          vim.api.nvim_set_current_win(w)
          break
        end
      end
    end
    vim.cmd("edit " .. vim.fn.fnameescape(target))
    vim.cmd("checktime")
  end
  if not vim.uv.fs_stat(target) then
    write(target, text)
    open()
    vim.notify(summary(notes, target), vim.log.levels.INFO, { title = "Makefile to CMake" })
    return
  end
  vim.ui.select({ "Compare with the current one", "Replace it", "Cancel" },
    { prompt = vim.fn.fnamemodify(target, ":~:.") .. " already exists" }, function(choice)
      if choice == "Replace it" then
        write(target, text)
        local b = vim.fn.bufnr(target)
        if b > 0 then vim.api.nvim_buf_call(b, function() vim.cmd("edit!") end) end
        open()
        vim.notify(summary(notes, target), vim.log.levels.INFO, { title = "Makefile to CMake" })
      elseif choice == "Compare with the current one" then
        compare(target, text, notes)
      end
    end)
end

local LABEL = [[Convert\ to\ CMakeLists.txt]]

function M.setup()
  if M._setup then return end
  M._setup = true
  vim.api.nvim_create_user_command("SyllableMakefileToCMake", function(o) M.run(o.args) end, {
    nargs = "?", complete = "file", desc = "Convert a Makefile to a CMakeLists.txt in Syllable's CMake DSL",
  })
  -- The project tree's right-click menu, on Makefiles.
  local okx, explorer = pcall(require, "config.explorer")
  if okx and explorer.add_menu_entries then
    explorer.add_menu_entries(function(item)
      if not item.file then return end
      if not item.dir then
        if is_makefile(item.file) and M.checkout(vim.fs.dirname(item.file)) then
          return { { "Convert to CMakeLists.txt", function() M.run(item.file) end } }
        end
        return
      end
      -- a folder with a Makefile in it
      for _, n in ipairs({ "Makefile", "makefile", "GNUmakefile" }) do
        local mf = item.file .. "/" .. n
        if vim.uv.fs_stat(mf) and M.checkout(item.file) then
          return { { "Convert " .. n .. " to CMakeLists.txt", function() M.run(mf) end } }
        end
      end
    end)
  end
  -- The editor's right-click menu, in a Makefile.
  vim.api.nvim_create_autocmd("MenuPopup", {
    group = vim.api.nvim_create_augroup("rick_popup_makefile_cmake", { clear = true }),
    callback = function()
      pcall(vim.cmd, "aunmenu PopUp." .. LABEL)
      local name = vim.api.nvim_buf_get_name(0)
      if (vim.bo.filetype == "make" or is_makefile(name)) and name ~= "" and M.checkout(vim.fs.dirname(name)) then
        vim.cmd(("anoremenu .504 PopUp.%s <Cmd>lua require('config.makefile_cmake').run()<CR>"):format(LABEL))
      end
    end,
  })
end

return M
