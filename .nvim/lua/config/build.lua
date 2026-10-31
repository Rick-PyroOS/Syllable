-- Build, like CLion's: Cmd+F9 (Space rb) builds in the background while you
-- keep editing. The output streams into the Build view of the bottom panel's
-- Run tab (Cmd+0 / Space ro shows / hides it); errors and warnings land in the margin and in Problems
-- (Space fd), and F2 / Shift+F2 walk the errors across files. Cmd+Shift+F9
-- (Space rf) recompiles just the current file; Cmd+F2 (Space rs) stops the build.
--
-- What runs is per project, set in the project's .nvim.lua:
--   vim.g.build_cmd       = "make -j8"            -- string, list, or function() -> one of those
--   vim.g.build_file_cmd  = "make -C {filedir} {file:t:r}.o"  -- {file} (from the root), {file:r},
--                                                     {file:t}, {file:t:r}, {filedir}, {dir} are filled in
--   vim.g.build_dir       = "/path/to/run/in"     -- default: the project root
--   vim.g.build_errorformat = "..."               -- default: Vim's, which knows gcc/clang and make
--   vim.g.build_path_map  = { ["/work/src"] = "/Users/me/Syllable" }  -- a build in a container
--                                                     names its own paths; these turn them into yours
-- With nothing set, a folder with a Makefile builds with make -j<cores>, and
-- one with a CMakeLists.txt with CMake (config/cmake.lua).

local M = {}
-- vim.g.build_cmd, or the chosen Run/Debug Configuration's (config/run_configs.lua).
local S = require("config.run_configs").settings
local ns = vim.api.nvim_create_namespace("rick_build")
local job -- the running vim.SystemObj
local out_buf, started, last_items = nil, nil, {}
local errors = {} -- build errors in order, for F2

local function root()
  -- The project root (its .nvim.lua or .git), not the nearest sub-Makefile.
  return vim.g.build_dir or vim.fs.root(0, { ".nvim.lua", ".git" }) or vim.fs.root(0, { "Makefile", "makefile" })
    or vim.fn.getcwd()
end

local function cores()
  local ok, n = pcall(function() return #vim.uv.cpu_info() end)
  return ok and n or 4
end

local function resolve(cmd)
  if type(cmd) == "function" then cmd = cmd() end
  return cmd
end

local function default_cmd(dir)
  for _, f in ipairs({ "Makefile", "makefile", "GNUmakefile" }) do
    if vim.uv.fs_stat(dir .. "/" .. f) then return "make -j" .. cores() end
  end
  return require("config.cmake").default_build(dir)
end

local function fill(cmd, file, dir)
  local function sub(s)
    local filedir = vim.fn.fnamemodify(file, ":h")
    return (s:gsub("{file:t:r}", vim.fn.fnamemodify(file, ":t:r")):gsub("{file:r}", vim.fn.fnamemodify(file, ":r"))
      :gsub("{file:t}", vim.fn.fnamemodify(file, ":t")):gsub("{filedir}", filedir == "" and "." or filedir)
      :gsub("{file}", file):gsub("{dir}", dir))
  end
  if type(cmd) == "table" then return vim.tbl_map(sub, cmd) end
  return sub(cmd)
end

-- The build's output ----------------------------------------------------------

-- The build's output is a view in the Run tab of the bottom tool window
-- (config/run.lua, config/toolwindow.lua): its window, when it shows.
local function panel_win()
  local tw = require("config.toolwindow")
  local w = tw.is_showing("run") and tw.win()
  return w and out_buf and vim.api.nvim_win_get_buf(w) == out_buf and w or nil
end

local function ensure_buf()
  if out_buf and vim.api.nvim_buf_is_valid(out_buf) then return out_buf end
  out_buf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_buf_set_name(out_buf, "Build")
  vim.bo[out_buf].filetype = "rick_build"
  vim.bo[out_buf].bufhidden = "hide"
  vim.api.nvim_buf_set_lines(out_buf, 0, -1, false, { "", "  Nothing has been built yet (Cmd+F9 or Space rb builds)" })
  vim.bo[out_buf].modifiable = false
  local function k(lhs, fn, desc) vim.keymap.set("n", lhs, fn, { buffer = out_buf, nowait = true, desc = desc }) end
  k("<CR>", function() M.open_line(vim.api.nvim_win_get_cursor(0)[1]) end, "Go to this error")
  k("q", function() M.toggle_panel(false) end, "Hide the panel")
  k("<2-LeftMouse>", function() M.open_line(vim.api.nvim_win_get_cursor(0)[1]) end, "Go to this error")
  return out_buf
end

function M.buffer() return ensure_buf() end

function M.toggle_panel(show)
  local w = panel_win()
  if show == nil then show = not w end
  if not show then
    if w then require("config.toolwindow").hide() end
    return
  end
  if w then return w end
  return require("config.run").show_build(ensure_buf())
end

local status = ""
function _G.RickBuildStatus() return status end

local function append(lines)
  local b = ensure_buf()
  vim.bo[b].modifiable = true
  local n = vim.api.nvim_buf_line_count(b)
  local empty = n == 1 and vim.api.nvim_buf_get_lines(b, 0, 1, false)[1] == ""
  vim.api.nvim_buf_set_lines(b, empty and 0 or n, -1, false, lines)
  vim.bo[b].modifiable = false
  local w = panel_win()
  if w and vim.api.nvim_get_current_win() ~= w then
    vim.api.nvim_win_set_cursor(w, { vim.api.nvim_buf_line_count(b), 0 }) -- follow the output
  end
end

-- Errors ---------------------------------------------------------------------

local SEV = { e = vim.diagnostic.severity.ERROR, w = vim.diagnostic.severity.WARN, n = vim.diagnostic.severity.HINT,
  i = vim.diagnostic.severity.INFO }

-- Turn the output into quickfix-style items with Vim's errorformat (which
-- follows make's "Entering directory" lines for relative paths).
local function parse(lines, dir)
  local efm = vim.g.build_errorformat or vim.go.errorformat
  local map = vim.g.build_path_map
  if type(map) == "table" and next(map) then
    local mapped = {}
    for i, l in ipairs(lines) do
      for from, to in pairs(map) do
        local at = l:find(from, 1, true)
        while at do
          l = l:sub(1, at - 1) .. to .. l:sub(at + #from)
          at = l:find(from, at + #to, true)
        end
      end
      mapped[i] = l
    end
    lines = mapped
  end
  local items = vim.fn.getqflist({ lines = lines, efm = efm }).items
  local out = {}
  for i, it in ipairs(items) do
    if it.valid == 1 and it.bufnr > 0 and it.lnum > 0 then
      local t = (it.type or ""):lower()
      if t == "" then
        local text = it.text:lower()
        t = text:match("^%s*warning") and "w" or text:match("^%s*note") and "n" or "e"
      end
      out[#out + 1] = {
        bufnr = it.bufnr, lnum = it.lnum, col = math.max(it.col, 1), type = t,
        text = vim.trim((it.text:gsub("^%s*[Ee]rror:%s*", ""):gsub("^%s*[Ww]arning:%s*", ""))), out_line = i,
      }
    end
  end
  return out
end

local function publish(items)
  -- Clear the last build's markers everywhere, then set this one's.
  for _, b in ipairs(vim.api.nvim_list_bufs()) do vim.diagnostic.reset(ns, b) end
  local per = {}
  for _, it in ipairs(items) do
    per[it.bufnr] = per[it.bufnr] or {}
    table.insert(per[it.bufnr], {
      lnum = it.lnum - 1, col = it.col - 1, severity = SEV[it.type] or SEV.e, message = it.text, source = "build",
    })
  end
  for b, d in pairs(per) do vim.diagnostic.set(ns, b, d) end
  errors = vim.tbl_filter(function(it) return it.type == "e" end, items)
  vim.fn.setqflist({}, " ", {
    title = "Build",
    items = vim.tbl_map(function(it)
      return { bufnr = it.bufnr, lnum = it.lnum, col = it.col, type = it.type:upper(), text = it.text }
    end, items),
  })
  last_items = items
end

-- Highlight the error and warning lines in the panel.
local function mark_panel(items)
  local b = ensure_buf()
  vim.api.nvim_buf_clear_namespace(b, ns, 0, -1)
  for _, it in ipairs(items) do
    local hl = it.type == "e" and "DiagnosticError" or it.type == "w" and "DiagnosticWarn" or "DiagnosticInfo"
    pcall(vim.api.nvim_buf_set_extmark, b, ns, it.out_line - 1 + M._offset, 0, { end_row = it.out_line + M._offset, end_col = 0, hl_group = hl, priority = 50 })
    pcall(vim.api.nvim_buf_set_extmark, b, ns, it.out_line - 1 + M._offset, 0, { virt_text = { { it.type == "e" and " ● " or " ▲ ", hl } }, virt_text_pos = "inline" })
  end
end
M._offset = 0

local function jump(it)
  local target
  for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if w ~= panel_win() and vim.api.nvim_win_get_config(w).relative == "" and vim.bo[vim.api.nvim_win_get_buf(w)].buftype == "" then
      target = w
      break
    end
  end
  if target then vim.api.nvim_set_current_win(target) end
  vim.bo[it.bufnr].buflisted = true -- a tab for it, like any file you open
  vim.cmd("buffer " .. it.bufnr)
  vim.api.nvim_win_set_cursor(0, { it.lnum, it.col - 1 })
  vim.cmd("normal! zv")
end

function M.open_line(row)
  for _, it in ipairs(last_items) do
    if it.out_line + M._offset == row then return jump(it) end
  end
end

-- F2 / Shift+F2: next / previous error. Build errors first, across files, in
-- the order the compiler gave them; without a failed build, the errors in this file.
function M.next_error(dir)
  local live = vim.tbl_filter(function(it) return vim.api.nvim_buf_is_valid(it.bufnr) end, errors)
  if #live == 0 then
    return vim.diagnostic.jump({ count = dir, severity = vim.diagnostic.severity.ERROR })
  end
  -- Where are we in the list? Use the diagnostics' current positions (they follow edits).
  local buf, row = vim.api.nvim_get_current_buf(), vim.api.nvim_win_get_cursor(0)[1]
  local list = {}
  for _, it in ipairs(live) do
    local d = vim.tbl_filter(function(x) return x.severity == vim.diagnostic.severity.ERROR and x.message == it.text end,
      vim.diagnostic.get(it.bufnr, { namespace = ns }))
    list[#list + 1] = { bufnr = it.bufnr, lnum = d[1] and d[1].lnum + 1 or it.lnum, col = d[1] and d[1].col + 1 or it.col, text = it.text }
  end
  local idx
  for i, it in ipairs(list) do
    if it.bufnr == buf and it.lnum == row then idx = i end
  end
  if not idx then
    -- Not on one: the first one after the cursor in this file, else the first in the list.
    idx = 0
    for i, it in ipairs(list) do
      if it.bufnr == buf and it.lnum > row then idx = i - 1; break end
    end
    if dir < 0 then idx = idx + 1 end
  end
  local n = ((idx - 1 + dir) % #list) + 1
  jump(list[n])
  vim.api.nvim_echo({ { ("Build error %d of %d: "):format(n, #list), "DiagnosticError" }, { list[n].text } }, false, {})
end

-- Running --------------------------------------------------------------------

local function finish(code, items, cmd_str, dir)
  local secs = started and (vim.uv.hrtime() - started) / 1e9 or 0
  local ne = #vim.tbl_filter(function(i) return i.type == "e" end, items)
  local nw = #vim.tbl_filter(function(i) return i.type == "w" end, items)
  local counts = (ne > 0 and (ne .. " error" .. (ne == 1 and "" or "s")) or "")
    .. (ne > 0 and nw > 0 and ", " or "") .. (nw > 0 and (nw .. " warning" .. (nw == 1 and "" or "s")) or "")
  local took = secs < 60 and ("%.1f s"):format(secs) or ("%d min %d s"):format(secs / 60, secs % 60)
  if code == "stopped" then
    status = "  stopped"
    append({ "", "Build stopped." })
    vim.notify("Build stopped", vim.log.levels.WARN)
  elseif code == 0 then
    status = "  ✓ finished in " .. took .. (nw > 0 and ("   " .. counts) or "")
    append({ "", "Build finished in " .. took .. (nw > 0 and (" with " .. counts) or "") .. "." })
    vim.notify("Build finished in " .. took .. (nw > 0 and (", " .. counts) or ""))
  else
    status = "  ✗ failed" .. (counts ~= "" and (": " .. counts) or "") .. " in " .. took .. "   F2 next error · Enter on a line opens it"
    append({ "", "Build failed (exit " .. code .. ")" .. (counts ~= "" and (": " .. counts) or "") .. "." })
    vim.notify("Build failed" .. (counts ~= "" and (": " .. counts) or "") .. "\nF2 goes to the next error; Space fd lists them.", vim.log.levels.ERROR, { title = "Build" })
    -- Show the first error in the panel rather than the tail of make's output.
    local w = M.toggle_panel(true)
    local first = vim.tbl_filter(function(i) return i.type == "e" end, items)[1]
    if w and first then
      vim.api.nvim_win_set_cursor(w, { first.out_line + M._offset, 0 })
      vim.api.nvim_win_call(w, function() vim.cmd("normal! zt") end)
    end
  end
  vim.cmd("redrawstatus!")
  vim.api.nvim_exec_autocmds("User", { pattern = "RickBuildDone", data = { code = code, errors = ne, warnings = nw } })
end

function M.run(cmd, label)
  if job then
    return vim.notify("A build is already running: Cmd+F2 (Space rs) stops it", vim.log.levels.WARN)
  end
  local dir = root()
  cmd = resolve(cmd)
  if not cmd and require("config.cmake").is_project(dir) then
    return vim.notify("cmake isn't installed. On a Mac: brew install cmake ninja", vim.log.levels.WARN, { title = "CMake" })
  end
  if not cmd then
    return vim.notify("Nothing to build here: no Makefile, and vim.g.build_cmd isn't set (see config/build.lua)", vim.log.levels.WARN)
  end
  pcall(vim.cmd, "silent! wall") -- build what you see, like CLion
  local argv = type(cmd) == "table" and cmd or { vim.o.shell, vim.o.shellcmdflag, cmd }
  local cmd_str = type(cmd) == "table" and table.concat(cmd, " ") or cmd
  local b = ensure_buf()
  vim.bo[b].modifiable = true
  vim.api.nvim_buf_set_lines(b, 0, -1, false, { "$ " .. cmd_str .. "    (in " .. vim.fn.fnamemodify(dir, ":~") .. ")", "" })
  vim.bo[b].modifiable = false
  vim.api.nvim_buf_clear_namespace(b, ns, 0, -1)
  M._offset = 2
  status = "  ⟳ " .. (label or "building") .. "…"
  M.toggle_panel(true)
  vim.cmd("redrawstatus!")
  local lines, partial = {}, { stdout = "", stderr = "" }
  started = vim.uv.hrtime()
  local function on_data(which)
    return function(_, data)
      if not data then return end
      local text = partial[which] .. data
      local parts = vim.split(text, "\n", { plain = true })
      partial[which] = table.remove(parts)
      if #parts > 0 then
        vim.schedule(function()
          for _, l in ipairs(parts) do lines[#lines + 1] = (l:gsub("\r$", "")) end
          append(vim.tbl_map(function(l) return (l:gsub("\r$", "")) end, parts))
        end)
      end
    end
  end
  local ok, obj = pcall(vim.system, argv, {
    cwd = dir, text = true, stdout = on_data("stdout"), stderr = on_data("stderr"),
    env = { TERM = "dumb", GCC_COLORS = "", CLICOLOR_FORCE = "0" },
  }, function(res)
    vim.schedule(function()
      local stopped = job and job.rick_stopped
      job = nil
      for _, which in ipairs({ "stdout", "stderr" }) do
        if partial[which] ~= "" then lines[#lines + 1] = partial[which]; append({ partial[which] }) end
      end
      -- errorformat resolves relative paths against the current directory.
      local items = vim.api.nvim_win_call(0, function()
        local here = vim.fn.getcwd()
        vim.cmd("noautocmd lcd " .. vim.fn.fnameescape(dir))
        local r = parse(lines, dir)
        vim.cmd("noautocmd lcd " .. vim.fn.fnameescape(here))
        return r
      end)
      publish(items)
      mark_panel(items)
      finish(stopped and "stopped" or res.code, items, cmd_str, dir)
    end)
  end)
  if not ok then
    status = "  ✗ couldn't start"
    return vim.notify("Couldn't start the build: " .. tostring(obj), vim.log.levels.ERROR)
  end
  job = obj
end

function M.build() M.run(S.build_cmd or default_cmd(root())) end

function M.build_file()
  local file = vim.api.nvim_buf_get_name(0)
  if file == "" then return end
  local dir = root()
  local rel = vim.fs.relpath and vim.fs.relpath(dir, file) or file
  local cmd = vim.g.build_file_cmd
  if not cmd then
    if not default_cmd(dir) then return M.build() end
    cmd = "make -C {filedir} {file:t:r}.o" -- the Makefile next to the file builds its .o
  end
  M.run(fill(resolve(cmd), rel or file, dir), "compiling " .. vim.fn.fnamemodify(file, ":t"))
end

function M.stop()
  if not job then return vim.notify("No build is running") end
  job.rick_stopped = true
  job:kill(15)
end

function M.running() return job ~= nil end

function M.setup()
  local function hl()
    vim.api.nvim_set_hl(0, "RickBuildTitle", { link = "WinBar", default = true })
  end
  hl()
  vim.api.nvim_create_autocmd("ColorScheme", { group = vim.api.nvim_create_augroup("rick_build", { clear = true }), callback = hl })
  local map = vim.keymap.set
  map("n", "<D-F9>", M.build, { desc = "Build" })
  map("n", "<leader>rb", M.build, { desc = "Build" })
  map("n", "<D-S-F9>", M.build_file, { desc = "Recompile this file" })
  map("n", "<leader>rf", M.build_file, { desc = "Recompile this file" })
  map("n", "<D-F2>", M.stop, { desc = "Stop the build" })
  map("n", "<leader>rs", M.stop, { desc = "Stop the build" })
  map("n", "<leader>ro", function() M.toggle_panel() end, { desc = "Show / hide the build's output" })
  map("n", "<F2>", function() M.next_error(1) end, { desc = "Next error" })
  map("n", "<S-F2>", function() M.next_error(-1) end, { desc = "Previous error" })
  map("n", "<F14>", function() M.next_error(-1) end, { desc = "Previous error" }) -- Shift+F2 as terminals send it
end

return M
