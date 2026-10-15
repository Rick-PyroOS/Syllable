-- A small SQLite binding over LuaJIT's FFI, using the libsqlite3 macOS ships
-- (and Linux's libsqlite3.so.0), so no plugin or build step is needed.
--   local db = require("config.sqlite").open(path)
--   db:exec("CREATE TABLE ...")                      -- statements, no results
--   db:run("INSERT INTO t VALUES (?, ?)", a, b)      -- one statement with values
--   for _, row in ipairs(db:rows("SELECT ...", x)) do print(row.name) end
--   db:transaction(function() ... end)
-- Strings bind as TEXT; wrap bytes in sqlite.blob(s) to store a BLOB.
-- Also zlib's compress / uncompress (M.deflate / M.inflate) when libz loads.

local ffi = require("ffi")
local M = {}

pcall(ffi.cdef, [[
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
int sqlite3_open_v2(const char *filename, sqlite3 **db, int flags, const char *vfs);
int sqlite3_close_v2(sqlite3 *db);
const char *sqlite3_errmsg(sqlite3 *db);
int sqlite3_busy_timeout(sqlite3 *db, int ms);
int sqlite3_exec(sqlite3 *db, const char *sql, void *cb, void *arg, char **err);
void sqlite3_free(void *p);
int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int n, sqlite3_stmt **stmt, const char **tail);
int sqlite3_bind_text(sqlite3_stmt *s, int i, const char *v, int n, void (*d)(void *));
int sqlite3_bind_blob(sqlite3_stmt *s, int i, const void *v, int n, void (*d)(void *));
int sqlite3_bind_int64(sqlite3_stmt *s, int i, int64_t v);
int sqlite3_bind_double(sqlite3_stmt *s, int i, double v);
int sqlite3_bind_null(sqlite3_stmt *s, int i);
int sqlite3_step(sqlite3_stmt *s);
int sqlite3_column_count(sqlite3_stmt *s);
const char *sqlite3_column_name(sqlite3_stmt *s, int i);
int sqlite3_column_type(sqlite3_stmt *s, int i);
int64_t sqlite3_column_int64(sqlite3_stmt *s, int i);
double sqlite3_column_double(sqlite3_stmt *s, int i);
const void *sqlite3_column_blob(sqlite3_stmt *s, int i);
int sqlite3_column_bytes(sqlite3_stmt *s, int i);
int sqlite3_finalize(sqlite3_stmt *s);
int64_t sqlite3_last_insert_rowid(sqlite3 *db);
unsigned long compressBound(unsigned long n);
int compress2(uint8_t *dest, unsigned long *destLen, const uint8_t *src, unsigned long srcLen, int level);
int uncompress(uint8_t *dest, unsigned long *destLen, const uint8_t *src, unsigned long srcLen);
]])

local SQLITE_OK, SQLITE_ROW, SQLITE_DONE = 0, 100, 101
local INTEGER, FLOAT, NULL = 1, 2, 5
local TRANSIENT = ffi.cast("void (*)(void *)", -1)

local function load(names)
  for i = 1, table.maxn(names) do
    if names[i] then
      local ok, lib = pcall(ffi.load, names[i])
      if ok then return lib end
    end
  end
end

local lib = load({ vim.g.sqlite_clib_path, "sqlite3", "libsqlite3.so.0", "/usr/lib/libsqlite3.dylib",
  "/opt/homebrew/opt/sqlite/lib/libsqlite3.dylib" })
local z = load({ "z", "libz.so.1", "/usr/lib/libz.dylib" })

function M.available() return lib ~= nil end

local Blob = {}
function M.blob(s) return setmetatable({ s }, Blob) end

local DB = {}
DB.__index = DB

function M.open(path)
  if not lib then return nil, "libsqlite3 not found" end
  local pp = ffi.new("sqlite3*[1]")
  -- READWRITE | CREATE | FULLMUTEX
  if lib.sqlite3_open_v2(path, pp, 0x02 + 0x04 + 0x10000, nil) ~= SQLITE_OK then
    local err = pp[0] ~= nil and ffi.string(lib.sqlite3_errmsg(pp[0])) or "can't open"
    if pp[0] ~= nil then lib.sqlite3_close_v2(pp[0]) end
    return nil, err
  end
  lib.sqlite3_busy_timeout(pp[0], 3000) -- another Neovim may be writing
  return setmetatable({ h = pp[0] }, DB)
end

function DB:close()
  if self.h then lib.sqlite3_close_v2(self.h) end
  self.h = nil
end

function DB:error() return ffi.string(lib.sqlite3_errmsg(self.h)) end

function DB:exec(sql)
  local err = ffi.new("char*[1]")
  if lib.sqlite3_exec(self.h, sql, nil, nil, err) ~= SQLITE_OK then
    local msg = err[0] ~= nil and ffi.string(err[0]) or self:error()
    if err[0] ~= nil then lib.sqlite3_free(err[0]) end
    error("sqlite: " .. msg, 2)
  end
end

local function bind(self, stmt, i, v)
  local t = type(v)
  local rc
  if v == nil then rc = lib.sqlite3_bind_null(stmt, i)
  elseif t == "boolean" then rc = lib.sqlite3_bind_int64(stmt, i, v and 1 or 0)
  elseif t == "number" then
    if v == math.floor(v) and math.abs(v) < 2 ^ 53 then rc = lib.sqlite3_bind_int64(stmt, i, v)
    else rc = lib.sqlite3_bind_double(stmt, i, v) end
  elseif t == "string" then rc = lib.sqlite3_bind_text(stmt, i, v, #v, TRANSIENT)
  elseif getmetatable(v) == Blob then rc = lib.sqlite3_bind_blob(stmt, i, v[1], #v[1], TRANSIENT)
  else error("sqlite: can't bind a " .. t, 3) end
  if rc ~= SQLITE_OK then error("sqlite: " .. self:error(), 3) end
end

-- Run one statement; returns its rows (a list of { column = value }).
function DB:rows(sql, ...)
  local pstmt = ffi.new("sqlite3_stmt*[1]")
  if lib.sqlite3_prepare_v2(self.h, sql, #sql, pstmt, nil) ~= SQLITE_OK then error("sqlite: " .. self:error(), 2) end
  local stmt = pstmt[0]
  local n = select("#", ...)
  for i = 1, n do
    local ok, err = pcall(bind, self, stmt, i, (select(i, ...)))
    if not ok then lib.sqlite3_finalize(stmt) error(err, 2) end
  end
  local out, cols = {}, {}
  for i = 0, lib.sqlite3_column_count(stmt) - 1 do cols[i] = ffi.string(lib.sqlite3_column_name(stmt, i)) end
  while true do
    local rc = lib.sqlite3_step(stmt)
    if rc == SQLITE_DONE then break end
    if rc ~= SQLITE_ROW then
      local msg = self:error()
      lib.sqlite3_finalize(stmt)
      error("sqlite: " .. msg, 2)
    end
    local row = {}
    for i, name in pairs(cols) do
      local ty = lib.sqlite3_column_type(stmt, i)
      if ty == INTEGER then row[name] = tonumber(lib.sqlite3_column_int64(stmt, i))
      elseif ty == FLOAT then row[name] = lib.sqlite3_column_double(stmt, i)
      elseif ty ~= NULL then
        local p = lib.sqlite3_column_blob(stmt, i)
        row[name] = p ~= nil and ffi.string(p, lib.sqlite3_column_bytes(stmt, i)) or ""
      end
    end
    out[#out + 1] = row
  end
  lib.sqlite3_finalize(stmt)
  return out
end

-- Run one statement for its effect; returns the last inserted rowid.
function DB:run(sql, ...)
  self:rows(sql, ...)
  return tonumber(lib.sqlite3_last_insert_rowid(self.h))
end

function DB:first(sql, ...) return self:rows(sql, ...)[1] end

function DB:transaction(fn)
  self:exec("BEGIN IMMEDIATE")
  local ok, err = pcall(fn)
  if ok then self:exec("COMMIT") else pcall(self.exec, self, "ROLLBACK"); error(err, 2) end
end

-- zlib -----------------------------------------------------------------------------

function M.deflate(s)
  if not z then return nil end
  local n = ffi.new("unsigned long[1]", z.compressBound(#s))
  local buf = ffi.new("uint8_t[?]", n[0])
  if z.compress2(buf, n, s, #s, 6) ~= 0 then return nil end
  return ffi.string(buf, n[0])
end

function M.inflate(s, size)
  if not z then return nil end
  local n = ffi.new("unsigned long[1]", math.max(size, 1))
  local buf = ffi.new("uint8_t[?]", n[0])
  if z.uncompress(buf, n, s, #s) ~= 0 then return nil end
  return ffi.string(buf, n[0])
end

return M
