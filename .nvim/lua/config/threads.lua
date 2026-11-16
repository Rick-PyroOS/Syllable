-- Thread check: "does this need a mutex?" For each function that writes
-- shared data (members, globals, statics), find which threads can run it and
-- whether a lock is held, and mark the risky ones in the margin (M):
--
--   Runs on 2 threads (download_worker, main) and writes m_cResults, m_nTotal
--   with no lock. Guard them with m_cLock, as ClearResults does.
--
-- It's off unless a project turns it on, and it knows nothing about any
-- framework by itself: the project's .nvim.lua describes how its threads start
-- and how it locks, then calls setup(), which adds the keys, the margin check,
-- the Space tT toggle and the right-click entry. Syllable's is in
-- Syllable/.nvim.lua:
--   vim.g.thread_model = {
--     spawn = { "spawn_thread" },        -- f( "name", func, ... ) starts a thread running func
--     irq = { "request_irq" },           -- f( irq, handler, ... ): handler runs as an interrupt
--     loopers = { "Looper", "Window" },  -- classes with their own thread ...
--     looper_callbacks = { "HandleMessage" }, -- ... and the methods it calls on it
--     views = { "View" },                -- classes that belong to their window's thread ...
--     view_callbacks = { "Paint" },      -- ... and the methods that thread calls
--     handlers = { "Handler" },          -- classes whose callbacks run on the looper they joined ...
--     handler_callbacks = { "HandleMessage" }, -- ... and those callbacks
--     locks = { Lock = "Unlock" },       -- lock calls and the call that ends each
--     guards = { "Locker" },             -- types whose scope holds a lock (Lua patterns)
--     atomic = { "^atomic_" },           -- calls that are safe as they are (Lua patterns)
--     mutators = { "AddItem" },          -- container calls that write (STL ones are known)
--     member_prefix = "^m_", global_prefix = "^g_",
--     lock_hint = "an os::Locker member",  -- what to suggest when no lock is in sight
--     fix = {                            -- how to add one, for the "How to fix it" advice
--       name = "m_cLock",                -- the new member's name
--       decl = "os::Locker %s;",         -- its declaration (%s: the name)
--       init = '%s( "%s" )',             -- constructor initializer (name, class), or nil
--       guard = "os::AutoLocker cLock( &%s );", -- holds it to the end of the scope
--     },
--     view_fix = "Lock it first: ...",     -- the advice for a view touched off its thread
--   }
--   require("config.threads").setup()
--
-- How it knows (no model involved): main() and the spawned functions start
-- threads, and so do the loopers' and views' callbacks; clangd's call hierarchy
-- walks up from the function to those; a write is locked inside a guard's
-- scope, between a lock call and its unlock, or when every caller holds a lock
-- at the call. It also flags a view method called from a spawned thread
-- without the window locked.
--
-- Space cx (or right-click > Which Threads Run This?) shows the threads, the
-- paths to them, the writes and the locks for the function under the cursor.
-- Space tT turns the margin check on / off (remembered). Silence one with
-- "// nomodel: threads -- why it's fine" like the other M checks.

local M = {}
local ns = vim.api.nvim_create_namespace("rick_threads")
M.ns = ns
local MAX_DEPTH, MAX_NODES = 10, 300

local STL_MUTATORS = { "push_back", "push_front", "pop_back", "pop_front", "insert", "erase", "clear", "resize",
  "assign", "swap", "splice", "remove", "sort", "reserve", "emplace", "emplace_back", "emplace_front" }

local function set(list)
  local t = {}
  for _, v in ipairs(list or {}) do t[v] = true end
  return t
end

-- The project's thread model (vim.g.thread_model), or nil: then the check is off.
local cfg
local function load_model()
  local m = vim.g.thread_model
  if type(m) ~= "table" then return nil end
  return {
    spawn = m.spawn or {}, irq = m.irq or {},
    loopers = set(m.loopers), views = set(m.views), handlers = set(m.handlers),
    looper_callbacks = set(m.looper_callbacks), view_callbacks = set(m.view_callbacks),
    handler_callbacks = set(m.handler_callbacks),
    locks = m.locks or {}, guards = m.guards or { "Lock", "Guard", "lock_guard", "unique_lock", "scoped_lock" },
    atomic = m.atomic or {}, mutators = set(vim.list_extend(vim.deepcopy(STL_MUTATORS), m.mutators or {})),
    member_prefix = m.member_prefix, global_prefix = m.global_prefix,
    lock_hint = m.lock_hint or "a mutex", view_fix = m.view_fix or "Lock its window first.",
    fix = vim.tbl_extend("force", { name = "m_mutex", decl = "std::mutex %s;",
      guard = "std::lock_guard<std::mutex> guard( %s );" }, m.fix or {}),
  }
end

local function matches_any(name, patterns)
  for _, pat in ipairs(patterns) do
    if name:match(pat) then return true end
  end
  return false
end

-- Coroutines over clangd ------------------------------------------------------

local function await_request(client, method, params, buf)
  local co = coroutine.running()
  local done, e, r = false, nil, nil
  client:request(method, params, function(err, res)
    e, r = err, res
    if done then coroutine.resume(co) else done = true end
  end, buf)
  if not done then
    done = true
    coroutine.yield()
  end
  return e, r
end

local function clangd(buf)
  return vim.lsp.get_clients({ bufnr = buf, name = "clangd" })[1]
end

-- Source of a file: the loaded buffer if there is one, else the file on disk.
local trees = {}
local function source(path)
  local b = vim.fn.bufnr(path)
  if b > 0 and vim.api.nvim_buf_is_loaded(b) then
    local ok, parser = pcall(vim.treesitter.get_parser, b, "cpp")
    if ok and parser then
      return { root = parser:parse()[1]:root(), src = b, lines = vim.api.nvim_buf_get_lines(b, 0, -1, false) }
    end
  end
  local stat = vim.uv.fs_stat(path)
  if not stat then return nil end
  local key = path .. ":" .. stat.mtime.sec .. ":" .. stat.size
  if trees[key] then return trees[key] end
  local lines = vim.fn.readfile(path)
  local text = table.concat(lines, "\n")
  local ok, parser = pcall(vim.treesitter.get_string_parser, text, "cpp")
  if not ok then return nil end
  trees[key] = { root = parser:parse()[1]:root(), src = text, lines = lines }
  return trees[key]
end

local function text(node, s) return vim.treesitter.get_node_text(node, s.src) end

-- The function_definition around (row, col).
local function function_at(s, row, col)
  local node = s.root:named_descendant_for_range(row, col or 0, row, col or 0)
  while node and node:type() ~= "function_definition" do node = node:parent() end
  return node
end

-- "DownloadWindow::AddResult" -> class "DownloadWindow", name "AddResult";
-- methods defined inside the class body get the class from there.
local function names_of(fn, s)
  local d = fn:field("declarator")[1]
  while d and d:field("declarator")[1] and not d:type():match("identifier") and d:type() ~= "destructor_name" do
    d = d:field("declarator")[1]
  end
  if not d then return nil end
  local full = text(d, s):gsub("%s+", "")
  local cls, name = full:match("^(.*)::([~%w_]+)$")
  if not cls then
    name = full
    local p = fn:parent()
    while p do
      if p:type() == "class_specifier" or p:type() == "struct_specifier" then
        local n = p:field("name")[1]
        cls = n and text(n, s)
        break
      end
      p = p:parent()
    end
  end
  cls = cls and cls:match("([%w_]+)$")
  return { full = cls and (cls .. "::" .. name) or name, class = cls, name = name }
end

-- The project: entry points and class bases ----------------------------------
--
-- One rg over the project finds the thread starts and the class bases; it runs
-- once per project, off the UI thread, and a save rescans only the saved file
-- (a full rescan on every save stalled typing in a big tree).

local projects = {} -- root -> { files = { [path] = { spawned = {}, bases = {} } }, merged = {...} }

-- Runs in a coroutine: rg's output lines, without blocking Neovim.
local function rg(args)
  if vim.fn.executable("rg") ~= 1 then return {} end
  local co = coroutine.running()
  local cmd = { "rg", "--no-heading", "--with-filename", "-n", "-o", "-P", "--type", "cpp", "--type", "c" }
  vim.list_extend(cmd, args)
  vim.system(cmd, { text = true }, function(res) vim.schedule(function() coroutine.resume(co, res) end) end)
  local res = coroutine.yield()
  if res.code > 1 then return {} end
  return vim.split(res.stdout or "", "\n", { trimempty = true })
end

local function patterns()
  local out = {}
  -- spawn( "name", (void*)Func, ... ): Func starts a thread called name. The
  -- name may be worked out at run time (spawn( m_cName.c_str(), Func, ... )).
  if #cfg.spawn > 0 then
    out[#out + 1] = "(" .. table.concat(cfg.spawn, "|") .. [[)\s*\(\s*("[^"]*"|[^,()\s"]+(\(\))?)\s*,\s*(\([^)]*\)\s*)?&?[\w:]+]]
  end
  -- irq( n, Handler, ... ): Handler runs as an interrupt handler.
  if #cfg.irq > 0 then
    out[#out + 1] = "(" .. table.concat(cfg.irq, "|") .. [[)\s*\(\s*[^,]+,\s*&?[\w:]+]]
  end
  -- class X : public os::Window, ...  (for "is this a looper / a view?")
  out[#out + 1] = [[(class|struct)\s+\w+\s*:\s*[^{;]+]]
  return out
end

local spawn_set, irq_set
-- One rg match (path stripped) into a file's entry.
local function take(f, m)
  local call = m:match("^([%w_]+)%s*%(")
  if call and spawn_set[call] then
    local fname = m:match(",%s*[^,]-([%w_:]+)%s*$")
    local tname = m:match('^[%w_]+%s*%(%s*"([^"]*)"')
    if fname then
      fname = fname:match("([%w_]+)$")
      -- A name made at run time: call the thread after its function.
      f.spawned[fname] = { label = (tname or fname) .. " thread", named = tname ~= nil }
    end
  elseif call and irq_set[call] then
    local fname = m:match(",%s*&?([%w_:]+)%s*$")
    if fname then f.spawned[fname:match("([%w_]+)$")] = { label = "interrupt handler", named = true } end
  else
    local cls, bases = m:match("[cs][lt][ar][su][sc]t?%s+([%w_]+)%s*:%s*(.+)$")
    if cls then
      f.bases[cls] = f.bases[cls] or {}
      for b in bases:gmatch("([%w_:]+)%s*[,{]?") do
        local short = b:match("([%w_]+)$")
        if short ~= "public" and short ~= "private" and short ~= "protected" and short ~= "virtual" then
          table.insert(f.bases[cls], short)
        end
      end
    end
  end
end

-- rg over targets (the root, or one file): { [path] = { spawned, bases } }.
local function scan(targets)
  spawn_set, irq_set = set(cfg.spawn), set(cfg.irq)
  local args = {}
  for _, pat in ipairs(patterns()) do vim.list_extend(args, { "-e", pat }) end
  vim.list_extend(args, targets)
  local files = {}
  for _, l in ipairs(rg(args)) do
    local path, m = l:match("^(.-):%d+:(.*)$")
    if path then
      files[path] = files[path] or { spawned = {}, bases = {} }
      take(files[path], m)
    end
  end
  return files
end

local function merge(pr)
  local p = { spawned = {}, bases = {} }
  for _, f in pairs(pr.files) do
    for k, v in pairs(f.spawned) do p.spawned[k] = v end
    for k, v in pairs(f.bases) do p.bases[k] = v end
  end
  pr.merged = p
end

-- Runs in a coroutine. The project's entry points and class bases.
local function scan_project(root)
  local key = table.concat(cfg.spawn, ",") .. ":" .. table.concat(cfg.irq, ",")
  local pr = projects[root]
  if pr and pr.key == key then
    if pr.waiters then -- a scan is running: wait for it
      local co = coroutine.running()
      table.insert(pr.waiters, co)
      coroutine.yield()
    end
    return pr.merged
  end
  pr = { key = key, files = {}, waiters = {} }
  projects[root] = pr
  pr.files = scan({ root })
  merge(pr)
  local waiters = pr.waiters
  pr.waiters = nil
  for _, co in ipairs(waiters) do coroutine.resume(co) end
  return pr.merged
end

-- Runs in a coroutine: after a save, rescan just that file in any project it's in.
local function rescan_file(path)
  for root, pr in pairs(projects) do
    if not pr.waiters and vim.startswith(path, root .. "/") then
      pr.files[path] = nil
      for fp, f in pairs(scan({ path })) do pr.files[fp] = f end
      merge(pr)
    end
  end
end

-- "looper", "view" or nil for a class, following bases within the project.
local function kind_of(p, cls, seen)
  if not cls then return nil end
  if cfg.loopers[cls] then return "looper" end
  if cfg.views[cls] then return "view" end
  seen = seen or {}
  if seen[cls] then return nil end
  seen[cls] = true
  for _, b in ipairs(p.bases[cls] or {}) do
    local k = kind_of(p, b, seen)
    if k then return k end
  end
  if cfg.handlers[cls] then return "handler" end
end

-- Which thread a function starts, if it's an entry point, and whether it is a
-- thread that isn't a window's (a spawned one or an interrupt handler).
local function entry_thread(p, n)
  if not n then return nil end
  if n.name == "main" and not n.class then return "main thread" end
  local sp = p.spawned[n.name]
  if sp then return sp.label, sp.named end
  local k = kind_of(p, n.class)
  if k == "looper" and cfg.looper_callbacks[n.name] then return n.class .. " thread" end
  if k == "view" and cfg.view_callbacks[n.name] then return n.class .. "'s window thread" end
  if k == "handler" and cfg.handler_callbacks[n.name] then return n.class .. "'s looper thread" end
end

-- Is cls the class base or one derived from it (within the project)?
local function derives(p, cls, base, seen)
  if cls == base then return true end
  seen = seen or {}
  if not cls or seen[cls] then return false end
  seen[cls] = true
  for _, b in ipairs(p.bases[cls] or {}) do
    if derives(p, b, base, seen) then return true end
  end
  return false
end

-- Inside one function: locks and writes -----------------------------------------

local function strip(s) return (s:gsub("^[&*%s]+", ""):gsub("%s+", "")) end

local function analyze(fn, s, globals)
  local body = fn:field("body")[1]
  local res = { locks = {}, writes = {}, uses = {}, calls_lock = false }
  if not body then return res end
  local last_row = body:end_()
  local statics = {}
  local unlock_calls = {}

  local function enclosing_block_end(node)
    local p = node:parent()
    while p and p:type() ~= "compound_statement" do p = p:parent() end
    return p and select(1, p:end_()) or last_row
  end

  local function call_name(call)
    local f = call:field("function")[1]
    if not f then return nil end
    if f:type() == "field_expression" then
      local field = f:field("field")[1]
      local obj = f:field("argument")[1]
      return field and text(field, s), obj and strip(text(obj, s))
    elseif f:type() == "identifier" or f:type() == "qualified_identifier" then
      local name = text(f, s):match("([%w_]+)$")
      local args = call:field("arguments")[1]
      local first = args and args:named_child(0)
      return name, first and strip(text(first, s)) or nil
    end
  end

  -- Pass 1: locks and statics.
  local function walk1(node)
    local t = node:type()
    if t == "declaration" then
      local ty = node:field("type")[1]
      local tyname = ty and text(ty, s):match("([%w_]+)$") or ""
      local storage = false
      for c in node:iter_children() do
        if c:type() == "storage_class_specifier" and text(c, s) == "static" then storage = true end
      end
      for c in node:iter_children() do
        if c:type() == "init_declarator" then
          local d = c:field("declarator")[1]
          if storage and d then statics[text(d, s):match("([%w_]+)") or ""] = true end
          if matches_any(tyname, cfg.guards) then
            local v = c:field("value")[1]
            local arg = v and v:named_child(0)
            local name = arg and strip(text(arg, s)) or tyname
            local row = node:start()
            res.locks[#res.locks + 1] = { name = name, from = row, to = enclosing_block_end(node), how = tyname }
          end
        elseif c:type() == "function_declarator" and matches_any(tyname, cfg.guards) then
          -- "Locker cLock( m_cLock );" parses as a function declaration (C++'s most vexing parse)
          local params = c:field("parameters")[1]
          local arg = params and params:named_child(0)
          res.locks[#res.locks + 1] = { name = arg and strip(text(arg, s)) or tyname, from = node:start(),
            to = enclosing_block_end(node), how = tyname }
        elseif storage and c:type() == "identifier" then
          statics[text(c, s)] = true
        end
      end
    elseif t == "call_expression" then
      local name, obj = call_name(node)
      if name and cfg.locks[name] then
        res.locks[#res.locks + 1] = { name = obj or "the looper", from = node:start(), to = nil, close = cfg.locks[name],
          how = name .. "()" }
        res.calls_lock = true
      elseif name then
        for _, u in pairs(cfg.locks) do
          if u == name then unlock_calls[#unlock_calls + 1] = { name = obj or "the looper", row = node:start() } end
        end
      end
    end
    for c in node:iter_children() do walk1(c) end
  end
  walk1(body)
  for _, l in ipairs(res.locks) do
    if not l.to then
      l.to = last_row
      for _, u in ipairs(unlock_calls) do
        if u.row > l.from and u.name == l.name then l.to = u.row; break end
      end
    end
  end

  local function lock_at(row)
    for _, l in ipairs(res.locks) do
      if row >= l.from and row <= l.to then return l.name end
    end
  end

  -- What a write's target is: a member, a global or a static (else a local, ignored).
  local function target(node)
    local t = node:type()
    if t == "parenthesized_expression" or t == "pointer_expression" or t == "subscript_expression" then
      local inner = node:field("argument")[1] or node:named_child(0)
      return inner and target(inner)
    elseif t == "field_expression" then
      local obj, field = node:field("argument")[1], node:field("field")[1]
      local fname = field and text(field, s) or "?"
      if obj and text(obj, s) == "this" then return { kind = "member", name = fname } end
      if cfg.member_prefix and fname:match(cfg.member_prefix) then
        return { kind = "member", name = fname, via = obj and strip(text(obj, s)) }
      end
      return obj and target(obj)
    elseif t == "identifier" then
      local name = text(node, s)
      if statics[name] then return { kind = "static", name = name } end
      if globals[name] or (cfg.global_prefix and name:match(cfg.global_prefix)) then return { kind = "global", name = name } end
      if cfg.member_prefix and name:match(cfg.member_prefix) then return { kind = "member", name = name } end
    elseif t == "qualified_identifier" then
      return { kind = "global", name = text(node, s) }
    end
  end

  local function add_write(node, at)
    local tg = target(node)
    if not tg then return end
    local row = at:start()
    tg.row, tg.lock = row, lock_at(row)
    res.writes[#res.writes + 1] = tg
  end

  -- Pass 2: writes.
  local function walk2(node)
    local t = node:type()
    if t == "assignment_expression" then
      local left = node:field("left")[1]
      if left then add_write(left, node) end
    elseif t == "update_expression" then
      local arg = node:field("argument")[1]
      if arg then add_write(arg, node) end
    elseif t == "call_expression" then
      local f = node:field("function")[1]
      local name = select(1, call_name(node))
      if name and matches_any(name, cfg.atomic) then return end -- atomic_add( &m_n, 1 ) is fine as it is
      if f and f:type() == "field_expression" and name and cfg.mutators[name] then
        local obj = f:field("argument")[1]
        if obj then add_write(obj, node) end
      end
    end
    for c in node:iter_children() do walk2(c) end
  end
  walk2(body)

  -- Pass 3: every use (read or write) of this object's members: m_x, this->x.
  local function walk3(node)
    local t = node:type()
    local name
    if t == "identifier" and cfg.member_prefix then
      name = text(node, s)
      if not name:match(cfg.member_prefix) then name = nil end
    elseif t == "field_expression" then
      local obj, field = node:field("argument")[1], node:field("field")[1]
      if obj and field and text(obj, s) == "this" then name = text(field, s) end
    end
    if name then
      local row = node:start()
      res.uses[#res.uses + 1] = { name = name, row = row, lock = lock_at(row) }
    end
    for c in node:iter_children() do walk3(c) end
  end
  walk3(body)
  return res
end

-- File-scope variables (for "is this a global?").
local function globals_of(s)
  local g = {}
  for c in s.root:iter_children() do
    if c:type() == "declaration" then
      for d in c:iter_children() do
        local id = (d:type() == "init_declarator" and d:field("declarator")[1]) or (d:type() == "identifier" and d)
        if id then g[text(id, s):match("([%w_]+)") or ""] = true end
      end
    end
  end
  return g
end

-- Walking up the callers ---------------------------------------------------------

-- clangd lists Base::Method( ... ) inside a subclass as a call of every
-- override of Method. Such a call names its class, so it can only reach this
-- function when that class is this function's class or derives from it.
local function base_call(p, n, call)
  if not (n and n.class) then return false end
  local s = source(vim.uri_to_fname(call.from.uri))
  if not s then return false end
  for _, r in ipairs(call.fromRanges or {}) do
    local line = s.lines[r.start.line + 1] or ""
    local qual = line:sub(1, r.start.character):match("([%w_]+)%s*::%s*$")
    if not qual or qual == n.class or derives(p, qual, n.class) then return false end
  end
  return #(call.fromRanges or {}) > 0
end

-- The same method in the class's bases (Base::Method), as call-hierarchy
-- items: clangd lists a call through a base pointer (pcBase->Method()) as a
-- call of Base::Method only, not of the override that it reaches.
local overridden_cache = {}
local function overridden(client, buf, p, n)
  if not (n and n.class) then return {} end
  local key = n.class .. "::" .. n.name
  if overridden_cache[key] then return overridden_cache[key] end
  local items, seen = {}, {}
  local function up(cls)
    for _, b in ipairs(p.bases[cls] or {}) do
      if not seen[b] then
        seen[b] = true
        local err, syms = await_request(client, "workspace/symbol", { query = b .. "::" .. n.name }, buf)
        for _, sym in ipairs(not err and syms or {}) do
          local cont = (sym.containerName or ""):match("([%w_]+)$")
          if sym.name == n.name and cont == b and sym.location and sym.location.range then
            -- The range starts at the declaration ("virtual ..."): ask at the name.
            local st = sym.location.range.start
            local src = source(vim.uri_to_fname(sym.location.uri))
            local line = src and src.lines[st.line + 1] or ""
            local at = line:find(n.name .. "%s*%(", st.character + 1)
            local pos = { line = st.line, character = at and at - 1 or st.character }
            -- clangd only answers this for a file it has open: load it (hidden).
            local hb = vim.uri_to_bufnr(sym.location.uri)
            if not vim.lsp.buf_is_attached(hb, client.id) then
              vim.fn.bufload(hb)
              vim.bo[hb].bufhidden = "hide"
              vim.lsp.buf_attach_client(hb, client.id)
            end
            local e, its = await_request(client, "textDocument/prepareCallHierarchy",
              { textDocument = { uri = sym.location.uri }, position = pos }, hb)
            if not e and its and its[1] then items[#items + 1] = its[1] end
          end
        end
        up(b)
      end
    end
  end
  up(n.class)
  overridden_cache[key] = items
  return items
end

-- Runs in a coroutine. Returns { threads = { [label] = path }, unknown = { names },
-- callers_locked = lock name or nil, edges = n }.
local function reach(client, buf, item, p)
  local result = { threads = {}, unknown = {}, edges = 0, foreign = {} }
  local seen, nodes = {}, 0
  local queue = { { item = item, path = { (item.name:match("([~%w_]+)$")) or item.name }, depth = 0 } }
  local first = true
  local held_all, any_edge = true, false
  while #queue > 0 do
    local cur = table.remove(queue, 1)
    local key = cur.item.uri .. ":" .. cur.item.selectionRange.start.line
    if not seen[key] then
      seen[key] = true
      nodes = nodes + 1
      local path = vim.uri_to_fname(cur.item.uri)
      local s = source(path)
      local fn = s and function_at(s, cur.item.selectionRange.start.line, cur.item.selectionRange.start.character)
      local n = fn and names_of(fn, s)
      local th, foreign = entry_thread(p, n)
      if foreign then result.foreign[th] = true end
      if th then
        result.threads[th] = result.threads[th] or table.concat(vim.fn.reverse(vim.deepcopy(cur.path)), " → ")
      end
      -- A thread's start (main, a spawned function, an interrupt handler, a
      -- looper's or view's callback) is where the walk ends: clangd lists the
      -- spawn line as a "caller", but that only hands the function to the new
      -- thread; and above a callback is the framework's dispatch, which clangd
      -- links to every class's callback of that name.
      if cur.depth < MAX_DEPTH and nodes < MAX_NODES and not th then
        local err, calls = await_request(client, "callHierarchy/incomingCalls", { item = cur.item }, buf)
        calls = not err and calls or {}
        for _, base in ipairs(overridden(client, buf, p, n)) do
          local e2, more = await_request(client, "callHierarchy/incomingCalls", { item = base }, buf)
          vim.list_extend(calls, not e2 and more or {})
        end
        calls = vim.tbl_filter(function(call) return not base_call(p, n, call) end, calls)
        if #calls == 0 then
          result.unknown[#result.unknown + 1] = n and n.full or cur.item.name
        end
        for _, call in ipairs(calls) do
          if first then
            -- Is a lock held where the direct caller calls us?
            any_edge = true
            result.edges = result.edges + 1
            local cs = source(vim.uri_to_fname(call.from.uri))
            local cfn = cs and function_at(cs, call.from.selectionRange.start.line, call.from.selectionRange.start.character)
            local held
            if cfn then
              local a = analyze(cfn, cs, globals_of(cs))
              for _, r in ipairs(call.fromRanges or {}) do
                for _, l in ipairs(a.locks) do
                  if r.start.line >= l.from and r.start.line <= l.to then held = l.name end
                end
              end
            end
            if not held then held_all = false else result.callers_locked = held end
          end
          local cn = call.from.name
          queue[#queue + 1] = { item = call.from, path = vim.list_extend(vim.deepcopy(cur.path), { cn }), depth = cur.depth + 1 }
        end
      end
      first = false
    end
  end
  if not (any_edge and held_all) then result.callers_locked = nil end
  return result
end

local function prepare(client, buf, row, col)
  local params = { textDocument = { uri = vim.uri_from_bufnr(buf) }, position = { line = row, character = col } }
  local err, items = await_request(client, "textDocument/prepareCallHierarchy", params, buf)
  return not err and items and items[1] or nil
end

-- Everything about one function: its threads, writes and locks.
local function study(client, buf, fn, s, p, globals)
  local n = names_of(fn, s)
  local d = fn:field("declarator")[1]
  while d and d:field("declarator")[1] do d = d:field("declarator")[1] end
  if not (n and d) then return nil end
  local row, col = d:range()
  -- Put the cursor on the name itself (after "Class::").
  local line = s.lines[row + 1] or ""
  local at = line:find(n.name, col + 1, true)
  local item = prepare(client, buf, row, at and at - 1 or col)
  local info = { name = n, row = fn:start(), name_row = row, local_ = analyze(fn, s, globals), fn = fn }
  info.kind = kind_of(p, n.class)
  if item then info.reach = reach(client, buf, item, p) end
  return info
end

local function list(t)
  local out = {}
  for k in pairs(t) do out[#out + 1] = k end
  table.sort(out)
  return out
end

-- A member written with no lock by a function that only one thread runs can
-- still race: another function of the class may use it, also with no lock, on
-- another thread (a handler's callback rebuilds a cache that a window's
-- callback reads). For one writer: { members = { names }, other = info,
-- thread = the other thread, mine = the writer's thread } or nil. more(info)
-- walks a function's callers when it hasn't been yet (in a coroutine).
local function cross(info, all, more)
  local r = info.reach
  if not (r and info.name.class) or r.callers_locked then return nil end
  local cls = info.name.class
  -- Threads that reach it only while the object is being made or destroyed
  -- (through its constructor or destructor) don't count: nothing else can
  -- see the object yet, or any more.
  local function live(reach)
    return vim.tbl_filter(function(t)
      for step in (reach.threads[t] .. " → "):gmatch("(.-) → ") do
        if step == cls or step == "~" .. cls then return false end
      end
      return true
    end, list(reach.threads))
  end
  local mine = live(r)
  if #mine ~= 1 then return nil end -- none known, or judge() already covers it
  local written, order = {}, {}
  for _, w in ipairs(info.local_.writes) do
    if w.kind == "member" and not w.via and not w.lock and not written[w.name] then
      written[w.name] = true
      order[#order + 1] = w.name
    end
  end
  if #order == 0 then return nil end
  for _, o in ipairs(all) do
    if o ~= info and o.name.class == info.name.class then
      local hit = {}
      for _, u in ipairs(o.local_.uses or {}) do
        if written[u.name] and not u.lock and not vim.tbl_contains(hit, u.name) then hit[#hit + 1] = u.name end
      end
      if #hit > 0 then
        if not o.reach and o.fn then more(o) end
        local or_ = o.reach
        if or_ and not or_.callers_locked then
          for _, t in ipairs(live(or_)) do
            if t ~= mine[1] then
              return { members = hit, other = o, thread = t, mine = mine[1] }
            end
          end
        end
      end
    end
  end
end

-- Where a cross() finding is: the writer's first unlocked write of the
-- members, and the other function's first unlocked use of them (0-based rows).
local function cross_rows(info, c)
  local want = {}
  for _, m in ipairs(c.members) do want[m] = true end
  local w_row, u_row
  for _, w in ipairs(info.local_.writes) do
    if want[w.name] and not w.lock then w_row = w.row; break end
  end
  for _, u in ipairs(c.other.local_.uses or {}) do
    if want[u.name] and not u.lock then u_row = u.row; break end
  end
  return w_row, u_row
end

-- The lock to use: one this file already takes for these names, or a new member.
local function fix_lock(c, suggest)
  local lock, where = suggest(c.members)
  if lock and lock ~= "the looper" then return lock, where, false end
  return cfg.fix.name, nil, true
end

local function cross_message(info, c, suggest)
  local r = info.reach
  local names = table.concat(c.members, ", ")
  local lock, where, new = fix_lock(c, suggest)
  local w_row, u_row = cross_rows(info, c)
  local msg = ("Writes %s with no lock on the %s (%s), and %s uses %s with no lock on the %s (%s)."):format(
    names, c.mine, r.threads[c.mine], c.other.name.name, #c.members > 1 and "them" or "it", c.thread,
    c.other.reach.threads[c.thread])
  return msg .. (" Fix: %s, held around the write at line %d and %s's use at line %d (Space cx shows how)."):format(
    new and ("add a lock member (" .. cfg.fix.decl:format(lock) .. ")") or ("take " .. lock .. (where and (", as " .. where .. " does") or "")),
    (w_row or info.name_row) + 1, c.other.name.name, (u_row or c.other.row or 0) + 1)
end

-- Space cx's "How to fix it" lines for a cross() finding. lines(row) gives a
-- source line of the file (0-based row).
local function cross_fix(info, c, suggest, line_at)
  local cls = info.name.class
  local lock, _, new = fix_lock(c, suggest)
  local guard = cfg.fix.guard:format(lock)
  local w_row, u_row = cross_rows(info, c)
  local out = {}
  local function add(l) out[#out + 1] = l end
  local n = 1
  if new then
    add(("%d. Give %s a lock (in the class, next to %s):"):format(n, cls, c.members[1]))
    add("       " .. cfg.fix.decl:format(lock))
    if cfg.fix.init then
      add("   and set it up in each constructor's initializer list:")
      add("       " .. cfg.fix.init:format(lock, cls))
    end
    n = n + 1
  end
  local wline = w_row and vim.trim(line_at(w_row) or "") or ""
  add(("%d. In %s, do the slow work into a local first and hold the lock only"):format(n, info.name.name))
  add(("   for the write (line %d):"):format((w_row or info.name_row) + 1))
  add("       {")
  add("           " .. guard)
  add("           " .. wline)
  add("       }")
  n = n + 1
  local uline = u_row and vim.trim(line_at(u_row) or "") or ""
  add(("%d. In %s, hold it for as long as it uses %s (line %d):"):format(n, c.other.name.name,
    table.concat(c.members, ", "), (u_row or 0) + 1))
  add("           " .. guard)
  add("           " .. uline)
  add("   or copy under the lock and work on the copy, if the work is slow.")
  add("The check sees the lock once it's there (guards / locks in the thread model).")
  return out
end

-- Findings for one studied function.
local function judge(info, suggest)
  local out = {}
  local r = info.reach
  if not r then return out end
  local threads = list(r.threads)
  local unlocked, seen = {}, {}
  for _, w in ipairs(info.local_.writes) do
    if not w.lock and not r.callers_locked and not seen[w.name] then
      seen[w.name] = true
      unlocked[#unlocked + 1] = w
    end
  end
  if #threads >= 2 and #unlocked > 0 then
    local names = vim.tbl_map(function(w) return w.name end, unlocked)
    local lock, where = suggest(names)
    local shown = #threads > 4 and vim.list_slice(threads, 1, 3) or threads
    local more = #threads > #shown and (" and " .. (#threads - #shown) .. " more") or ""
    local msg = ("Runs on %d threads (%s%s) and writes %s with no lock."):format(#threads, table.concat(shown, ", "),
      more, table.concat(names, ", "))
    msg = msg .. (lock and (" Guard " .. (#names > 1 and "them" or "it") .. " with " .. lock
      .. (where and (", as " .. where .. " does") or "") .. ".") or (" Guard " .. (#names > 1 and "them" or "it") .. " with a lock (" .. cfg.lock_hint .. ")."))
    out[#out + 1] = { row = unlocked[1].row, message = msg, severity = vim.diagnostic.severity.WARN }
  end
  -- A view touched from a thread that isn't a window thread, with nothing locked.
  if info.kind == "view" and not r.callers_locked and not info.local_.calls_lock then
    local foreign = vim.tbl_filter(function(t) return r.foreign[t] end, threads)
    if #foreign > 0 then
      out[#out + 1] = {
        row = info.name_row,
        severity = vim.diagnostic.severity.WARN,
        message = ("%s belongs to its window's thread, but the %s calls %s without locking the window (%s). %s")
          :format(info.name.class, foreign[1], info.name.name, r.threads[foreign[1]], cfg.view_fix),
      }
    end
  end
  return out
end

-- Running over a buffer ---------------------------------------------------------------

local running = {}

local function functions_of(s)
  local out = {}
  local function walk(node)
    if node:type() == "function_definition" then out[#out + 1] = node return end
    for c in node:iter_children() do walk(c) end
  end
  walk(s.root)
  return out
end

-- The lock that already guards one of these names somewhere in the file.
local function suggester(all)
  return function(names)
    local want = {}
    for _, n in ipairs(names) do want[n] = true end
    for _, a in ipairs(all) do
      for _, w in ipairs(a.local_.writes) do
        if want[w.name] and w.lock then return w.lock, a.name.name end
      end
    end
    for _, a in ipairs(all) do
      for _, l in ipairs(a.local_.locks) do
        if l.name ~= "the looper" then return l.name, nil end
      end
    end
  end
end

function M.check(buf)
  buf = buf or vim.api.nvim_get_current_buf()
  if running[buf] or not vim.api.nvim_buf_is_valid(buf) then return end
  cfg = load_model()
  if not (cfg and require("config.prefs").get("thread_check")) then return vim.diagnostic.reset(ns, buf) end
  local ft = vim.bo[buf].filetype
  if ft ~= "cpp" and ft ~= "c" then return end
  local client = clangd(buf)
  if not client then return end
  running[buf] = true
  coroutine.wrap(function()
    local ok, err = pcall(function()
      local s = source(vim.api.nvim_buf_get_name(buf))
      if not s then return end
      local p = scan_project(client.root_dir or vim.fn.getcwd())
      local globals = globals_of(s)
      local all = {}
      for _, fn in ipairs(functions_of(s)) do
        local a = analyze(fn, s, globals)
        local n = names_of(fn, s)
        local is_view = n and kind_of(p, n.class) == "view"
        -- Only functions that write shared data (or view methods) need the callers walk.
        if #a.writes > 0 or is_view then
          local info = study(client, buf, fn, s, p, globals)
          if info then all[#all + 1] = info end
        else
          all[#all + 1] = { name = n or { name = "?" }, local_ = a, fn = fn }
        end
      end
      local function more(o)
        local info = study(client, buf, o.fn, s, p, globals)
        if info then o.reach, o.kind = info.reach, info.kind end
      end
      local suggest = suggester(all)
      for _, info in ipairs(all) do
        local c = cross(info, all, more)
        if c then
          local first
          for _, w in ipairs(info.local_.writes) do
            if w.name == c.members[1] then first = w.row; break end
          end
          info.cross = { row = first or info.name_row, message = cross_message(info, c, suggest),
            severity = vim.diagnostic.severity.WARN }
        end
      end
      local silenced = require("config.mcp").silenced(buf, "nomodel")
      local items = {}
      for _, info in ipairs(all) do
        local found = judge(info, suggest)
        if info.cross then found[#found + 1] = info.cross end
        for _, f in ipairs(found) do
          local said = silenced(f.row, "threads")
          if said ~= true and vim.api.nvim_buf_is_valid(buf) then
            local line = vim.api.nvim_buf_get_lines(buf, f.row, f.row + 1, false)[1] or ""
            items[#items + 1] = {
              lnum = f.row, col = (line:find("%S") or 1) - 1, end_col = #line,
              severity = said and vim.diagnostic.severity.HINT or f.severity, message = f.message,
              source = "thread check",
              user_data = { scanner = true, mcp = true, checked = said or nil, severity = f.severity },
            }
          end
        end
      end
      if vim.api.nvim_buf_is_valid(buf) then vim.diagnostic.set(ns, buf, items) end
      M.last = all
    end)
    running[buf] = nil
    if not ok then vim.notify("Thread check: " .. tostring(err), vim.log.levels.WARN) end
  end)()
end

-- Space cx: the threads, paths, writes and locks for the function under the cursor.
function M.explain()
  local buf = vim.api.nvim_get_current_buf()
  cfg = load_model()
  if not cfg then
    return vim.notify("Which threads run this? needs this project's thread model (vim.g.thread_model in its .nvim.lua)")
  end
  local client = clangd(buf)
  if not client then return vim.notify("Which threads run this? needs clangd running in this file") end
  local row, col = unpack(vim.api.nvim_win_get_cursor(0))
  coroutine.wrap(function()
    local ok, err = pcall(function()
      local s = source(vim.api.nvim_buf_get_name(buf))
      local fn = s and function_at(s, row - 1, col)
      if not fn then return vim.notify("Put the cursor inside a function") end
      local p = scan_project(client.root_dir or vim.fn.getcwd())
      local globals = globals_of(s)
      local info = study(client, buf, fn, s, p, globals)
      if not info then return vim.notify("clangd doesn't know this function yet") end
      -- The file's other functions, for "does another thread use what this writes?"
      local all = { info }
      for _, f in ipairs(functions_of(s)) do
        if f:id() ~= fn:id() then
          all[#all + 1] = { name = names_of(f, s) or { name = "?" }, local_ = analyze(f, s, globals), fn = f }
        end
      end
      local c = cross(info, all, function(o)
        local i = study(client, buf, o.fn, s, p, globals)
        if i then o.reach = i.reach end
      end)
      local r = info.reach or { threads = {}, unknown = {} }
      local lines, hls = {}, {}
      local function add(l, hl) lines[#lines + 1] = l; if hl then hls[#lines] = hl end end
      local threads = list(r.threads)
      add(info.name.full, "Title")
      add("")
      if #threads == 0 then
        add("No thread found that reaches it" .. (#r.unknown > 0 and " (no callers: " .. table.concat(r.unknown, ", ") .. ")" or "") .. ".", "Comment")
      elseif #threads == 1 then
        add("Only the " .. threads[1] .. " runs this.", "DiagnosticOk")
        add("  " .. r.threads[threads[1]], "Comment")
      else
        add("Runs on " .. #threads .. " threads:", "DiagnosticWarn")
        for _, t in ipairs(threads) do
          add("  • " .. t)
          add("      " .. r.threads[t], "Comment")
        end
      end
      if #threads > 0 and #r.unknown > 0 then
        add("  Also called from code with no callers found: " .. table.concat(r.unknown, ", "), "Comment")
      end
      add("")
      local writes = info.local_.writes
      if #writes == 0 then
        add("Writes no shared data.", "Comment")
      else
        add("Writes:")
        local seen = {}
        for _, w in ipairs(writes) do
          local key = w.name .. ":" .. w.row
          if not seen[key] then
            seen[key] = true
            local how = w.lock and ("locked by " .. w.lock) or r.callers_locked and ("callers hold " .. r.callers_locked) or "no lock"
            add(("  %-16s line %-4d %s %s"):format(w.name, w.row + 1, w.kind, "(" .. how .. ")"),
              (w.lock or r.callers_locked) and "DiagnosticOk"
                or ((#threads >= 2 or (c and vim.tbl_contains(c.members, w.name))) and "DiagnosticWarn" or nil))
          end
        end
      end
      if c then
        add("")
        add(("%s also uses %s, on the %s:"):format(c.other.name.name, table.concat(c.members, ", "), c.thread), "DiagnosticWarn")
        add("      " .. c.other.reach.threads[c.thread], "Comment")
        add("  Neither side holds a lock: guard " .. (#c.members > 1 and "them" or "it") .. " in both.", "DiagnosticWarn")
        add("")
        add("How to fix it:", "Title")
        for _, l in ipairs(cross_fix(info, c, suggester(all), function(row) return s.lines[row + 1] end)) do
          add(l, l:match("^%s%s%s%s%s") and "String" or nil)
        end
      end
      if #info.local_.locks > 0 then
        add("")
        add("Locks taken here:")
        for _, l in ipairs(info.local_.locks) do
          add(("  %s (%s), lines %d–%d"):format(l.name, l.how, l.from + 1, l.to + 1))
        end
      end
      local width = 20
      for _, l in ipairs(lines) do width = math.max(width, vim.fn.strdisplaywidth(l) + 2) end
      local fb = vim.api.nvim_create_buf(false, true)
      vim.api.nvim_buf_set_lines(fb, 0, -1, false, lines)
      for i, hl in pairs(hls) do vim.api.nvim_buf_set_extmark(fb, ns, i - 1, 0, { end_row = i, hl_group = hl, hl_eol = false }) end
      vim.bo[fb].modifiable = false
      local w = vim.api.nvim_open_win(fb, true, {
        relative = "cursor", row = 1, col = 0, width = math.min(width, vim.o.columns - 4), height = #lines,
        border = "rounded", title = " Which threads run this? ", style = "minimal",
      })
      vim.keymap.set("n", "q", "<cmd>close<cr>", { buffer = fb, nowait = true })
      vim.keymap.set("n", "<Esc>", "<cmd>close<cr>", { buffer = fb, nowait = true })
      vim.api.nvim_create_autocmd("WinLeave", { buffer = fb, once = true, callback = function()
        if vim.api.nvim_win_is_valid(w) then vim.api.nvim_win_close(w, true) end end })
    end)
    if not ok then vim.notify("Which threads run this? " .. tostring(err), vim.log.levels.WARN) end
  end)()
end

-- Called from a project's .nvim.lua; running it again changes nothing.
function M.setup()
  local group = vim.api.nvim_create_augroup("rick_threads", { clear = true })
  local timers = {}
  local function later(buf, ms)
    if timers[buf] then timers[buf]:stop() end
    timers[buf] = timers[buf] or vim.uv.new_timer()
    timers[buf]:start(ms, 0, vim.schedule_wrap(function() M.check(buf) end))
  end
  vim.api.nvim_create_autocmd("BufWritePost", {
    group = group, pattern = { "*.c", "*.cpp", "*.cc", "*.cxx", "*.h", "*.hpp" },
    callback = function(ev)
      local path = vim.api.nvim_buf_get_name(ev.buf)
      coroutine.wrap(function() rescan_file(path) end)()
      later(ev.buf, 1500)
    end,
  })
  vim.api.nvim_create_autocmd("LspAttach", {
    group = group,
    callback = function(ev)
      local c = vim.lsp.get_client_by_id(ev.data.client_id)
      if c and c.name == "clangd" then later(ev.buf, 4000) end -- give the index a moment
    end,
  })
  vim.keymap.set("n", "<leader>cx", M.explain, { desc = "Which threads run this?" })
  vim.keymap.set("n", "<leader>cX", function() projects = {}; M.check() end, { desc = "Run the thread check now" })
  require("config.prefs").add_toggle({ "thread_check", "<leader>tT", "Toggle the thread (mutex) check in the margin",
    "Thread check", function() M.check() end })
  vim.cmd([[nnoremenu .503 PopUp.Which\ Threads\ Run\ This? <Cmd>lua require("config.threads").explain()<CR>]])
  -- The entry is only there for a right-click inside a C/C++ function (the
  -- click has already moved the cursor there); it's put back with its own
  -- priority so it keeps its place at the end.
  vim.api.nvim_create_autocmd("MenuPopup", {
    group = group,
    callback = function()
      local in_fn = false
      local ft = vim.bo.filetype
      if ft == "c" or ft == "cpp" then
        local ok, node = pcall(vim.treesitter.get_node)
        while ok and node do
          if node:type() == "function_definition" then in_fn = true; break end
          node = node:parent()
        end
      end
      pcall(vim.cmd, [[nunmenu PopUp.Which\ Threads\ Run\ This?]])
      if in_fn then
        vim.cmd([[nnoremenu .503 PopUp.Which\ Threads\ Run\ This? <Cmd>lua require("config.threads").explain()<CR>]])
      end
    end,
  })
  -- Already-open C/C++ files with clangd running (the .nvim.lua ran after them).
  for _, c in ipairs(vim.lsp.get_clients({ name = "clangd" })) do
    for buf in pairs(c.attached_buffers or {}) do later(buf, 4000) end
  end
end

return M
