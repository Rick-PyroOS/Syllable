-- Local History, like CLion's: every save (auto-save too) keeps a copy of the
-- file for 30 days, separate from git, so it covers files git doesn't track.
-- The copies live in one SQLite database (stdpath("state")/local_history.sqlite3),
-- compressed, with identical copies stored once; a burst of auto-saves within
-- a minute keeps only the newest.
--
--   Space gh   this file's versions, newest first, with a diff against now
--   Space gH   the project's history: every file's changes, newest first
--              (a file's saves a few minutes apart are one row), files you
--              have since deleted, and labels. Right-click a folder in the
--              project tree > Local History for just that folder.
--   Space gL   put a label ("before the refactor") on this moment
-- In the lists:
--   Enter    compare that version with the file now (the » arrows or Accept
--            Left take a change back); a deleted file opens read-only
--   Ctrl+R   restore that version (one undo puts it back; a deleted file is
--            written back to disk)
--   Ctrl+L   put a label
-- Copies from the older file-per-version store are moved in on first start.

local M = {}
local sqlite = require("config.sqlite")
local state = vim.fn.stdpath("state")
local DB_PATH = state .. "/local_history.sqlite3"
local OLD_DIR = state .. "/local_history"
local KEEP_DAYS, MAX_BYTES = 30, 1024 * 1024
local MERGE_MS = 60 * 1000 -- auto-saves closer than this replace each other
local GROUP_MS = 5 * 60 * 1000 -- the project list joins a file's saves closer than this
local MAX_ROWS = 3000

local db, db_err
local function open()
  if db or db_err then return db end
  local d, err = sqlite.open(DB_PATH)
  if not d then
    db_err = err
    vim.notify("Local History is off: " .. tostring(err), vim.log.levels.WARN)
    return nil
  end
  d:exec([[
    PRAGMA journal_mode = WAL;
    PRAGMA synchronous = NORMAL;
    CREATE TABLE IF NOT EXISTS content (
      hash TEXT PRIMARY KEY, size INTEGER NOT NULL, zipped INTEGER NOT NULL, data BLOB NOT NULL);
    CREATE TABLE IF NOT EXISTS versions (
      id INTEGER PRIMARY KEY, path TEXT NOT NULL, time INTEGER NOT NULL, hash TEXT NOT NULL,
      auto INTEGER NOT NULL DEFAULT 0, added INTEGER, removed INTEGER);
    CREATE INDEX IF NOT EXISTS versions_path ON versions (path, time);
    CREATE INDEX IF NOT EXISTS versions_time ON versions (time);
    CREATE TABLE IF NOT EXISTS labels (
      id INTEGER PRIMARY KEY, root TEXT NOT NULL, time INTEGER NOT NULL, name TEXT NOT NULL);
  ]])
  db = d
  return db
end
M.db = open

local function now_ms()
  local t = vim.uv.clock_gettime("realtime")
  return t.sec * 1000 + math.floor(t.nsec / 1e6)
end

local function read_file(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local s = f:read("*a")
  f:close()
  return s
end

local function storable(text) return text and #text <= MAX_BYTES and not text:find("\0", 1, true) end

local function put_content(d, text)
  local hash = vim.fn.sha256(text)
  if not d:first("SELECT 1 AS x FROM content WHERE hash = ?", hash) then
    local packed = sqlite.deflate(text)
    local zipped = packed ~= nil and #packed < #text
    d:run("INSERT INTO content (hash, size, zipped, data) VALUES (?, ?, ?, ?)", hash, #text, zipped and 1 or 0,
      sqlite.blob(zipped and packed or text))
  end
  return hash
end

local function content(hash)
  local d = open()
  local r = d and hash and d:first("SELECT size, zipped, data FROM content WHERE hash = ?", hash)
  if not r then return nil end
  if r.zipped == 1 then return sqlite.inflate(r.data, r.size) end
  return r.data
end
M.content = content

-- Lines added and removed going from old to new.
local function counts(old, new)
  local a, r = 0, 0
  for _, h in ipairs(vim.diff(old or "", new or "", { result_type = "indices" })) do
    r, a = r + h[2], a + h[4]
  end
  return a, r
end

-- Keep text as path's newest version (nothing if it hasn't changed).
local function record(path, text, auto)
  local d = open()
  if not d or not storable(text) then return end
  d:transaction(function()
    local last = d:first("SELECT id, time, hash, auto FROM versions WHERE path = ? ORDER BY time DESC, id DESC LIMIT 1", path)
    if last and last.hash == vim.fn.sha256(text) then return end
    local t = now_ms()
    local replace = auto and last and last.auto == 1 and t - last.time < MERGE_MS
    local base = last
    if replace then
      base = d:first("SELECT hash FROM versions WHERE path = ? AND id <> ? ORDER BY time DESC, id DESC LIMIT 1", path, last.id)
    end
    local hash = put_content(d, text)
    local added, removed
    if base then added, removed = counts(content(base.hash), text) end
    if replace then
      d:run("UPDATE versions SET time = ?, hash = ?, added = ?, removed = ? WHERE id = ?", t, hash, added, removed, last.id)
    else
      d:run("INSERT INTO versions (path, time, hash, auto, added, removed) VALUES (?, ?, ?, ?, ?, ?)", path, t, hash,
        auto and 1 or 0, added, removed)
    end
  end)
end

M.record = record -- find_in_files.lua keeps the before/after of each Replace in Files

local function save(buf)
  local path = vim.api.nvim_buf_get_name(buf)
  if path == "" or vim.bo[buf].buftype ~= "" then return end
  local ok, err = pcall(record, path, read_file(path), vim.g.rick_autosaving == true)
  if not ok then vim.notify("Local History: " .. tostring(err), vim.log.levels.WARN) end
end

local function purge()
  local d = open()
  if not d then return end
  local cutoff = now_ms() - KEEP_DAYS * 86400000
  d:transaction(function()
    d:run("DELETE FROM versions WHERE time < ?", cutoff)
    d:run("DELETE FROM labels WHERE time < ?", cutoff)
    d:run("DELETE FROM content WHERE hash NOT IN (SELECT hash FROM versions)")
  end)
end

-- Move the older store (a folder per file, a file per version) into the database.
local function import_old()
  if not vim.uv.fs_stat(OLD_DIR) then return end
  local d = open()
  if not d then return end
  local n = 0
  d:transaction(function()
    for name, kind in vim.fs.dir(OLD_DIR) do
      if kind == "directory" then
        local path = name:gsub("%%", "/")
        local list = {}
        for f, k in vim.fs.dir(OLD_DIR .. "/" .. name) do
          if k == "file" and f:match("^%d+$") then list[#list + 1] = f end
        end
        table.sort(list)
        local prev
        for _, f in ipairs(list) do
          local text = read_file(OLD_DIR .. "/" .. name .. "/" .. f)
          if storable(text) then
            local added, removed
            if prev then added, removed = counts(prev, text) end
            d:run("INSERT INTO versions (path, time, hash, auto, added, removed) VALUES (?, ?, ?, 0, ?, ?)", path,
              tonumber(f), put_content(d, text), added, removed)
            prev, n = text, n + 1
          end
        end
      end
    end
  end)
  os.rename(OLD_DIR, OLD_DIR .. ".imported")
  if n > 0 then vim.notify(("Local History moved %d saved copies into its database"):format(n)) end
end

-- Showing it ---------------------------------------------------------------------------

local function ago(ms)
  local s = os.time() - math.floor(ms / 1000)
  if s < 60 then return "just now" end
  if s < 3600 then return math.floor(s / 60) .. " min ago" end
  if s < 86400 then return math.floor(s / 3600) .. " h ago" end
  return math.floor(s / 86400) .. " days ago"
end

local function stamp(ms) return os.date("%Y-%m-%d %H:%M:%S", math.floor(ms / 1000)) end

-- "Today 17:34", "Yesterday 09:02", "Oct 12 17:34" for the lists.
local function when(ms)
  local t = math.floor(ms / 1000)
  local day = os.date("%Y-%m-%d", t)
  if day == os.date("%Y-%m-%d") then return os.date("Today     %H:%M", t) end
  if day == os.date("%Y-%m-%d", os.time() - 86400) then return os.date("Yesterday %H:%M", t) end
  return os.date("%b %d    %H:%M", t)
end

local function project_root(path)
  return vim.fs.root(path ~= "" and path or vim.fn.getcwd(), { ".nvim.lua", ".git" }) or vim.fn.getcwd()
end

local function rel(path, root)
  return path:sub(1, #root + 1) == root .. "/" and path:sub(#root + 2) or vim.fn.fnamemodify(path, ":~")
end

-- The file's text now: its buffer if loaded, else the disk.
local function current_text(path)
  local b = vim.fn.bufnr(path)
  if b > 0 and vim.api.nvim_buf_is_loaded(b) then
    return table.concat(vim.api.nvim_buf_get_lines(b, 0, -1, false), "\n") .. "\n"
  end
  return read_file(path)
end

local function unified(old, new, name, left, right)
  local diff = vim.diff(old or "", new or "", { result_type = "unified", ctxlen = 3 })
  return "diff --git a/" .. name .. " b/" .. name .. "\n--- a/" .. name .. "\t" .. left .. "\n+++ b/" .. name .. "\t"
    .. right .. "\n" .. (diff ~= "" and diff or "@@ -1 +1 @@\n same as " .. right .. "\n")
end

local function to_lines(text) return vim.split((text or ""):gsub("\n$", ""), "\n", { plain = true }) end

local function file_buf(path)
  local b = vim.fn.bufadd(path)
  vim.fn.bufload(b)
  vim.bo[b].buflisted = true
  return b
end

local function compare(item)
  local text = content(item.hash)
  local name = vim.fn.fnamemodify(item.path, ":t")
  if not vim.uv.fs_stat(item.path) then
    -- Deleted since: show that version read-only.
    vim.cmd("tabnew")
    local b = vim.api.nvim_get_current_buf()
    vim.api.nvim_buf_set_lines(b, 0, -1, false, to_lines(text))
    vim.bo[b].buftype, vim.bo[b].bufhidden, vim.bo[b].modifiable = "nofile", "wipe", false
    vim.bo[b].filetype = vim.filetype.match({ filename = item.path }) or ""
    pcall(vim.api.nvim_buf_set_name, b, name .. " (deleted, " .. stamp(item.time) .. ")")
    return vim.notify(name .. " was deleted. Ctrl+R on it in Local History writes it back.")
  end
  require("config.diff").compare(to_lines(text), file_buf(item.path), "Local History: " .. stamp(item.time),
    name .. " (now)")
end

local function restore(item)
  local text = content(item.hash)
  if not text then return end
  local name = vim.fn.fnamemodify(item.path, ":t")
  local b = vim.fn.bufnr(item.path)
  if b > 0 and vim.api.nvim_buf_is_loaded(b) then
    vim.api.nvim_buf_set_lines(b, 0, -1, false, to_lines(text))
    return vim.notify("Restored " .. name .. " from " .. stamp(item.time) .. " (undo puts it back)")
  end
  -- Not open: keep what's on disk first, so this can be undone from here too.
  local old = read_file(item.path)
  if old then pcall(record, item.path, old, false) end
  vim.fn.mkdir(vim.fs.dirname(item.path), "p")
  local f = io.open(item.path, "wb")
  if not f then return vim.notify("Couldn't write " .. item.path, vim.log.levels.ERROR) end
  f:write(text)
  f:close()
  pcall(record, item.path, text, false)
  vim.cmd.edit(vim.fn.fnameescape(item.path))
  vim.notify((old and "Restored " or "Brought back ") .. name .. " from " .. stamp(item.time))
end

function M.put_label(root)
  root = root or project_root(vim.api.nvim_buf_get_name(0))
  vim.ui.input({ prompt = "Label for this moment: " }, function(name)
    if not name or vim.trim(name) == "" then return end
    -- Unsaved edits go in first, so the label covers them.
    vim.g.rick_autosaving = true
    pcall(vim.cmd, "silent! wall")
    vim.g.rick_autosaving = false
    local d = open()
    if not d then return end
    d:run("INSERT INTO labels (root, time, name) VALUES (?, ?, ?)", root, now_ms(), vim.trim(name))
    vim.notify("Label \"" .. vim.trim(name) .. "\" put on " .. vim.fn.fnamemodify(root, ":t"))
  end)
end

local function labels_for(d, path)
  local out = {}
  for _, l in ipairs(d:rows("SELECT root, time, name FROM labels ORDER BY time DESC")) do
    if path == l.root or path:sub(1, #l.root + 1) == l.root .. "/" or l.root:sub(1, #path + 1) == path .. "/" then
      out[#out + 1] = l
    end
  end
  return out
end

local function label_item(l)
  return { label = l.name, time = l.time, text = l.name, preview = { loc = false,
    text = "Label \"" .. l.name .. "\", put " .. stamp(l.time) .. " on " .. vim.fn.fnamemodify(l.root, ":~")
      .. ".\n\nThe rows below it are from before it." } }
end

-- Merge labels into items (both newest first).
local function with_labels(items, labels)
  local out, i = {}, 1
  for _, it in ipairs(items) do
    while labels[i] and labels[i].time >= it.time do
      out[#out + 1] = label_item(labels[i])
      i = i + 1
    end
    out[#out + 1] = it
  end
  for j = i, #labels do out[#out + 1] = label_item(labels[j]) end
  return out
end

local TAG = "\239\128\171" -- nf-fa-tag

local function picker(name, title, items, root)
  require("config.dialogs").open(name, function()
    return Snacks.picker({
      title = title,
      items = items,
      sort = { fields = { "idx" } },
      preview = function(ctx)
        local it = ctx.item
        if it.label then return Snacks.picker.preview.preview(ctx) end
        if not it.diff then it.diff = it.make_diff() end
        return Snacks.picker.preview.diff(ctx)
      end,
      format = function(it)
        if it.label then
          return { { when(it.time) .. "  ", "Special" }, { TAG .. " " .. it.label, "Special" } }
        end
        local out = { { when(it.time) .. "  ", "Comment" } }
        if it.rel then out[#out + 1] = { it.rel .. "  ", it.deleted and "DiagnosticError" or nil } end
        if it.deleted then out[#out + 1] = { "deleted  ", "DiagnosticError" } end
        if it.added then
          out[#out + 1] = { "+" .. it.added, "Added" }
          out[#out + 1] = { " −" .. it.removed .. "  ", "Removed" }
        elseif it.first and not it.deleted then
          out[#out + 1] = { "first copy  ", "Comment" }
        end
        if (it.saves or 1) > 1 then out[#out + 1] = { it.saves .. " saves", "Comment" } end
        if not it.rel then out[#out + 1] = { ago(it.time), "Comment" } end
        return out
      end,
      confirm = function(p, it)
        p:close()
        if it and not it.label then vim.schedule(function() compare(it) end) end
      end,
      actions = {
        restore = function(p, it)
          if not it or it.label then return end
          p:close()
          vim.schedule(function() restore(it) end)
        end,
        put_label = function(p)
          p:close()
          vim.schedule(function() M.put_label(root) end)
        end,
      },
      win = { input = { keys = {
        ["<C-r>"] = { "restore", mode = { "i", "n" } },
        ["<C-l>"] = { "put_label", mode = { "i", "n" } },
      } } },
    })
  end)
end

-- One file's versions, each diffed against the file now.
function M.show(path)
  path = path or vim.api.nvim_buf_get_name(0)
  local d = open()
  if not d or path == "" then return end
  local rows = d:rows("SELECT id, time, hash, added, removed FROM versions WHERE path = ? ORDER BY time DESC, id DESC LIMIT ?",
    path, MAX_ROWS)
  if #rows == 0 then return vim.notify("No local history for this file yet (it starts at the next save)") end
  local name = vim.fn.fnamemodify(path, ":t")
  local items = {}
  for _, r in ipairs(rows) do
    items[#items + 1] = { path = path, hash = r.hash, time = r.time, text = stamp(r.time), added = r.added,
      removed = r.removed, first = r.added == nil,
      make_diff = function() return unified(content(r.hash), current_text(path), name, stamp(r.time), "now") end }
  end
  picker("local_history", "Local History: " .. name, with_labels(items, labels_for(d, path)), project_root(path))
end

-- Every file under dir (the project by default), newest first.
function M.show_project(dir)
  local d = open()
  if not d then return end
  local root = (dir or project_root(vim.api.nvim_buf_get_name(0))):gsub("/$", "")
  local rows = d:rows([[SELECT id, path, time, hash, added, removed FROM versions
    WHERE path > ? AND path < ? ORDER BY time DESC, id DESC LIMIT ?]], root .. "/", root .. "0", MAX_ROWS)
  if #rows == 0 then
    return vim.notify("No local history under " .. vim.fn.fnamemodify(root, ":~") .. " yet")
  end
  -- A file's saves a few minutes apart become one row.
  local groups, g = {}, nil
  for _, r in ipairs(rows) do
    if g and g.path == r.path and g.oldest.time - r.time < GROUP_MS then
      g.oldest, g.saves = r, g.saves + 1
    else
      g = { path = r.path, hash = r.hash, time = r.time, newest = r, oldest = r, saves = 1 }
      groups[#groups + 1] = g
    end
  end
  local seen = {}
  for _, it in ipairs(groups) do
    local o = it.oldest
    local base = d:first([[SELECT hash FROM versions WHERE path = ? AND (time < ? OR (time = ? AND id < ?))
      ORDER BY time DESC, id DESC LIMIT 1]], it.path, o.time, o.time, o.id)
    it.base = base and base.hash
    if it.saves == 1 then
      it.added, it.removed = it.newest.added, it.newest.removed
    elseif it.base then
      it.added, it.removed = counts(content(it.base), content(it.hash))
    end
    it.first = not it.base
    it.rel = rel(it.path, root)
    it.text = it.rel
    if not seen[it.path] then
      seen[it.path] = true
      it.deleted = vim.uv.fs_stat(it.path) == nil
    end
    it.make_diff = function()
      return unified(it.base and content(it.base) or "", content(it.hash), it.rel, it.base and "before" or "(new)",
        stamp(it.time))
    end
  end
  picker("local_history_project", "Local History: " .. vim.fn.fnamemodify(root, ":t") .. "/",
    with_labels(groups, labels_for(d, root)), root)
end

function M.setup()
  vim.api.nvim_create_autocmd("BufWritePost", {
    group = vim.api.nvim_create_augroup("rick_local_history", { clear = true }),
    callback = function(ev) save(ev.buf) end,
  })
  -- Tidy up after startup: bring over old copies, drop ones past 30 days.
  vim.defer_fn(function()
    if not sqlite.available() then return end
    local ok, err = pcall(function() import_old() purge() end)
    if not ok then vim.notify("Local History: " .. tostring(err), vim.log.levels.WARN) end
  end, 2000)
  vim.keymap.set("n", "<leader>gh", function() M.show() end, { desc = "Local history (this file)" })
  vim.keymap.set("n", "<leader>gH", function() M.show_project() end, { desc = "Local history (project)" })
  vim.keymap.set("n", "<leader>gL", function() M.put_label() end, { desc = "Put a Local History label" })
end

return M
