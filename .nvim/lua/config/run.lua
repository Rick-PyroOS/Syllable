-- Run and Debug, like CLion's: Shift+F10 (Space rr) builds, then runs; Shift+F9
-- (Space rd) builds, then runs under the debugger; Cmd+F2 (Space rs) stops
-- whatever is running (the build too). The program's output shows in a Run
-- panel at the bottom (a terminal: you can type into it), with a tab per log
-- the project names (a serial console, say). Space rl shows / hides it.
--
-- What runs is per project, set in the project's .nvim.lua:
--   vim.g.run_cmd     = "./sb run"              -- string, list, or function() -> one of those
--   vim.g.run_dir     = "/path/to/run/in"       -- default: the build's folder (vim.g.build_dir or the root)
--   vim.g.run_logs    = { "out/run-serial.log" } -- files shown as more tabs (followed like tail -f),
--                                                  relative to run_dir
--   vim.g.run_build_first = false              -- default true: Run and Debug build first, and stop
--                                                  if the build fails
-- Debugging (config/debug.lua does the debugger itself):
--   vim.g.debug_cmd   = "./sb run --gdb"        -- starts the program waiting for the debugger
--                                                  (QEMU's -s -S); default: no command, the
--                                                  debugger starts debug_program itself
--   vim.g.debug_target  = "localhost:1234"      -- attach to this gdb stub (QEMU, gdbserver)
--   vim.g.debug_program = "/path/to/kernel.elf" -- the file with the symbols (or function() -> path)
--   vim.g.debug_gdb     = "i686-elf-gdb"        -- default: gdb (14 or newer: it speaks DAP)
--   vim.g.debug_gdb_init = { "set architecture i386" }  -- gdb commands run before attaching
-- Or, for a program on another system with gdbserver --multi running there:
--   vim.g.debug_remote      = "localhost:2345"   -- gdb connects (extended-remote) and starts it there
--   vim.g.debug_remote_exec = "/path/on/target"  -- the program's path on that system
--                                                  (debug_program is the local copy with symbols)
--   vim.g.debug_connect_timeout = 300           -- seconds gdb keeps trying to reach it
-- vim.g.build_path_map (config/build.lua) also maps the source paths in the
-- debug info, so a program built in a container opens your files.
-- With nothing set, Run says what to set.

local M = {}
-- vim.g settings, or the chosen Run/Debug Configuration's (config/run_configs.lua).
local S = require("config.run_configs").settings

local panel = { win = nil, tabs = {}, current = nil } -- tabs: { name, buf, job }
-- The build's output (config/build.lua) is a view here too, first, and stays
-- when a new run replaces the program's tabs.
local build_tab = { name = "Build", buf = nil }
local function all_tabs()
  if build_tab.buf and vim.api.nvim_buf_is_valid(build_tab.buf) then return { build_tab, unpack(panel.tabs) } end
  return panel.tabs
end
local run_job -- the job id of the program's terminal

local function root()
  return S.run_dir or vim.g.build_dir or vim.fs.root(0, { ".nvim.lua", ".git" }) or vim.fn.getcwd()
end

local function resolve(v)
  if type(v) == "function" then v = v() end
  return v
end

local function abs(path, dir)
  if path:sub(1, 1) == "/" or path:sub(1, 1) == "~" then return vim.fs.normalize(path) end
  return vim.fs.normalize(dir .. "/" .. path)
end

-- The Run panel -------------------------------------------------------------

-- The Run tab of the bottom tool window (config/toolwindow.lua).
local function panel_valid() return require("config.toolwindow").is_showing("run") end

local function show(tab)
  if not tab or not vim.api.nvim_buf_is_valid(tab.buf) then return end
  panel.current = tab
  return require("config.toolwindow").show("run") -- follows the end, as a terminal does
end

function M.buffer()
  local t = panel.current
  if not (t and vim.api.nvim_buf_is_valid(t.buf)) then t = all_tabs()[1] end
  return t and vim.api.nvim_buf_is_valid(t.buf) and t.buf or nil
end

-- Show the build's output (the Build view of this tab); returns the window.
function M.show_build(buf)
  build_tab.buf = buf
  return show(build_tab)
end

-- The Run tab's row, as CLion's Run toolbar: buttons to Run, Debug, Stop
-- and Build (Stop is grey when nothing is running), the run configuration
-- (click it to edit the configurations), and the run's own tabs (the
-- program, then the logs) when there is more than one; click one to switch.
local ACTIONS = {
  { "", "Run", "RickRunGo", function() M.run() end },
  { "", "Debug", "RickRunDebug", function() M.debug() end },
  { "", "Stop", "RickRunStop", function() M.stop() end },
  { "", "Build", "RickRunBuild", function() require("config.build").build() end },
}
_G.RickRunAction = function(i)
  if i == 9 then return require("config.run_configs").edit() end
  local a = ACTIONS[i]
  if a then vim.schedule(a[4]) end
end

-- The Run tab's right-click entries: the row's buttons, and the configurations.
function M.menu()
  local items = {}
  for _, a in ipairs(ACTIONS) do items[#items + 1] = { a[2], a[4] } end
  items[#items + 1] = { "-" }
  items[#items + 1] = { "Edit Configurations…", function() require("config.run_configs").edit() end }
  return items
end

local function busy()
  local dbg = package.loaded["config.debug"]
  return run_job ~= nil or (dbg and dbg.active()) or require("config.build").running()
end

function M.tabs_winbar()
  local parts = {}
  for i, a in ipairs(ACTIONS) do
    local hl = (a[2] == "Stop" and not busy()) and "Comment" or a[3]
    parts[#parts + 1] = ("%%%d@v:lua.RickRunAction@%%#%s#%s %%#RickToolTab#%s %%X  "):format(i, hl, a[1], a[2])
  end
  local name = require("config.run_configs").selected_name():gsub("%%", "%%%%")
  parts[#parts + 1] = ("%%#Comment#│  %%9@v:lua.RickRunAction@%%#Comment# %%#RickToolTab#%s %%X"):format(name)
  local tabs = all_tabs()
  if #tabs >= 2 then
    parts[#parts + 1] = "  "
    for i, t in ipairs(tabs) do
      local hl = t == panel.current and "RickToolTabSel" or "RickToolTab"
      parts[#parts + 1] = ("%%%d@v:lua.RickRunTab@%%#%s# %s %%X"):format(i, hl, t.name)
    end
  end
  -- The build's progress or result while its output shows.
  if panel.current == build_tab then parts[#parts + 1] = "%#Comment#" .. _G.RickBuildStatus():gsub("%%", "%%%%") end
  return table.concat(parts, "")
end

_G.RickRunTab = function(i) show(all_tabs()[i]) end

function M.toggle_panel() require("config.toolwindow").toggle("run") end

-- A terminal tab running argv in dir (the program, or tail -F on a log).
local function new_tab(name, argv, dir, on_exit)
  local buf = vim.api.nvim_create_buf(false, true)
  vim.bo[buf].bufhidden = "hide"
  local tab = { name = name, buf = buf }
  table.insert(panel.tabs, tab)
  vim.api.nvim_buf_call(buf, function()
    tab.job = vim.fn.jobstart(argv, { term = true, cwd = dir, on_exit = function(_, code)
      vim.schedule(function() if on_exit then on_exit(code) end end)
    end })
  end)
  vim.b[buf].term_title = name
  vim.b[buf].rick_run = true
  return tab
end

local function clear_tabs()
  for _, t in ipairs(panel.tabs) do
    t.stopped = true
    if t.job then pcall(vim.fn.jobstop, t.job) end
  end
  local old = panel.tabs
  panel.tabs, panel.current, run_job = {}, nil, nil
  vim.schedule(function()
    for _, t in ipairs(old) do
      local tw = require("config.toolwindow").win()
      if vim.api.nvim_buf_is_valid(t.buf) and not (tw and vim.api.nvim_win_get_buf(tw) == t.buf) then
        pcall(vim.api.nvim_buf_delete, t.buf, { force = true })
      end
    end
  end)
end

-- Starting --------------------------------------------------------------------

local function as_argv(cmd)
  return type(cmd) == "table" and cmd or { vim.o.shell, vim.o.shellcmdflag, cmd }
end

-- Run cmd (the program) in a fresh Run panel, with the log tabs; on_exit(code)
-- when it ends.
local function start(label, cmd, on_exit)
  local dir = root()
  clear_tabs()
  -- The build went fine: its panel makes way for the program's.
  pcall(function() require("config.build").toggle_panel(false) end)
  local cmd_str = type(cmd) == "table" and table.concat(cmd, " ") or cmd
  local tab
  tab = new_tab(label, as_argv(cmd), dir, function(code)
    if tab.stopped then return end -- Stop (or a new run) ended it: no news
    run_job = nil
    vim.notify(("%s ended%s"):format(label, code == 0 and "" or (" (exit " .. code .. ")")),
      code == 0 and vim.log.levels.INFO or vim.log.levels.WARN)
    if on_exit then on_exit(code) end
  end)
  run_job = tab.job
  for _, log in ipairs(resolve(S.run_logs) or {}) do
    local path = abs(log, dir)
    new_tab(vim.fs.basename(path), { "tail", "-n", "+1", "-F", path }, dir)
  end
  show(tab)
  vim.notify(label .. ": " .. cmd_str)
end

-- Build first (unless the project says not to), then go().
local function after_build(go)
  if S.run_build_first == false then return go() end
  local build = require("config.build")
  if build.running() then return vim.notify("A build is running; Run waits for it to finish, try again then") end
  vim.api.nvim_create_autocmd("User", {
    pattern = "RickBuildDone", once = true,
    callback = function(ev)
      local code = ev.data and ev.data.code
      if code == 0 then vim.schedule(go) elseif code ~= "stopped" then
        vim.notify("Not running: the build failed (F2 goes to the first error)", vim.log.levels.WARN)
      end
    end,
  })
  build.build()
  if not build.running() then -- nothing to build here: run anyway
    vim.api.nvim_clear_autocmds({ event = "User", pattern = "RickBuildDone" })
    go()
  end
end

function M.run()
  local cmd = resolve(S.run_cmd)
  if not cmd then
    return vim.notify("Nothing to run: add a Run/Debug Configuration (Space rc), or set vim.g.run_cmd in the project's .nvim.lua (see config/run.lua)", vim.log.levels.WARN)
  end
  if require("config.debug").active() then require("config.debug").stop() end
  after_build(function() start("Run", cmd) end)
end

function M.debug()
  local program = resolve(S.debug_program)
  local target = resolve(S.debug_target)
  local remote, remote_exec = resolve(S.debug_remote), resolve(S.debug_remote_exec)
  if remote then target = nil end
  if not program and not target then
    return vim.notify("Nothing to debug: set vim.g.debug_program (and vim.g.debug_target for a gdb stub) in the project's .nvim.lua (see config/run.lua)", vim.log.levels.WARN)
  end
  local dbg = require("config.debug")
  if dbg.active() then dbg.stop() end
  after_build(function()
    local opts = { program = program, target = target, remote = remote, remote_exec = remote_exec, cwd = root() }
    local cmd = resolve(S.debug_cmd)
    -- A gdbserver on a system that is already running (Run started it): debug
    -- there, leaving it running.
    if remote and run_job then cmd = nil; opts.wait = true end
    if not cmd then
      -- The debugger's panels take the bottom; the last run's output makes way.
      pcall(function() require("config.build").toggle_panel(false) end)
      if not remote and panel_valid() then require("config.toolwindow").hide() end
      return dbg.start(opts)
    end
    start("Debug", cmd)
    -- Attach once the stub or gdbserver answers (QEMU takes a moment to start).
    opts.wait = true
    dbg.start(opts)
  end)
end

-- Stop the program, the debugger and the build.
function M.stop()
  local stopped = false
  local dbg = package.loaded["config.debug"]
  if dbg and dbg.active() then
    -- An app debugged on a running system: stop the app, keep the system.
    local remote = dbg.remote()
    dbg.stop(); stopped = true
    if remote then return end
  end
  if run_job then
    for _, t in ipairs(panel.tabs) do if t.job == run_job then t.stopped = true end end
    pcall(vim.fn.jobstop, run_job); run_job = nil; stopped = true
  end
  local build = require("config.build")
  if build.running() then build.stop(); stopped = true end
  if not stopped then vim.notify("Nothing is running") end
end

function M.running() return run_job ~= nil end

function M.setup()
  local function hl()
    vim.api.nvim_set_hl(0, "RickRunGo", { link = "DiagnosticOk", default = true })
    vim.api.nvim_set_hl(0, "RickRunDebug", { link = "DiagnosticOk", default = true })
    vim.api.nvim_set_hl(0, "RickRunStop", { link = "DiagnosticError", default = true })
    vim.api.nvim_set_hl(0, "RickRunBuild", { link = "DiagnosticOk", default = true })
  end
  hl()
  vim.api.nvim_create_autocmd("ColorScheme", { group = vim.api.nvim_create_augroup("rick_run_hl", { clear = true }), callback = hl })
  local map = vim.keymap.set
  map("n", "<S-F10>", M.run, { desc = "Run" })
  map("n", "<F22>", M.run, { desc = "Run" }) -- Shift+F10 as terminals send it
  map("n", "<leader>rr", M.run, { desc = "Run" })
  map("n", "<S-F9>", M.debug, { desc = "Debug" })
  map("n", "<F21>", M.debug, { desc = "Debug" }) -- Shift+F9 as terminals send it
  map("n", "<leader>rd", M.debug, { desc = "Debug" })
  -- Cmd+F2 / Space rs stop everything (build.lua maps them to the build alone).
  map({ "n", "t" }, "<D-F2>", M.stop, { desc = "Stop" })
  map("n", "<leader>rs", M.stop, { desc = "Stop (run, debug, build)" })
  map("n", "<leader>rl", M.toggle_panel, { desc = "Show / hide the Run panel" })
end

return M
