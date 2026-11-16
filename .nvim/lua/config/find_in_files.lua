-- Find in Files / Replace in Files, like CLion's: one popup over the code.
--
--   Cmd+Shift+F or Space sf   Find in Files
--   Cmd+Shift+R or Space sr   Replace in Files (Ctrl+R in the popup switches)
--
-- The boxes start empty (a one-line selection fills Find, as in CLion). Matches
-- show as you type, grouped by file, with the code around the selected one in
-- the preview underneath. Nothing is highlighted in your files: the marks live
-- only in the popup, and nothing is left behind when it closes.
--   Up / Down (or the mouse)  pick a match       Enter (or a double-click)  open it
--   Tab / Shift+Tab           next / previous box     Esc  close
--   Alt+C Match Case   Alt+W Words   Alt+X Regex   (or click Aa W .*)
-- File mask: *.c, *.h  (a ! in front leaves files out; empty searches everything).
-- Replace mode shows each line as it will read after the replacement, and the
-- buttons (clickable too):
--   Alt+R  Replace       the selected line's matches
--   Alt+A  Replace All   writes every file (asks first)
--   Alt+P  Preview       a dry run: every change, per file, nothing written
--   Alt+U  Undo Replace  puts back every file the last replace changed, exactly
--                        as it was (also after a restart: :UndoReplaceInFiles)
-- With Regex on, $1, $2 or ${name} in Replace put in what the group matched.
-- A file open with unsaved changes is searched and changed in the editor, not
-- on disk; a file open without changes is changed in the editor and saved; the
-- rest are changed on disk. ripgrep (rg) does the searching.

local M = {}
local api, uv = vim.api, vim.uv
local ns = api.nvim_create_namespace("rick_find_in_files")
local ns_sel = api.nvim_create_namespace("rick_find_in_files_sel")
local LIMIT = 1000 -- matches shown while typing (Replace All and Preview take them all)
local DEBOUNCE = 120
local UNDO_DIR = vim.fn.stdpath("state") .. "/find_in_files_undo"
local LABEL_W = 12

local opts = { case = false, words = false, regex = false } -- kept between opens, like CLion
local saved_mask = ""
local S -- the open popup
local last -- the last replace, for Undo Replace (false: none on disk either)
local hits = {} -- clickable chips: { row, from, to, action }
local session = 0

---------------------------------------------------------------------------
-- Files and buffers

local function realpath(p) return uv.fs_realpath(p) or vim.fs.normalize(p) end

local function read_file(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local data = f:read("*a")
  f:close()
  return data
end

local function write_file(path, data)
  local f, err = io.open(path, "wb")
  if not f then return false, err end
  f:write(data)
  f:close()
  return true
end

-- realpath -> loaded buffer, for normal file buffers.
local function buf_map()
  local m = {}
  for _, b in ipairs(api.nvim_list_bufs()) do
    if api.nvim_buf_is_loaded(b) and vim.bo[b].buftype == "" then
      local n = api.nvim_buf_get_name(b)
      if n ~= "" then m[realpath(n)] = b end
    end
  end
  return m
end

local function buf_text(b) return table.concat(api.nvim_buf_get_lines(b, 0, -1, false), "\n") end

local function history(path, text)
  local ok, lh = pcall(require, "config.local_history")
  if ok and lh.record and text then pcall(lh.record, path, text, false) end
end

---------------------------------------------------------------------------
-- Searching with rg

local function mask_globs(mask)
  local out = {}
  for g in (mask or ""):gmatch("[^,;]+") do
    g = vim.trim(g)
    if g ~= "" then out[#out + 1] = g end
  end
  return out
end

-- The file mask as rg applies it, for open buffers searched through stdin.
local function mask_ok(globs, rel)
  local any_pos, pos_hit = false, false
  for _, g in ipairs(globs) do
    local neg = g:sub(1, 1) == "!"
    local pat = neg and g:sub(2) or g
    local ok, lp = pcall(vim.glob.to_lpeg, pat)
    local hit = ok and lp:match(pat:find("/") and rel or vim.fs.basename(rel)) ~= nil
    if neg then
      if hit then return false end
    else
      any_pos = true
      pos_hit = pos_hit or hit
    end
  end
  return pos_hit or not any_pos
end

local function rg_cmd(q, extra, path)
  local a = { "rg", "--no-config", "--sort=path", q.case and "--case-sensitive" or "--ignore-case" }
  if q.words then a[#a + 1] = "--word-regexp" end
  if not q.regex then a[#a + 1] = "--fixed-strings" end
  vim.list_extend(a, extra)
  vim.list_extend(a, { "-e", q.find, path })
  return a
end

-- What to search: the project from disk, plus each file open with unsaved
-- changes as it is in the editor (its copy on disk is skipped).
local function jobs(q)
  local globs, gargs = mask_globs(q.mask), {}
  for _, g in ipairs(globs) do vim.list_extend(gargs, { "--glob", g }) end
  local list, skip = { { gargs = gargs } }, {}
  for _, b in ipairs(api.nvim_list_bufs()) do
    if api.nvim_buf_is_loaded(b) and vim.bo[b].buftype == "" and vim.bo[b].modified then
      local name = api.nvim_buf_get_name(b)
      local p = name ~= "" and realpath(name)
      if p and vim.startswith(p, q.root .. "/") then
        local rel = p:sub(#q.root + 2)
        if mask_ok(globs, rel) then
          skip[rel] = true
          list[#list + 1] = { rel = rel, buf = b, stdin = buf_text(b) .. "\n" }
        end
      end
    end
  end
  for _, j in ipairs(list) do j.skip = skip end
  return list
end

local function txt(o)
  if not o then return nil end
  return o.text or (o.bytes and vim.base64.decode(o.bytes))
end

-- One rg --json line; adds a hit (one per matching line) and returns its match count.
local function parse_json(l, job, out)
  if l:sub(1, 15) ~= '{"type":"match"' then return end
  local ok, ev = pcall(vim.json.decode, l)
  if not ok or type(ev) ~= "table" or ev.type ~= "match" then return end
  local d = ev.data
  local rel = job.rel or (txt(d.path) or ""):gsub("^%./", "")
  if not job.rel and job.skip[rel] then return end
  local line = (txt(d.lines) or ""):gsub("\r?\n$", "")
  local subs = {}
  -- k: its place among the line's matches, empty ones included, which is how
  -- the --replace output lines up with them.
  for k, sm in ipairs(d.submatches or {}) do
    if sm["end"] > sm.start then subs[#subs + 1] = { s = sm.start, e = sm["end"], text = txt(sm.match), k = k } end
  end
  if #subs == 0 then return end
  out[#out + 1] = { rel = rel, lnum = d.line_number, line = line, subs = subs }
  return #subs
end

-- One rg --only-matching --replace line: "path\0line:col:replacement". The
-- column counts in the already-replaced line, so they're matched up by order.
local function parse_repl(l, job, map)
  local z = l:find("\0", 1, true)
  if not z then return end
  local lnum, r = l:match("^(%d+):%d+:(.*)$", z + 1)
  if not lnum then return end
  local key = (job.rel or l:sub(1, z - 1):gsub("^%./", "")) .. "\0" .. lnum
  local list = map[key] or {}
  map[key] = list
  list[#list + 1] = r
  return true
end

local function liner(on_line)
  local rest = ""
  return function(_, data)
    if not data then
      if rest ~= "" then on_line(rest) end
      rest = ""
      return
    end
    data = rest .. data
    local from = 1
    while true do
      local nl = data:find("\n", from, true)
      if not nl then break end
      on_line(data:sub(from, nl - 1))
      from = nl + 1
    end
    rest = data:sub(from)
  end
end

local function collect(q, res)
  local all = {}
  for _, part in ipairs(res.parts) do vim.list_extend(all, part) end
  table.sort(all, function(a, b)
    if a.rel ~= b.rel then return a.rel < b.rel end
    return a.lnum < b.lnum
  end)
  local files, by, total = {}, {}, 0
  for _, h in ipairs(all) do
    if q.repl then
      local list = res.repl[h.rel .. "\0" .. h.lnum] or {}
      for _, sm in ipairs(h.subs) do sm.repl = list[sm.k] end
    end
    local f = by[h.rel]
    if not f then
      f = { rel = h.rel, path = q.root .. "/" .. h.rel, hits = {}, n = 0 }
      by[h.rel] = f
      files[#files + 1] = f
    end
    h.file = f
    f.hits[#f.hits + 1] = h
    f.n = f.n + #h.subs
    total = total + #h.subs
  end
  local err
  local e = table.concat(res.errs)
  if e:find("regex parse error") or e:find("error parsing") or (#files == 0 and e:find("error")) then
    err = (q.regex and "Bad regex: " or "") .. (e:match("error: ([^\n]+)") or vim.trim(e):match("[^\n]+") or "error")
  elseif #files == 0 and res.spawn_err then
    err = res.spawn_err
  end
  return { files = files, total = total, truncated = res.truncated, err = err }
end

-- Start the searches; done(result) runs when they all finish (limit: stop
-- after that many matches). Returns the processes and the shared state.
local function search(q, limit, done)
  local res = { parts = {}, repl = {}, errs = {}, count = 0, truncated = false }
  local procs, pending = {}, 0
  local function finished()
    pending = pending - 1
    if pending == 0 and done then vim.schedule(function() done(collect(q, res)) end) end
  end
  local function spawn(cmd, stdin, on_line)
    pending = pending + 1
    local holder = {}
    local ok, p = pcall(vim.system, cmd, {
      cwd = q.root,
      stdin = stdin,
      stdout = liner(function(l) on_line(l, holder) end),
      stderr = function(_, d) if d then res.errs[#res.errs + 1] = d end end,
    }, finished)
    if ok then
      holder.p = p
      procs[#procs + 1] = p
    else
      res.spawn_err = "Can't run rg: " .. tostring(p)
      pending = pending - 1
    end
  end
  for i, job in ipairs(jobs(q)) do
    local part = {}
    res.parts[i] = part
    local path = job.rel and "-" or "."
    spawn(rg_cmd(q, vim.list_extend({ "--json" }, job.gargs or {}), path), job.stdin, function(l, h)
      local n = parse_json(l, job, part)
      if n and limit then
        res.count = res.count + n
        if res.count >= limit and h.p and not h.killed then
          res.truncated, h.killed = true, true
          pcall(h.p.kill, h.p, 15)
        end
      end
    end)
    if q.repl then
      local r = q.repl
      if not q.regex then r = r:gsub("%$", "$$") end
      local extra = { "--null", "--line-number", "--column", "--only-matching", "--with-filename", "--replace", r }
      local count = 0
      spawn(rg_cmd(q, vim.list_extend(extra, job.gargs or {}), path), job.stdin, function(l, h)
        if parse_repl(l, job, res.repl) and limit then
          count = count + 1
          if count >= limit and h.p and not h.killed then
            h.killed = true
            pcall(h.p.kill, h.p, 15)
          end
        end
      end)
    end
  end
  if pending == 0 and done then vim.schedule(function() done(collect(q, res)) end) end
  return procs, res
end

local function search_now(q)
  local procs, res = search(q, nil, nil)
  for _, p in ipairs(procs) do p:wait(120000) end
  return collect(q, res)
end

---------------------------------------------------------------------------
-- Replacing, and the snapshot Undo Replace puts back

local function snap_load()
  if last ~= nil then return last or nil end
  last = false
  local raw = read_file(UNDO_DIR .. "/index.json")
  local ok, idx = pcall(vim.json.decode, raw or "")
  if not ok or type(idx) ~= "table" or type(idx.files) ~= "table" then return nil end
  for i, e in ipairs(idx.files) do
    e.before, e.after = read_file(UNDO_DIR .. "/" .. i .. ".before"), read_file(UNDO_DIR .. "/" .. i .. ".after")
    if not e.before then return nil end
  end
  last = idx
  return last
end

local function snap_save(snap)
  last = snap or false
  vim.fn.delete(UNDO_DIR, "rf")
  if not snap then return end
  vim.fn.mkdir(UNDO_DIR, "p")
  local idx = { time = snap.time, find = snap.find, replace = snap.replace, root = snap.root, files = {} }
  for i, e in ipairs(snap.files) do
    write_file(UNDO_DIR .. "/" .. i .. ".before", e.before)
    write_file(UNDO_DIR .. "/" .. i .. ".after", e.after or "")
    idx.files[i] = { path = e.path, rel = e.rel, kind = e.kind, n = e.n }
  end
  local ok, json = pcall(vim.json.encode, idx)
  if ok then write_file(UNDO_DIR .. "/index.json", json) end
end

-- Replace the given hits (each checked against the text as it is now).
-- Returns one undo entry per changed file, the count, and how many were skipped.
local function apply(files)
  local entries, n, skipped, errors = {}, 0, 0, {}
  local bufs = buf_map()
  for _, f in ipairs(files) do
    local hs = vim.list_slice(f.hits)
    table.sort(hs, function(a, b) return a.lnum > b.lnum end)
    local b = bufs[f.path]
    local c = 0
    if b then
      local was_mod = vim.bo[b].modified
      local before = buf_text(b)
      local disk = not was_mod and read_file(f.path) or nil
      for _, h in ipairs(hs) do
        local line = api.nvim_buf_get_lines(b, h.lnum - 1, h.lnum, false)[1]
        for i = #h.subs, 1, -1 do
          local sm = h.subs[i]
          if line and sm.repl and line:sub(sm.s + 1, sm.e) == sm.text then
            api.nvim_buf_set_text(b, h.lnum - 1, sm.s, h.lnum - 1, sm.e, { sm.repl })
            c = c + 1
          else
            skipped = skipped + 1
          end
        end
      end
      if c > 0 then
        if was_mod or not disk then
          entries[#entries + 1] = { path = f.path, rel = f.rel, kind = "buffer", before = before, after = buf_text(b), n = c }
        else
          history(f.path, disk)
          vim.g.rick_autosaving = true -- not formatted, like an auto-save (plugins/format.lua)
          local ok, err = pcall(api.nvim_buf_call, b, function() vim.cmd("silent lockmarks update") end)
          vim.g.rick_autosaving = false
          if not ok then errors[#errors + 1] = f.rel .. ": " .. tostring(err) end
          entries[#entries + 1] = { path = f.path, rel = f.rel, kind = "file", before = disk, after = read_file(f.path), n = c }
        end
      end
    else
      local data = read_file(f.path)
      if data then
        local lines = vim.split(data, "\n", { plain = true })
        for _, h in ipairs(hs) do
          local line = lines[h.lnum]
          for i = #h.subs, 1, -1 do
            local sm = h.subs[i]
            if line and sm.repl and line:sub(sm.s + 1, sm.e) == sm.text then
              line = line:sub(1, sm.s) .. sm.repl .. line:sub(sm.e + 1)
              c = c + 1
            else
              skipped = skipped + 1
            end
          end
          lines[h.lnum] = line
        end
        if c > 0 then
          local new = table.concat(lines, "\n")
          history(f.path, data)
          local ok, err = write_file(f.path, new)
          if ok then
            history(f.path, new)
            entries[#entries + 1] = { path = f.path, rel = f.rel, kind = "file", before = data, after = new, n = c }
          else
            errors[#errors + 1] = f.rel .. ": " .. tostring(err)
            c = 0
          end
        end
      else
        errors[#errors + 1] = f.rel .. ": can't read"
      end
    end
    n = n + c
  end
  return entries, n, skipped, errors
end

local function plural(n, word) return n .. " " .. word .. (n == 1 and "" or (word:sub(-2) == "ch" and "es" or "s")) end

local function rel_name(e, root)
  if e.rel then return e.rel end
  return root and vim.startswith(e.path, root .. "/") and e.path:sub(#root + 2) or vim.fn.fnamemodify(e.path, ":~:.")
end

-- Put back every file the last replace changed. Returns the message.
function M.undo()
  local snap = snap_load()
  if not snap or #snap.files == 0 then
    local msg = "Undo Replace: nothing to undo"
    vim.notify(msg)
    return msg
  end
  local restored, changed, failed = {}, {}, {}
  local bufs = buf_map()
  for _, e in ipairs(snap.files) do
    local name = rel_name(e, snap.root)
    local b = bufs[e.path]
    if e.kind == "buffer" then
      if not b then -- closed since: open it again to put the text back, and save it
        b = vim.fn.bufadd(e.path)
        vim.fn.bufload(b)
      end
      if buf_text(b) ~= e.after then changed[#changed + 1] = name end
      api.nvim_buf_set_lines(b, 0, -1, false, vim.split(e.before, "\n", { plain = true }))
      if not bufs[e.path] then
        vim.g.rick_autosaving = true
        pcall(api.nvim_buf_call, b, function() vim.cmd("silent lockmarks update") end)
        vim.g.rick_autosaving = false
      end
      restored[#restored + 1] = name .. (bufs[e.path] and " (in the editor)" or "")
    else
      if read_file(e.path) ~= e.after then changed[#changed + 1] = name end
      local ok, err = write_file(e.path, e.before)
      if ok then
        history(e.path, e.before)
        -- Reload an open copy (its undo history keeps what it had).
        if b then pcall(api.nvim_buf_call, b, function() vim.cmd("silent! edit!") end) end
        restored[#restored + 1] = name
      else
        failed[#failed + 1] = name .. " (" .. tostring(err) .. ")"
      end
    end
  end
  snap_save(nil)
  local msg = ("Undo Replace: restored %s: %s"):format(plural(#restored, "file"), table.concat(restored, ", "))
  if #changed > 0 then msg = msg .. ". Changed since the replace (now back as before it): " .. table.concat(changed, ", ") end
  if #failed > 0 then msg = msg .. ". Could not restore: " .. table.concat(failed, ", ") end
  vim.notify(msg, #failed > 0 and vim.log.levels.WARN or vim.log.levels.INFO)
  return msg, ("Undo Replace: restored %s%s"):format(plural(#restored, "file"), #failed > 0 and (", %d failed"):format(#failed) or "")
end

---------------------------------------------------------------------------
-- The popup

local FIELDS = { find = { "find", "mask" }, replace = { "find", "replace", "mask" } }
local LABELS = { find = "Find:", replace = "Replace:", mask = "File mask:" }

local function is_open() return S ~= nil and api.nvim_win_is_valid(S.wins.input) end

function M.label()
  local f = S and S.fields[vim.v.lnum]
  if not f or vim.v.virtnum ~= 0 then return string.rep(" ", LABEL_W) end
  return "%#RickFifLabel# " .. LABELS[f] .. string.rep(" ", LABEL_W - 1 - #LABELS[f]) .. "%*"
end

local function read_fields()
  local lines = api.nvim_buf_get_lines(S.bufs.input, 0, -1, false)
  for i, f in ipairs(S.fields) do S.vals[f] = lines[i] or "" end
end

local function write_fields()
  local lines = {}
  for i, f in ipairs(S.fields) do lines[i] = S.vals[f] end
  api.nvim_buf_set_lines(S.bufs.input, 0, -1, false, lines)
end

local function query()
  return {
    find = S.vals.find, repl = S.mode == "replace" and S.vals.replace or nil, mask = S.vals.mask, root = S.root,
    case = opts.case, words = opts.words, regex = opts.regex,
  }
end

local function geometry()
  local cols, lines = vim.o.columns, vim.o.lines - vim.o.cmdheight
  local width = math.min(160, math.max(50, math.floor(cols * 0.86)))
  local height = math.max(16, math.floor(lines * 0.88))
  height = math.min(height, lines - 2)
  local col = math.floor((cols - width) / 2)
  local top = math.max(0, math.floor((lines - height) / 2) - 1)
  local nin = #S.fields
  local rest = height - nin - 4
  local list_h = math.max(3, math.floor(rest * 0.45))
  local prev_h = math.max(3, rest - list_h)
  local w = width - 2
  return {
    input = { row = top, col = col, width = w, height = nin },
    list = { row = top + nin + 2, col = col, width = w, height = list_h },
    preview = { row = top + nin + 2 + list_h + 1, col = col, width = w, height = prev_h },
  }
end

local BORDERS = {
  input = { "╭", "─", "╮", "│", "┤", "─", "├", "│" },
  list = { "", "", "", "│", "┤", "─", "├", "│" },
  preview = { "", "", "", "│", "╯", "─", "╰", "│" },
}

local function hints()
  if S.mode == "replace" then
    return " Alt+R Replace · Alt+A Replace All · Alt+P Preview · Alt+U Undo Replace · Ctrl+R Find · Esc Close "
  end
  return " ↑↓ Select · Enter Open · Tab Next box · Ctrl+R Replace · Esc Close "
end

local function status_chunks()
  local out = {}
  if S.msg then out[#out + 1] = { " " .. S.msg .. " ", S.msg_hl or "RickFifMsg" } end
  local r = S.res
  if S.vals.find == "" then
    out[#out + 1] = { " Type to search in " .. vim.fs.basename(S.root) .. " ", "Comment" }
  elseif S.searching and not r then
    out[#out + 1] = { " Searching… ", "Comment" }
  elseif r and r.err then
    out[#out + 1] = { " " .. r.err .. " ", "RickFindNone" }
  elseif r and r.total == 0 then
    out[#out + 1] = { " No matches ", "RickFindNone" }
  elseif r then
    local s = r.truncated and ("first %d matches shown"):format(r.total)
      or ("%s in %s"):format(plural(r.total, "match"), plural(#r.files, "file"))
    out[#out + 1] = { " " .. s .. (S.searching and " …" or "") .. " ", "RickFindTitle" }
  end
  return #out > 0 and out or "" -- a footer can't be an empty list
end

local function win_config(name, g)
  local c = {
    relative = "editor", row = g[name].row, col = g[name].col, width = g[name].width, height = g[name].height,
    border = BORDERS[name], zindex = 60,
  }
  if name == "input" then
    c.title = { { S.mode == "replace" and " Replace in Files " or " Find in Files ", "RickFindTitle" },
      { (S.in_dir and "In Folder: " or "In Project: ") .. vim.fs.basename(S.root) .. " ", "Comment" } }
    c.title_pos = "left"
    c.footer, c.footer_pos = status_chunks(), "right"
  elseif name == "list" then
    local h = S.flat and S.flat[S.sel]
    c.footer = h and { { " " .. h.file.rel .. ":" .. h.lnum .. " ", "RickFindTitle" } } or ""
    c.footer_pos = "left"
  else
    c.footer, c.footer_pos = { { hints(), "Comment" } }, "right"
  end
  return c
end

local function relayout()
  if not is_open() then return end
  local g = geometry()
  for _, name in ipairs({ "input", "list", "preview" }) do
    if api.nvim_win_is_valid(S.wins[name]) then api.nvim_win_set_config(S.wins[name], win_config(name, g)) end
  end
  if S.dry and api.nvim_win_is_valid(S.dry.win) then
    api.nvim_win_set_config(S.dry.win, {
      relative = "editor", row = g.list.row, col = g.list.col, width = g.list.width,
      height = g.list.height + g.preview.height + 1,
    })
  end
end

local function set_footers()
  if not is_open() then return end
  local g = geometry()
  pcall(api.nvim_win_set_config, S.wins.input, { footer = status_chunks(), footer_pos = "right" })
  pcall(api.nvim_win_set_config, S.wins.list, { footer = win_config("list", g).footer, footer_pos = "left" })
end

-- Toggles and buttons at the right of the boxes; remember where they are for clicks.
local function draw_input()
  if not is_open() then return end
  local b = S.bufs.input
  api.nvim_buf_clear_namespace(b, ns, 0, -1)
  hits = {}
  local width = api.nvim_win_get_width(S.wins.input) - LABEL_W
  local function place(row, parts)
    local vt, total = {}, 0
    for _, p in ipairs(parts) do total = total + vim.fn.strdisplaywidth(p[1]) end
    local col = width - total
    for _, p in ipairs(parts) do
      local w = vim.fn.strdisplaywidth(p[1])
      if p[3] then hits[#hits + 1] = { row = row, from = col, to = col + w, action = p[3] } end
      col = col + w
      vt[#vt + 1] = { p[1], p[2] }
    end
    api.nvim_buf_set_extmark(b, ns, row, 0, { virt_text = vt, virt_text_pos = "right_align" })
  end
  local function chip(parts, label, on, action)
    parts[#parts + 1] = { " " .. label .. " ", on and "RickFindOn" or "RickFindOff", action }
    parts[#parts + 1] = { " ", "RickFind" }
  end
  for row, f in ipairs(S.fields) do
    local parts = {}
    if f == "find" then
      chip(parts, "Aa", opts.case, "case")
      chip(parts, "W", opts.words, "words")
      chip(parts, ".*", opts.regex, "regex")
      parts[#parts + 1] = { "  ", "RickFind" }
      chip(parts, "⇄ Replace", S.mode == "replace", "mode")
    elseif f == "replace" then
      chip(parts, "Replace", true, "replace")
      chip(parts, "Replace All", true, "replace_all")
      chip(parts, "Preview", true, "preview")
      local can = snap_load() ~= nil
      parts[#parts + 1] = { " Undo Replace ", can and "RickFindOff" or "RickFifDim", "undo" }
      parts[#parts + 1] = { " ", "RickFind" }
    elseif f == "mask" and S.vals.mask == "" then
      api.nvim_buf_set_extmark(b, ns, row - 1, 0,
        { virt_text = { { "*.c, *.h   (empty: all files)", "RickFifDim" } }, virt_text_pos = "overlay" })
    end
    if #parts > 0 then place(row - 1, parts) end
  end
end

local function file_icon(name)
  local ok, dev = pcall(require, "nvim-web-devicons")
  if ok then
    local icon, hl = dev.get_icon(name, vim.fn.fnamemodify(name, ":e"), { default = true })
    if icon then return icon, hl end
  end
  return "", "Comment"
end

-- The line as shown in the list: tabs as spaces, indent dropped, and the
-- start cut off when the match would be off the right edge.
local function shown(h, avail)
  local line = h.line:gsub("\t", " ")
  local start = #line:match("^%s*")
  local first = h.subs[1].s
  local lead = ""
  if first - start > avail - 24 and avail > 40 then
    start = first - 16
    while start < #line and line:byte(start + 1) >= 0x80 and line:byte(start + 1) < 0xC0 do start = start + 1 end
    lead = "…"
  end
  return lead .. line:sub(start + 1), #lead - start
end

local function preview_buf(f)
  local b = buf_map()[f.path]
  local tick = b and api.nvim_buf_get_changedtick(b) or 0
  local c = S.pbufs[f.rel]
  if c and c.tick == tick and api.nvim_buf_is_valid(c.buf) then return c.buf end
  local lines
  if b then
    lines = api.nvim_buf_get_lines(b, 0, -1, false)
  else
    local data = read_file(f.path) or ""
    if #data > 4 * 1024 * 1024 then data = data:sub(1, 4 * 1024 * 1024) end
    lines = vim.split(data, "\n", { plain = true })
    if lines[#lines] == "" then lines[#lines] = nil end
    for i, l in ipairs(lines) do lines[i] = l:gsub("\r$", "") end
  end
  local pb = c and api.nvim_buf_is_valid(c.buf) and c.buf or api.nvim_create_buf(false, true)
  vim.bo[pb].modifiable = true
  api.nvim_buf_set_lines(pb, 0, -1, false, lines)
  vim.bo[pb].modifiable = false
  if not c then
    local ft = vim.filetype.match({ filename = f.path, buf = pb })
    if ft then
      local lang = vim.treesitter.language.get_lang(ft)
      if not (lang and pcall(vim.treesitter.start, pb, lang)) then vim.bo[pb].syntax = ft end
    end
  end
  S.pbufs[f.rel] = { buf = pb, tick = tick }
  return pb
end

local function show_preview()
  local w = S.wins.preview
  local h = S.flat[S.sel]
  if not h then
    api.nvim_win_set_buf(w, S.bufs.empty)
    return
  end
  local pb = preview_buf(h.file)
  if api.nvim_win_get_buf(w) ~= pb then
    api.nvim_win_set_buf(w, pb)
    -- A buffer new to the window starts from the window's global options.
    vim.wo[w].number, vim.wo[w].cursorline, vim.wo[w].wrap = true, true, false
    vim.wo[w].signcolumn, vim.wo[w].foldcolumn, vim.wo[w].scrolloff = "no", "0", 0
    vim.wo[w].winhighlight = vim.wo[S.wins.list].winhighlight
  end
  api.nvim_buf_clear_namespace(pb, ns, 0, -1)
  if h.lnum > api.nvim_buf_line_count(pb) then return end
  local len = #(api.nvim_buf_get_lines(pb, h.lnum - 1, h.lnum, false)[1] or "")
  for _, sm in ipairs(h.subs) do
    if sm.e <= len then
      local replacing = S.mode == "replace" and sm.repl
      api.nvim_buf_set_extmark(pb, ns, h.lnum - 1, sm.s,
        { end_col = sm.e, hl_group = replacing and "RickFifOld" or "RickFifMatch", priority = 200 })
      if replacing then
        api.nvim_buf_set_extmark(pb, ns, h.lnum - 1, sm.e,
          { virt_text = { { sm.repl, "RickFifNew" } }, virt_text_pos = "inline", priority = 200 })
      end
    end
  end
  pcall(api.nvim_win_set_cursor, w, { h.lnum, h.subs[1].s })
  api.nvim_win_call(w, function() vim.cmd("normal! zz") end)
end

local function select(i)
  if not is_open() then return end
  local n = #S.flat
  local lb, lw = S.bufs.list, S.wins.list
  api.nvim_buf_clear_namespace(lb, ns_sel, 0, -1)
  if n == 0 then
    S.sel = 1
    show_preview()
    set_footers()
    return
  end
  S.sel = math.max(1, math.min(n, i))
  local h = S.flat[S.sel]
  -- Under the match marks (a line highlight would cover them).
  api.nvim_buf_set_extmark(lb, ns_sel, h.row - 1, 0, { end_row = h.row, end_col = 0, hl_group = "RickFifSel", hl_eol = true, priority = 1 })
  -- Show the file name above the first match of a file.
  if h.file.hits[1] == h then api.nvim_win_set_cursor(lw, { h.row - 1, 0 }) end
  api.nvim_win_set_cursor(lw, { h.row, 0 })
  show_preview()
  set_footers()
end

local function render_list()
  if not is_open() then return end
  local lb = S.bufs.list
  local W = api.nvim_win_get_width(S.wins.list)
  local lines, marks, rows = {}, {}, {}
  S.flat = {}
  local r = S.res
  if S.vals.find == "" then
    lines[1] = "  Type text to find in " .. vim.fs.basename(S.root)
      .. (S.mode == "replace" and "   (Alt+U: Undo Replace)" or "")
    marks[#marks + 1] = { 0, 0, #lines[1], "Comment" }
  elseif r and r.err then
    lines[1] = "  " .. r.err
    marks[#marks + 1] = { 0, 0, #lines[1], "RickFindNone" }
  elseif r and r.total == 0 then
    lines[1] = "  No matches"
    marks[#marks + 1] = { 0, 0, #lines[1], "Comment" }
  end
  local inline = {}
  for _, f in ipairs(r and r.files or {}) do
    local name, dir = vim.fs.basename(f.rel), vim.fs.dirname(f.rel)
    local icon, icon_hl = file_icon(name)
    local head = " " .. icon .. " "
    local l = head .. name .. "  " .. (dir ~= "." and dir .. "/" or "") .. "  " .. plural(f.n, "match")
    lines[#lines + 1] = l
    local row = #lines - 1
    rows[#lines] = { file = f }
    marks[#marks + 1] = { row, 1, #head, icon_hl }
    marks[#marks + 1] = { row, #head, #head + #name, "RickFifFile" }
    marks[#marks + 1] = { row, #head + #name, #l, "Comment" }
    for _, h in ipairs(f.hits) do
      S.flat[#S.flat + 1] = h
      h.idx = #S.flat
      local prefix = ("   %5d   "):format(h.lnum)
      local text, off = shown(h, W - #prefix)
      lines[#lines + 1] = prefix .. text
      h.row = #lines
      rows[#lines] = { hit = h }
      marks[#marks + 1] = { h.row - 1, 0, #prefix, "LineNr" }
      for _, sm in ipairs(h.subs) do
        local s, e = #prefix + sm.s + off, #prefix + sm.e + off
        if s >= #prefix and e <= #lines[#lines] then
          if S.mode == "replace" and sm.repl then
            marks[#marks + 1] = { h.row - 1, s, e, "RickFifOld" }
            inline[#inline + 1] = { h.row - 1, e, sm.repl }
          else
            marks[#marks + 1] = { h.row - 1, s, e, "RickFifMatch" }
          end
        end
      end
    end
  end
  vim.bo[lb].modifiable = true
  api.nvim_buf_set_lines(lb, 0, -1, false, lines)
  vim.bo[lb].modifiable = false
  api.nvim_buf_clear_namespace(lb, ns, 0, -1)
  for _, m in ipairs(marks) do
    pcall(api.nvim_buf_set_extmark, lb, ns, m[1], m[2], { end_col = m[3], hl_group = m[4] })
  end
  for _, m in ipairs(inline) do
    pcall(api.nvim_buf_set_extmark, lb, ns, m[1], m[2], { virt_text = { { m[3], "RickFifNew" } }, virt_text_pos = "inline" })
  end
  S.rows = rows
  select(S.sel or 1)
end

local function refresh() -- after results arrive or the mode changes
  render_list()
  draw_input()
  set_footers()
end

local function signature(q)
  return table.concat({ q.find, q.repl or "\1", q.mask, tostring(q.case), tostring(q.words), tostring(q.regex) }, "\0")
end

local function kill_running()
  for _, p in ipairs(S.procs or {}) do pcall(p.kill, p, 15) end
  S.procs = {}
end

local function run_search(force)
  if not is_open() then return end
  local q = query()
  local sig = signature(q)
  if sig == S.sig and not force then return end
  S.sig = sig
  S.gen = S.gen + 1
  local gen = S.gen
  kill_running()
  if q.find == "" then
    S.res, S.searching, S.sel = nil, false, 1
    return refresh()
  end
  S.searching = true
  set_footers()
  S.procs = search(q, LIMIT, function(res)
    if not is_open() or S.gen ~= gen then return end
    S.searching = false
    if not force then S.sel = 1 end
    S.res = res
    refresh()
  end)
end

local function schedule_search(delay, force)
  S.timer:stop()
  S.timer:start(delay or DEBOUNCE, 0, vim.schedule_wrap(function() run_search(force) end))
end

local function message(msg, hl)
  if not S then return end
  S.msg, S.msg_hl = msg, hl
  set_footers()
end

function M.toggle(name)
  opts[name] = not opts[name]
  draw_input()
  schedule_search(0)
end

local function set_mode(mode)
  if not is_open() or S.mode == mode then return end
  read_fields()
  local row = S.fields[api.nvim_win_get_cursor(S.wins.input)[1]]
  S.mode, S.fields = mode, FIELDS[mode]
  write_fields()
  relayout()
  local want = mode == "replace" and S.vals.find ~= "" and "replace" or row
  for i, f in ipairs(S.fields) do
    if f == want then api.nvim_win_set_cursor(S.wins.input, { i, #S.vals[f] }) end
  end
  S.res = nil
  S.searching = S.vals.find ~= ""
  refresh()
  schedule_search(0)
end

local function move(d) if is_open() then select(S.sel + d) end end

local function next_box(d)
  local row = api.nvim_win_get_cursor(S.wins.input)[1]
  row = (row - 1 + d) % #S.fields + 1
  api.nvim_win_set_cursor(S.wins.input, { row, #S.vals[S.fields[row]] })
  draw_input()
end

-- Open the selected match in the code and close the popup.
local function open_selected()
  local h = S and S.flat[S.sel]
  if not h then return end
  local target, path, lnum, col = S.target, h.file.path, h.lnum, h.subs[1].s
  local was_insert = vim.fn.mode():find("^[iR]") ~= nil
  M.close()
  local done = false
  local function go()
    if done then return end
    done = true
    local function normal(w)
      return api.nvim_win_is_valid(w) and api.nvim_win_get_config(w).relative == ""
        and vim.bo[api.nvim_win_get_buf(w)].buftype == ""
    end
    if not (target and normal(target)) then
      target = nil
      for _, w in ipairs(api.nvim_tabpage_list_wins(0)) do
        if normal(w) then target = w break end
      end
    end
    if target then api.nvim_set_current_win(target) end
    vim.cmd("normal! m'")
    local b = buf_map()[path]
    if b then api.nvim_win_set_buf(0, b) else vim.cmd.edit(vim.fn.fnameescape(path)) end
    pcall(api.nvim_win_set_cursor, 0, { lnum, col })
    vim.cmd("normal! zvzz")
  end
  -- Only once Insert mode has ended, or leaving it steps the cursor left.
  if was_insert then
    api.nvim_create_autocmd("ModeChanged", { once = true, callback = function() vim.schedule(go) end })
    vim.defer_fn(go, 300)
  else
    vim.schedule(go)
  end
end

local function replace_selected()
  if S.mode ~= "replace" then return end
  local h = S.flat[S.sel]
  if not h then return end
  local entries, n, skipped, errors = apply({ { path = h.file.path, rel = h.file.rel, hits = { h } } })
  if n == 0 then
    message(#errors > 0 and errors[1] or "The file changed; results updated", "RickFindNone")
    return schedule_search(0, true)
  end
  -- Replacing one line at a time in this popup adds up into one Undo Replace.
  local snap = snap_load()
  if not (snap and snap.session == S.session) then
    snap = { time = os.time(), find = S.vals.find, replace = S.vals.replace, root = S.root, files = {}, session = S.session }
  end
  for _, e in ipairs(entries) do
    local found
    for _, old in ipairs(snap.files) do
      if old.path == e.path then
        old.after, old.n, found = e.after, old.n + e.n, true
        if old.kind == "file" and e.kind == "buffer" then old.kind = "buffer" end
      end
    end
    if not found then snap.files[#snap.files + 1] = e end
  end
  snap_save(snap)
  message(("Replaced %s in %s:%d"):format(plural(n, "match"), h.file.rel, h.lnum)
    .. (skipped > 0 and (" (%d changed since, skipped)"):format(skipped) or ""))
  draw_input()
  schedule_search(0, true)
end

local function replace_all()
  if S.mode ~= "replace" then return end
  read_fields()
  local q = query()
  if q.find == "" then return end
  local res = search_now(q)
  if res.err then return message(res.err, "RickFindNone") end
  if res.total == 0 then return message("Nothing to replace", "RickFindNone") end
  local files = #res.files
  local ok, choice = pcall(vim.fn.confirm,
    ("Replace %s in %s?"):format(plural(res.total, "occurrence"), plural(files, "file")), "&Replace All\n&Cancel", 1)
  vim.cmd("redraw")
  if not ok or choice ~= 1 then return message("Replace All cancelled") end
  local entries, n, skipped, errors = apply(res.files)
  if #entries > 0 then
    snap_save({ time = os.time(), find = q.find, replace = q.repl, root = S.root, files = entries })
  end
  local unsaved = 0
  for _, e in ipairs(entries) do if e.kind == "buffer" then unsaved = unsaved + 1 end end
  local msg = ("Replaced %s in %s"):format(plural(n, "occurrence"), plural(#entries, "file"))
  local long = msg
  if unsaved > 0 then long = long .. (" (%d open with unsaved changes: changed in the editor only)"):format(unsaved) end
  if skipped > 0 then long, msg = long .. (", %d skipped"):format(skipped), msg .. (", %d skipped"):format(skipped) end
  if #errors > 0 then long = long .. ". Errors: " .. table.concat(errors, "; ") end
  vim.notify(long .. ". Undo Replace (Alt+U) puts them back.", #errors > 0 and vim.log.levels.WARN or vim.log.levels.INFO)
  message(#errors > 0 and ("%s, %d failed"):format(msg, #errors) or (msg .. " · Alt+U undoes"), #errors > 0 and "RickFindNone" or nil)
  draw_input()
  schedule_search(0, true)
end

local function undo()
  local msg, short = M.undo()
  if is_open() then
    message(short or msg)
    draw_input()
    schedule_search(0, true)
  end
end

local function close_dry()
  if S and S.dry then
    pcall(api.nvim_win_close, S.dry.win, true)
    S.dry = nil
    if is_open() then
      api.nvim_set_current_win(S.wins.input)
      vim.cmd("startinsert!")
    end
  end
end

-- Preview (dry run): every change the Replace All would make, nothing written.
local function preview()
  if S.mode ~= "replace" then return end
  read_fields()
  local q = query()
  if q.find == "" then return end
  local res = search_now(q)
  if res.err then return message(res.err, "RickFindNone") end
  local lines, marks, rows = {}, {}, {}
  local function add(l, hl, row_info)
    lines[#lines + 1] = l
    if hl then marks[#marks + 1] = { #lines - 1, 0, #l, hl, true } end
    rows[#lines] = row_info
  end
  local bufs = buf_map()
  add((" Preview of Replace All: %s in %s. Nothing has been written."):format(
    plural(res.total, "replacement"), plural(#res.files, "file")), "RickFindTitle")
  add(" ")
  for _, f in ipairs(res.files) do
    local b = bufs[f.path]
    local where = b and (vim.bo[b].modified and "open with unsaved changes: only the editor copy changes"
      or "open: changed in the editor and saved") or "changed on disk"
    local head = (" %s  ·  %s  ·  %s"):format(f.rel, plural(f.n, "replacement"), where)
    add(head, nil, { file = f })
    marks[#marks + 1] = { #lines - 1, 1, 1 + #f.rel, "RickFifFile" }
    marks[#marks + 1] = { #lines - 1, 1 + #f.rel, #head, "Comment" }
    for _, h in ipairs(f.hits) do
      local num = ("   %5d "):format(h.lnum)
      local pad = string.rep(" ", #num)
      local new, old_spans, new_spans, shift = h.line, {}, {}, 0
      for _, sm in ipairs(h.subs) do
        local r = sm.repl or sm.text
        old_spans[#old_spans + 1] = { sm.s, sm.e }
        new = new:sub(1, sm.s + shift) .. r .. new:sub(sm.e + shift + 1)
        new_spans[#new_spans + 1] = { sm.s + shift, sm.s + shift + #r }
        shift = shift + #r - (sm.e - sm.s)
      end
      local old_l, new_l = num .. "- " .. h.line:gsub("\t", " "), pad .. "+ " .. new:gsub("\t", " ")
      add(old_l, "RickFifDelLine", { hit = h })
      marks[#marks + 1] = { #lines - 1, 0, #num, "LineNr" }
      for _, sp in ipairs(old_spans) do marks[#marks + 1] = { #lines - 1, #num + 2 + sp[1], #num + 2 + sp[2], "RickFifOld" } end
      add(new_l, "RickFifAddLine", { hit = h })
      for _, sp in ipairs(new_spans) do marks[#marks + 1] = { #lines - 1, #pad + 2 + sp[1], #pad + 2 + sp[2], "RickFifNew" } end
    end
  end
  if res.total == 0 then add("  No matches: nothing would change.", "Comment") end
  local b = api.nvim_create_buf(false, true)
  vim.bo[b].bufhidden = "wipe"
  api.nvim_buf_set_lines(b, 0, -1, false, lines)
  vim.bo[b].modifiable = false
  for _, m in ipairs(marks) do
    if m[5] then
      pcall(api.nvim_buf_set_extmark, b, ns, m[1], 0, { line_hl_group = m[4], priority = 50 })
    else
      pcall(api.nvim_buf_set_extmark, b, ns, m[1], m[2], { end_col = m[3], hl_group = m[4], priority = 100 })
    end
  end
  local g = geometry()
  vim.cmd("stopinsert")
  local win = api.nvim_open_win(b, true, {
    relative = "editor", row = g.list.row, col = g.list.col, width = g.list.width,
    height = g.list.height + g.preview.height + 1, border = BORDERS.preview, zindex = 65, style = "minimal",
    footer = { { " Enter Go to the line · Alt+A Replace All · q / Esc Back ", "Comment" } }, footer_pos = "right",
  })
  vim.wo[win].winhighlight = "NormalFloat:RickFind,FloatBorder:RickFindBorder,Search:None,CurSearch:None"
  vim.wo[win].wrap = false
  vim.wo[win].cursorline = true
  S.dry = { win = win, buf = b }
  local function k(lhs, fn) vim.keymap.set("n", lhs, fn, { buffer = b, nowait = true }) end
  k("q", close_dry)
  k("<Esc>", close_dry)
  k("<M-a>", function() close_dry() replace_all() end)
  k("<M-u>", function() close_dry() undo() end)
  k("<CR>", function()
    local info = rows[api.nvim_win_get_cursor(0)[1]]
    local h = info and (info.hit or (info.file and info.file.hits[1]))
    close_dry()
    if not h then return end
    for _, x in ipairs(S.flat) do
      if x.rel == h.rel and x.lnum == h.lnum then return select(x.idx) end
    end
  end)
  message(("Preview: %s in %s, nothing written"):format(plural(res.total, "replacement"), plural(#res.files, "file")))
end

local ACTIONS = {
  case = function() M.toggle("case") end,
  words = function() M.toggle("words") end,
  regex = function() M.toggle("regex") end,
  mode = function() set_mode(S.mode == "replace" and "find" or "replace") end,
  replace = function() replace_selected() end,
  replace_all = function() replace_all() end,
  preview = function() preview() end,
  undo = function() undo() end,
}

function M.close()
  if not S then return end
  local s = S
  S = nil
  pcall(function() s.timer:stop() s.timer:close() end)
  for _, p in ipairs(s.procs or {}) do pcall(p.kill, p, 15) end
  if s.dry then pcall(api.nvim_win_close, s.dry.win, true) end
  for _, w in pairs(s.wins) do pcall(api.nvim_win_close, w, true) end
  for _, c in pairs(s.pbufs) do pcall(api.nvim_buf_delete, c.buf, { force = true }) end
  for _, b in pairs(s.bufs) do pcall(api.nvim_buf_delete, b, { force = true }) end
  saved_mask = s.vals.mask or saved_mask
  if vim.fn.mode():find("^[iR]") then vim.cmd("stopinsert") end
  if s.target and api.nvim_win_is_valid(s.target) and api.nvim_get_current_win() ~= s.target then
    pcall(api.nvim_set_current_win, s.target)
  end
end

local function scratch(ft)
  local b = api.nvim_create_buf(false, true)
  vim.bo[b].bufhidden = "wipe"
  if ft then vim.bo[b].filetype = ft end
  return b
end

local open_popup

-- mode: "find" or "replace".
-- dir: search only this folder (the project tree's Find in Folder…).
function M.open(mode, dir)
  local ok, err = pcall(open_popup, mode, dir)
  if not ok then
    M.close()
    vim.notify("Find in Files: " .. tostring(err), vim.log.levels.ERROR)
  end
end

open_popup = function(mode, dir)
  mode = mode == "replace" and "replace" or "find"
  local root = realpath(dir or vim.fn.getcwd())
  if is_open() and root ~= S.root then M.close() end
  if is_open() then
    set_mode(mode)
    if not S.dry then api.nvim_set_current_win(S.wins.input) end
    vim.cmd("startinsert!")
    return
  end
  -- A one-line selection fills Find, as in CLion.
  local prefill = ""
  local vm = vim.fn.mode()
  if vm == "v" or vm == "V" or vm == "\22" then
    local ok, region = pcall(vim.fn.getregion, vim.fn.getpos("v"), vim.fn.getpos("."), { type = vm })
    if ok and #region == 1 then prefill = region[1] end
    api.nvim_feedkeys(vim.keycode("<Esc>"), "nx", false)
    if opts.regex then prefill = prefill:gsub("[\\%^%$%.%|%?%*%+%(%)%[%]%{%}]", "\\%0") end
  end
  pcall(function() require("config.quick_docs").close() end)
  pcall(function() require("config.diagnostics").close_popup() end)
  session = session + 1
  S = {
    mode = mode, fields = FIELDS[mode], vals = { find = prefill, replace = "", mask = saved_mask },
    target = api.nvim_get_current_win(), root = root, in_dir = dir ~= nil, wins = {}, bufs = {}, pbufs = {},
    flat = {}, rows = {}, sel = 1, gen = 0, procs = {}, timer = assert(uv.new_timer()), session = session,
    searching = prefill ~= "",
  }
  S.bufs.input = scratch("rick_find_in_files")
  S.bufs.list = scratch()
  S.bufs.empty = scratch()
  vim.bo[S.bufs.empty].bufhidden = "hide" -- the preview swaps it in and out
  vim.b[S.bufs.input].completion = false -- no completion menu over the boxes
  write_fields()
  local g = geometry()
  local hl = "NormalFloat:RickFind,FloatBorder:RickFindBorder,FloatTitle:RickFindTitle,FloatFooter:RickFindBorder,"
    .. "Search:None,CurSearch:None,IncSearch:None"
  for _, name in ipairs({ "list", "preview", "input" }) do
    local buf = name == "input" and S.bufs.input or (name == "list" and S.bufs.list or S.bufs.empty)
    local c = win_config(name, g)
    c.style = "minimal"
    local w = api.nvim_open_win(buf, name == "input", c)
    S.wins[name] = w
    vim.wo[w].winhighlight = hl
    vim.wo[w].wrap = false
  end
  vim.wo[S.wins.input].statuscolumn = "%{%v:lua.require'config.find_in_files'.label()%}"
  vim.wo[S.wins.preview].number = true
  vim.wo[S.wins.preview].cursorline = true
  vim.wo[S.wins.preview].scrolloff = 0
  vim.wo[S.wins.list].scrolloff = 2

  local b = S.bufs.input
  local function k(modes, lhs, fn) vim.keymap.set(modes, lhs, fn, { buffer = b, nowait = true }) end
  local ni = { "n", "i" }
  k(ni, "<Esc>", M.close)
  k("n", "q", M.close)
  k(ni, "<CR>", open_selected)
  for _, lhs in ipairs({ "<Up>", "<C-p>" }) do k(ni, lhs, function() move(-1) end) end
  for _, lhs in ipairs({ "<Down>", "<C-n>" }) do k(ni, lhs, function() move(1) end) end
  k(ni, "<PageUp>", function() move(-api.nvim_win_get_height(S.wins.list)) end)
  k(ni, "<PageDown>", function() move(api.nvim_win_get_height(S.wins.list)) end)
  k(ni, "<Tab>", function() next_box(1) end)
  k(ni, "<S-Tab>", function() next_box(-1) end)
  k(ni, "<M-c>", function() M.toggle("case") end)
  k(ni, "<M-w>", function() M.toggle("words") end)
  k(ni, "<M-x>", function() M.toggle("regex") end)
  for _, lhs in ipairs({ "<C-r>", "<D-r>" }) do k(ni, lhs, ACTIONS.mode) end
  k(ni, "<D-F>", function() set_mode("find") end)
  k(ni, "<D-R>", function() set_mode("replace") end)
  k(ni, "<M-r>", replace_selected)
  k(ni, "<M-a>", replace_all)
  k(ni, "<M-p>", preview)
  k(ni, "<M-u>", undo)
  api.nvim_create_autocmd({ "TextChangedI", "TextChanged" }, {
    buffer = b,
    callback = function()
      if not is_open() then return end
      local lines = api.nvim_buf_get_lines(b, 0, -1, false)
      local n = #S.fields
      if #lines ~= n then -- keep one line per box (a pasted newline joins up)
        local row = api.nvim_win_get_cursor(S.wins.input)[1]
        local extra = #lines - n
        local fixed = {}
        for i = 1, #lines do
          if extra > 0 and i > row and i <= row + extra then
            fixed[#fixed] = fixed[#fixed] .. lines[i]
          else
            fixed[#fixed + 1] = lines[i]
          end
        end
        while #fixed < n do fixed[#fixed + 1] = "" end
        api.nvim_buf_set_lines(b, 0, -1, false, vim.list_slice(fixed, 1, n))
      end
      read_fields()
      S.msg = nil
      draw_input()
      if S.vals.find == "" then run_search() else schedule_search() end
    end,
  })
  vim.cmd("startinsert!")
  refresh()
  if prefill ~= "" then
    if mode == "replace" then api.nvim_win_set_cursor(S.wins.input, { 2, 0 }) end
    run_search()
  end
end

function M.setup()
  local function hl()
    local function copy(from, extra)
      local h = api.nvim_get_hl(0, { name = from, link = false })
      return vim.tbl_extend("force", h, extra or {})
    end
    -- Copies, not links: the popup turns Search off so an old / search can't show in it.
    api.nvim_set_hl(0, "RickFifMatch", copy("Search", { default = true }))
    api.nvim_set_hl(0, "RickFifOld", copy("DiffDelete", { strikethrough = true, default = true }))
    api.nvim_set_hl(0, "RickFifNew", copy("DiffAdd", { bold = true, default = true }))
    api.nvim_set_hl(0, "RickFifDelLine", { link = "DiffDelete", default = true })
    api.nvim_set_hl(0, "RickFifAddLine", { link = "DiffAdd", default = true })
    api.nvim_set_hl(0, "RickFifSel", { link = "Visual", default = true })
    api.nvim_set_hl(0, "RickFifFile", { link = "Directory", default = true })
    api.nvim_set_hl(0, "RickFifLabel", { link = "Comment", default = true })
    api.nvim_set_hl(0, "RickFifDim", { link = "Comment", default = true })
    api.nvim_set_hl(0, "RickFifMsg", { link = "DiagnosticOk", default = true })
    -- The find bar's look (findbar.lua), in case it isn't loaded.
    api.nvim_set_hl(0, "RickFind", { link = "NormalFloat", default = true })
    api.nvim_set_hl(0, "RickFindBorder", { link = "FloatBorder", default = true })
    api.nvim_set_hl(0, "RickFindTitle", { link = "FloatTitle", default = true })
    api.nvim_set_hl(0, "RickFindOn", { link = "PmenuSel", default = true })
    api.nvim_set_hl(0, "RickFindOff", { link = "Pmenu", default = true })
    api.nvim_set_hl(0, "RickFindNone", { link = "DiagnosticError", default = true })
  end
  hl()
  local group = api.nvim_create_augroup("rick_find_in_files", { clear = true })
  api.nvim_create_autocmd("ColorScheme", { group = group, callback = hl })
  api.nvim_create_autocmd("VimResized", { group = group, callback = function() if is_open() then relayout() refresh() end end })
  -- Going to another window (a click on the code, the project tree) closes the popup.
  api.nvim_create_autocmd("WinEnter", {
    group = group,
    callback = function()
      vim.schedule(function()
        if not is_open() then return end
        local cur = api.nvim_get_current_win()
        for _, w in pairs(S.wins) do if w == cur then return end end
        if S.dry and S.dry.win == cur then return end
        M.close()
      end)
    end,
  })

  -- Clicks: the toggles and buttons, picking a match (double-click opens it).
  local swallow, last_click = false, { idx = 0, t = 0 }
  vim.on_key(function(key)
    if not S or not key:find("\128", 1, true) then return end -- mouse keys are special keys
    local kt = vim.fn.keytrans(key) -- <LeftMouse>, <2-LeftMouse>, <2-LeftRelease>, ...
    if swallow and (kt == "<LeftRelease>" or kt:match("LeftDrag>$")) then
      if kt == "<LeftRelease>" then swallow = false end
      return ""
    end
    -- The second click of a quick double-click can reach on_key as just
    -- <2-LeftRelease> (no <2-LeftMouse>), so any multi-click counts.
    local dbl = kt:match("^<%d%-Left[MR]") ~= nil
    if swallow and dbl and kt:match("Release>$") then
      swallow = false
      return ""
    end
    if kt ~= "<LeftMouse>" and not dbl then return end
    local m = vim.fn.getmousepos()
    if m.winid == S.wins.input then
      for _, h in ipairs(hits) do
        if m.line - 1 == h.row and m.wincol - 1 - LABEL_W >= h.from and m.wincol - 1 - LABEL_W < h.to then
          swallow = true
          vim.schedule(ACTIONS[h.action])
          return ""
        end
      end
    elseif m.winid == S.wins.list then
      local r = S.rows[m.line]
      local h = r and (r.hit or (r.file and r.file.hits[1]))
      if h then
        local now = uv.now()
        local double = dbl or (last_click.idx == h.idx and now - last_click.t < 450)
        last_click = { idx = h.idx, t = now }
        vim.schedule(function()
          if not is_open() then return end
          select(h.idx)
          if double then open_selected() end
        end)
      end
      swallow = true
      return ""
    elseif m.winid == S.wins.preview then
      swallow = true
      return ""
    end
  end, ns)

  api.nvim_create_user_command("FindInFiles", function() M.open("find") end, { desc = "Find in Files" })
  api.nvim_create_user_command("ReplaceInFiles", function() M.open("replace") end, { desc = "Replace in Files" })
  api.nvim_create_user_command("UndoReplaceInFiles", function() undo() end,
    { desc = "Undo Replace: put back the files the last Replace in Files changed" })
end

return M
