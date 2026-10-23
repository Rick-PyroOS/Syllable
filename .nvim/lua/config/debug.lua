-- The debugger, like CLion's: nvim-dap talking to gdb (gdb 14 and newer speak
-- the Debug Adapter Protocol themselves: gdb -i dap), with nvim-dap-view as
-- the Debug window (Threads, Variables, Watches, Breakpoints, Console).
-- Run.lua starts a session (Shift+F9 / Space rd); see its header for the
-- per-project settings (what to start, which stub to attach to, which gdb).
--
-- Breakpoints: click a line number, or Cmd+F8 / Space rp on the line; a red
-- number marks one. Space rP asks for a condition. Cmd+Shift+F8 lists them.
-- While a session runs:
--   F8 step over   F7 step into   Shift+F8 step out   F9 resume
--   Alt+F8 evaluate the name under the cursor (or the selection)
--   Cmd+F2 stop   the current line is lit, its number shows ▶
-- Breakpoints are kept per project (stdpath("state")/breakpoints/) so they
-- come back after a restart.

local M = {}

local function dap() return require("dap") end

function M.active() return package.loaded["dap"] ~= nil and dap().session() ~= nil end
-- Debugging a program started through a gdbserver (debug_remote)?
function M.remote() return M.active() and dap().session().config.rick_remote == true end

---------------------------------------------------------------------------
-- Breakpoints kept across restarts

local function store_file()
  local root = vim.fs.root(0, { ".nvim.lua", ".git" }) or vim.fn.getcwd()
  return vim.fn.stdpath("state") .. "/breakpoints/" .. root:gsub("[/\\:]", "%%") .. ".json"
end

local function save_breakpoints()
  if not package.loaded["dap"] then return end
  local out = {}
  for buf, list in pairs(require("dap.breakpoints").get()) do
    local name = vim.api.nvim_buf_is_valid(buf) and vim.api.nvim_buf_get_name(buf) or ""
    if name ~= "" then
      out[name] = vim.tbl_map(function(b) return { line = b.line, condition = b.condition, logMessage = b.logMessage } end, list)
    end
  end
  local f = store_file()
  pcall(vim.fn.mkdir, vim.fs.dirname(f), "p")
  pcall(vim.fn.writefile, { vim.json.encode(out) }, f)
end

local loaded = {} -- files whose saved breakpoints are set already
local function restore_breakpoints(buf)
  local name = vim.api.nvim_buf_get_name(buf)
  if name == "" or loaded[name] or vim.bo[buf].buftype ~= "" then return end
  loaded[name] = true
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(store_file()), "\n")) end)
  if not ok or type(data) ~= "table" or not data[name] then return end
  local bps = require("dap.breakpoints")
  for _, b in ipairs(data[name]) do
    pcall(bps.set, { condition = b.condition, log_message = b.logMessage }, buf, b.line)
  end
end

function M.toggle_breakpoint(buf, line)
  if not buf or buf == 0 then buf = vim.api.nvim_get_current_buf() end
  line = line or vim.api.nvim_win_get_cursor(0)[1]
  restore_breakpoints(buf)
  local bps = require("dap.breakpoints")
  local here = vim.tbl_filter(function(b) return b.line == line end, bps.get(buf)[buf] or {})
  if #here > 0 then bps.remove(buf, line) else bps.set({}, buf, line) end
  -- Tell a running session about the change.
  local s = dap().session()
  if s then s:set_breakpoints({ [buf] = bps.get(buf)[buf] or {} }) end
  save_breakpoints()
end

function M.conditional_breakpoint()
  vim.ui.input({ prompt = "Break when: " }, function(cond)
    if not cond or cond == "" then return end
    local buf, line = vim.api.nvim_get_current_buf(), vim.api.nvim_win_get_cursor(0)[1]
    restore_breakpoints(buf)
    require("dap.breakpoints").remove(buf, line)
    require("dap.breakpoints").set({ condition = cond }, buf, line)
    local s = dap().session()
    if s then s:set_breakpoints({ [buf] = require("dap.breakpoints").get(buf)[buf] or {} }) end
    save_breakpoints()
  end)
end

---------------------------------------------------------------------------
-- Sessions

-- gdb's arguments: DAP mode, then the project's path map (build.lua's
-- build_path_map) as substitute-path, then its own init commands.
-- With opts.remote, gdb then connects to that gdbserver (--multi) and runs
-- opts.remote_exec on the other system.
local function gdb_args(opts)
  local args = { "-i", "dap", "-q" }
  for from, to in pairs(vim.g.build_path_map or {}) do
    vim.list_extend(args, { "-ex", ("set substitute-path %s %s"):format(from:gsub("/$", ""), to:gsub("/$", "")) })
  end
  for _, c in ipairs(vim.g.debug_gdb_init or {}) do vim.list_extend(args, { "-ex", c }) end
  if opts.remote then
    -- gdb retries the connection while gdbserver starts (a test connect
    -- would end a gdbserver --multi that has no program yet).
    -- debug_connect_timeout: seconds; building and booting the system first
    -- can take minutes.
    vim.list_extend(args, { "-ex", "set tcp connect-timeout " .. (vim.g.debug_connect_timeout or 300),
      "-ex", "target extended-remote " .. opts.remote })
    if opts.remote_exec then vim.list_extend(args, { "-ex", "set remote exec-file " .. opts.remote_exec }) end
  end
  return args
end

-- Wait until host:port takes a connection (QEMU starting), then cb(ok).
local function wait_port(target, cb, tries)
  tries = tries or 0
  local host, port = target:match("^(.-):(%d+)$")
  if not host then return cb(true) end
  if host == "" or host == "localhost" then host = "127.0.0.1" end
  local tcp = vim.uv.new_tcp()
  tcp:connect(host, tonumber(port), function(err)
    tcp:close()
    vim.schedule(function()
      if not err then return cb(true) end
      if tries >= 60 then return cb(false) end
      vim.defer_fn(function() wait_port(target, cb, tries + 1) end, 500)
    end)
  end)
end

-- gdb's major version (0 if unknown), asked once per gdb.
local gdb_majors = {}
local function gdb_major(gdb)
  if gdb_majors[gdb] == nil then
    gdb_majors[gdb] = tonumber(vim.fn.system({ gdb, "--version" }):match("(%d+)%.%d+")) or 0
  end
  return gdb_majors[gdb]
end

-- opts: program (the file with the symbols), cwd, and either target
-- (host:port of a gdb stub to attach to) or remote (host:port of a
-- gdbserver --multi to start remote_exec on); wait: the stub is still
-- starting.
function M.start(opts)
  local gdb = vim.g.debug_gdb or "gdb"
  if vim.fn.executable(gdb) == 0 then
    return vim.notify(("Debug: %s isn't installed (brew install %s, or set vim.g.debug_gdb)"):format(gdb,
      gdb == "gdb" and "gdb" or gdb), vim.log.levels.ERROR)
  end
  dap().adapters.gdb = { type = "executable", command = gdb, args = gdb_args(opts) }
  local config = opts.target and {
    type = "gdb", request = "attach", name = "Attach to " .. opts.target,
    target = opts.target, program = opts.program, cwd = opts.cwd,
  } or {
    type = "gdb", request = "launch", name = "Debug " .. vim.fs.basename(opts.program),
    program = opts.program, cwd = opts.cwd, args = vim.g.debug_args or {},
    rick_remote = opts.remote and true or nil,
  }
  -- gdb before 16 starts the program at launch, before the breakpoints are
  -- set (and refuses them as "notStopped" once it runs): have it stop at
  -- main, and carry on from there once they are in (the event_stopped
  -- listener below). Attaching stops the program where it is (QEMU: at the
  -- reset vector); that first stop is carried on from too, as CLion does.
  local major = gdb_major(gdb)
  if opts.remote and major > 0 and major < 16 then
    -- Old gdb starts a remote program at launch, before the breakpoints
    -- can be set, and a remote one doesn't wait at main.
    return vim.notify(("Debug: debugging through gdbserver needs gdb 16 or newer (%s is %d)"):format(gdb, major),
      vim.log.levels.ERROR)
  end
  if config.request == "launch" and major > 0 and major < 16 then
    config.stopAtBeginningOfMainSubprogram = true
    config.rick_skip_first = true
    config.rick_old_gdb = true
  elseif config.request == "attach" then
    config.rick_skip_first = true
  end
  -- Every open file's saved breakpoints go to the session.
  for _, buf in ipairs(vim.api.nvim_list_bufs()) do
    if vim.api.nvim_buf_is_loaded(buf) then restore_breakpoints(buf) end
  end
  local function go() dap().run(config) end
  if opts.target and opts.wait then
    vim.notify("Debug: waiting for " .. opts.target)
    wait_port(opts.target, function(ok)
      if ok then go() else vim.notify("Debug: nothing answered on " .. opts.target, vim.log.levels.ERROR) end
    end)
  else
    go()
  end
end

function M.stop()
  if not M.active() then return end
  local s = dap().session()
  if s and s.config.request == "attach" then dap().disconnect({ terminateDebuggee = false }) else dap().terminate() end
  dap().close()
  -- close() drops the session before gdb says it ended, so the end-of-session
  -- listeners never run: put the Debug window and the values away here.
  pcall(function() require("dap-view").close() end)
  pcall(function() require("dap-view.virtual-text").clear_virtual_text() end)
end

---------------------------------------------------------------------------
-- Keys and the margin

-- Line numbers are the last cells of the margin (after the fold column and
-- the E / S / M signs); a click there toggles a breakpoint.
local function on_number(m)
  if m.winid == 0 or m.line == 0 or not vim.api.nvim_win_is_valid(m.winid) then return false end
  local buf = vim.api.nvim_win_get_buf(m.winid)
  if vim.bo[buf].buftype ~= "" or vim.api.nvim_win_get_config(m.winid).relative ~= "" then return false end
  if not (vim.wo[m.winid].number or vim.wo[m.winid].relativenumber) then return false end
  local info = vim.fn.getwininfo(m.winid)[1]
  local lines = vim.api.nvim_buf_line_count(buf)
  local numbers = math.max(vim.wo[m.winid].numberwidth, #tostring(lines) + 1)
  return m.wincol > info.textoff - numbers and m.wincol <= info.textoff
end

local function in_session(fn, fallback)
  return function()
    if M.active() then return fn() end
    if fallback then return fallback() end
    vim.notify("No debug session (Shift+F9 or Space rd starts one)")
  end
end

local function evaluate()
  local mode = vim.fn.mode()
  local expr
  if mode == "v" or mode == "V" then
    expr = table.concat(vim.fn.getregion(vim.fn.getpos("v"), vim.fn.getpos("."), { type = mode }), "\n")
    vim.api.nvim_feedkeys(vim.keycode("<Esc>"), "nx", false)
  end
  if expr then
    require("dap.ui.widgets").hover(expr, { border = "rounded" })
  else
    require("dap.ui.widgets").hover(nil, { border = "rounded" })
  end
end

local function highlights()
  vim.api.nvim_set_hl(0, "RickBreakpoint", { fg = "#ffffff", bg = "#c53b53", bold = true })
  vim.api.nvim_set_hl(0, "RickBreakpointCond", { fg = "#ffffff", bg = "#ff9e64", bold = true })
  vim.api.nvim_set_hl(0, "RickBreakpointOff", { fg = "#c53b53", underline = true })
  vim.api.nvim_set_hl(0, "RickDebugLine", { bg = "#2d3f76" })
  vim.api.nvim_set_hl(0, "RickDebugLineNr", { fg = "#ffc777", bg = "#2d3f76", bold = true })
end

function M.setup()
  highlights()
  local sign = vim.fn.sign_define
  -- No sign text: the margin's sign columns are E / S / M; the line number shows it.
  sign("DapBreakpoint", { text = "", numhl = "RickBreakpoint" })
  sign("DapBreakpointCondition", { text = "", numhl = "RickBreakpointCond" })
  sign("DapLogPoint", { text = "", numhl = "RickBreakpointCond" })
  sign("DapBreakpointRejected", { text = "", numhl = "RickBreakpointOff" })
  sign("DapStopped", { text = "", linehl = "RickDebugLine", numhl = "RickDebugLineNr" })

  local group = vim.api.nvim_create_augroup("rick_debug", { clear = true })
  vim.api.nvim_create_autocmd("ColorScheme", { group = group, callback = highlights })
  vim.api.nvim_create_autocmd("BufReadPost", {
    group = group,
    callback = function(ev) if package.loaded["dap"] then restore_breakpoints(ev.buf) end end,
  })

  -- The Debug window opens with a session and closes after it.
  local ok_view, view = pcall(require, "dap-view")
  if ok_view then
    local l = dap().listeners
    l.after.event_initialized.rick_view = function() view.open() end
    l.before.event_terminated.rick_view = function() view.close() end
    l.before.event_exited.rick_view = function() view.close() end
    l.before.disconnect.rick_view = function() view.close() end
  end
  -- gdb offers C++ / Ada exception stops but refuses to set them while the
  -- program runs ("notStopped"), which nvim-dap and dap-view both try: an
  -- error popup at every start. Not offering them keeps the start quiet.
  dap().listeners.after.initialize.rick_debug = function(session)
    if session.config.type == "gdb" and session.capabilities then session.capabilities.exceptionBreakpointFilters = nil end
    -- Before gdb 16 configurationDone is a no-op that it refuses while the
    -- program runs; skip it.
    if session.config.rick_old_gdb then session.capabilities.supportsConfigurationDoneRequest = nil end
  end
  if ok_view then require("dap-view.state").exceptions_options.gdb = {} end
  dap().listeners.after.stackTrace.rick_debug = function(session, err, res)
    if not session.rick_first_pending then return end
    session.rick_first_pending = nil
    local f = not err and res and res.stackFrames and res.stackFrames[1]
    local path = f and f.source and f.source.path
    local buf = path and vim.fn.bufnr(path)
    for _, bp in ipairs(buf and buf > 0 and require("dap.breakpoints").get(buf)[buf] or {}) do
      if bp.line == f.line then return vim.notify("Stopped at a breakpoint") end
    end
    vim.schedule(function() if session.stopped_thread_id then session:_step("continue") end end)
  end
  dap().listeners.after.event_exited.rick_debug = function(_, body)
    local code = body and body.exitCode
    vim.notify(("Debug: the program ended%s"):format(code and code ~= 0 and (" (exit " .. code .. ")") or ""),
      code and code ~= 0 and vim.log.levels.WARN or vim.log.levels.INFO)
  end
  dap().listeners.after.event_stopped.rick_debug = function(session, body)
    if session.config.rick_skip_first and not session.rick_first_done then
      -- The first stop asked for above: go on once nvim-dap has read where
      -- it is (the stackTrace listener below), unless a breakpoint of yours
      -- is there too. Going on sooner races nvim-dap's own reading of it.
      session.rick_first_done = true
      session.rick_first_pending = true
      return
    end
    if body and body.reason == "breakpoint" then vim.notify("Stopped at a breakpoint") end
  end

  local map = vim.keymap.set
  local function bp() M.toggle_breakpoint() end
  map("n", "<D-F8>", bp, { desc = "Toggle breakpoint" })
  map("n", "<leader>rp", bp, { desc = "Toggle breakpoint" })
  map("n", "<leader>rP", M.conditional_breakpoint, { desc = "Conditional breakpoint" })
  local function list() if ok_view then view.open(); pcall(view.jump_to_view, "breakpoints") else dap().list_breakpoints() end end
  map("n", "<D-S-F8>", list, { desc = "Show breakpoints" })
  map("n", "<F8>", in_session(function() dap().step_over() end), { desc = "Step over" })
  local diff_next = function() if vim.wo.diff then require("config.diff").actions.next() end end
  map("n", "<F7>", in_session(function() dap().step_into() end, diff_next), { desc = "Step into (diff: next change)" })
  map("n", "<S-F8>", in_session(function() dap().step_out() end), { desc = "Step out" })
  map("n", "<F20>", in_session(function() dap().step_out() end), { desc = "Step out" }) -- Shift+F8 as terminals send it
  map("n", "<F9>", in_session(function() dap().continue() end), { desc = "Resume" })
  map("n", "<D-M-r>", in_session(function() dap().continue() end), { desc = "Resume" })
  map({ "n", "x" }, "<M-F8>", in_session(evaluate), { desc = "Evaluate" })
  map({ "n", "x" }, "<F56>", in_session(evaluate), { desc = "Evaluate" }) -- Alt+F8 as terminals send it
  map({ "n", "x" }, "<leader>re", in_session(evaluate), { desc = "Evaluate" })

  -- A click on a line number toggles a breakpoint there.
  local press, release = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
  local ate = false
  vim.on_key(function(k, typed)
    k = typed ~= "" and typed or k
    if k == release and ate then
      ate = false
      return ""
    end
    if k ~= press then return end
    local m = vim.fn.getmousepos()
    if not on_number(m) then return end
    ate = true
    local buf, line = vim.api.nvim_win_get_buf(m.winid), m.line
    vim.schedule(function() M.toggle_breakpoint(buf, line) end)
    return ""
  end, vim.api.nvim_create_namespace("rick_debug_click"))
end

return M
