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
-- vim.g.build_path_map (config/build.lua) also maps the source paths in the
-- debug info, so a program built in a container opens your files.
-- With nothing set, Run says what to set.

local M = {}

local panel = { win = nil, tabs = {}, current = nil } -- tabs: { name, buf, job }
local run_job -- the job id of the program's terminal

local function root()
  return vim.g.run_dir or vim.g.build_dir or vim.fs.root(0, { ".nvim.lua", ".git" }) or vim.fn.getcwd()
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

local function panel_valid() return panel.win and vim.api.nvim_win_is_valid(panel.win) end

local function show(tab)
  if not tab or not vim.api.nvim_buf_is_valid(tab.buf) then return end
  panel.current = tab
  if not panel_valid() then
    local prev = vim.api.nvim_get_current_win()
    vim.cmd("botright split")
    panel.win = vim.api.nvim_get_current_win()
    vim.api.nvim_win_set_height(panel.win, math.max(8, math.floor(vim.o.lines * 0.3)))
    vim.wo[panel.win].winfixheight = true
    vim.api.nvim_win_set_buf(panel.win, tab.buf)
    vim.api.nvim_set_current_win(prev)
  else
    vim.api.nvim_win_set_buf(panel.win, tab.buf)
  end
  for opt, v in pairs({ number = false, relativenumber = false, signcolumn = "no", foldcolumn = "0", spell = false }) do
    vim.wo[panel.win][opt] = v
  end
  -- The tabs (Run, then the logs) as the panel's title row; click one to switch.
  local parts = {}
  for i, t in ipairs(panel.tabs) do
    local hl = t == tab and "TabLineSel" or "TabLine"
    parts[#parts + 1] = ("%%%d@v:lua.RickRunTab@%%#%s# %s %%X"):format(i, hl, t.name)
  end
  vim.wo[panel.win].winbar = table.concat(parts, "%#TabLineFill# ") .. "%#TabLineFill#%=%#Comment# Cmd+F2 stops  Space rl hides "
  -- Follow the end, as a terminal does.
  local n = vim.api.nvim_buf_line_count(tab.buf)
  pcall(vim.api.nvim_win_set_cursor, panel.win, { n, 0 })
end

_G.RickRunTab = function(i) show(panel.tabs[i]) end

function M.toggle_panel()
  if panel_valid() then
    pcall(vim.api.nvim_win_close, panel.win, true)
    panel.win = nil
  elseif panel.current then
    show(panel.current)
  else
    vim.notify("Nothing has run yet (Shift+F10 or Space rr runs)")
  end
end

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
      if vim.api.nvim_buf_is_valid(t.buf) and not (panel_valid() and vim.api.nvim_win_get_buf(panel.win) == t.buf) then
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
  for _, log in ipairs(resolve(vim.g.run_logs) or {}) do
    local path = abs(log, dir)
    new_tab(vim.fs.basename(path), { "tail", "-n", "+1", "-F", path }, dir)
  end
  show(tab)
  vim.notify(label .. ": " .. cmd_str)
end

-- Build first (unless the project says not to), then go().
local function after_build(go)
  if vim.g.run_build_first == false then return go() end
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
  local cmd = resolve(vim.g.run_cmd)
  if not cmd then
    return vim.notify("Nothing to run: set vim.g.run_cmd in the project's .nvim.lua (see config/run.lua)", vim.log.levels.WARN)
  end
  if require("config.debug").active() then require("config.debug").stop() end
  after_build(function() start("Run", cmd) end)
end

function M.debug()
  local program = resolve(vim.g.debug_program)
  local target = resolve(vim.g.debug_target)
  local remote, remote_exec = resolve(vim.g.debug_remote), resolve(vim.g.debug_remote_exec)
  if remote then target = nil end
  if not program and not target then
    return vim.notify("Nothing to debug: set vim.g.debug_program (and vim.g.debug_target for a gdb stub) in the project's .nvim.lua (see config/run.lua)", vim.log.levels.WARN)
  end
  local dbg = require("config.debug")
  if dbg.active() then dbg.stop() end
  after_build(function()
    local opts = { program = program, target = target, remote = remote, remote_exec = remote_exec, cwd = root() }
    local cmd = resolve(vim.g.debug_cmd)
    -- A gdbserver on a system that is already running (Run started it): debug
    -- there, leaving it running.
    if remote and run_job then cmd = nil; opts.wait = true end
    if not cmd then
      -- The debugger's panels take the bottom; the last run's output makes way.
      pcall(function() require("config.build").toggle_panel(false) end)
      if not remote and panel_valid() then pcall(vim.api.nvim_win_close, panel.win, true); panel.win = nil end
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
