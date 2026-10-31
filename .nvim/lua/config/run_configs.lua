-- Run/Debug Configurations, like CLion's: a dialog (Ctrl+Alt+R > Edit
-- Configurations…, or :RunConfigurations) where you keep named ways to run the
-- project, and pick the one Shift+F10 (Run) and Shift+F9 (Debug) use.
--
-- Kinds of configuration:
--   Syllable QEMU   build, deploy (rebuild the disk image), boot QEMU; debug
--                   an app through gdbserver or the kernel through QEMU's stub
--   Remote host     build, copy files to a machine over SSH (rsync), run the
--                   program there; debug it through gdbserver
--   Custom          any commands
-- "Project (.nvim.lua)" is always there: it uses what the project's .nvim.lua
-- sets (vim.g.run_cmd and friends, config/run.lua), and is the default.
-- A configuration's empty fields fall back to the .nvim.lua's values too.
--
-- They're saved in .nvim-run.json at the project root, so they can be
-- committed and shared. The chosen one is remembered there as well.
--
-- In the dialog: click a configuration on the left, or a field on the right
-- to change it (Enter does the same on the line under the cursor); + Add,
-- - Delete and Copy are above the list. Run / Debug / OK / Cancel along the bottom. Esc cancels.

local M = {}

local PROJECT = "Project (.nvim.lua)"

local function root()
  return vim.fs.root(0, { ".nvim.lua", ".git" }) or vim.fn.getcwd()
end
local function file() return root() .. "/.nvim-run.json" end

-- Kinds ------------------------------------------------------------------------

local function g(name, fallback)
  local v = vim.g[name]
  if type(v) == "function" then return fallback end
  if type(v) == "table" then return table.concat(v, ", ") end
  return v ~= nil and tostring(v) or fallback
end

local function join(...)
  local out = {}
  for _, c in ipairs({ ... }) do if c and c ~= "" then out[#out + 1] = c end end
  return #out > 0 and table.concat(out, " && ") or nil
end

local function list(s)
  local out = {}
  for item in (s or ""):gmatch("[^,]+") do
    item = vim.trim(item)
    if item ~= "" then out[#out + 1] = item end
  end
  return #out > 0 and out or nil
end

local function nonempty(s) return s ~= nil and s ~= "" and s or nil end

local DEBUG_WHAT = { "App (gdbserver)", "Kernel (QEMU gdb stub)", "As the .nvim.lua decides (by file)" }

M.kinds = {
  {
    id = "qemu", title = "Syllable QEMU",
    fields = {
      { "Before launch" },
      { key = "build_first", label = "Build first", kind = "bool", default = true },
      { key = "build", label = "Build", default = function() return g("build_cmd", "./sb build") end },
      { key = "deploy", label = "Deploy", default = "./sb image --debug", help = "Puts your build on the disk image" },
      { "Launch" },
      { key = "dir", label = "Directory", default = function() return g("run_dir", ".") end },
      { key = "qemu", label = "Start QEMU", default = "./sb run --gdbserver" },
      { key = "serial", label = "Serial log", default = "out/run-serial.log", help = "Shown as a tab next to the output" },
      { "Debug" },
      { key = "debug_what", label = "Debug", kind = "choice", choices = DEBUG_WHAT, default = DEBUG_WHAT[1] },
      { key = "gdbserver", label = "gdbserver at", default = "localhost:2345" },
      { key = "app_path", label = "App on Syllable", default = "", help = "e.g. /boot/Applications/Foo/Foo" },
      { key = "app_symbols", label = "App with symbols", default = "", help = "The local copy, e.g. out/sysroot/Applications/Foo/Foo" },
      { key = "qemu_debug", label = "Start QEMU (kernel)", default = "./sb run --gdb" },
      { key = "stub", label = "Kernel stub at", default = "localhost:1234" },
      { key = "kernel", label = "Kernel symbols", default = "out/kernel.so" },
      { key = "gdb", label = "gdb", default = function() return g("debug_gdb", "i386-elf-gdb") end },
      { key = "gdb_init", label = "gdb commands", default = "", help = "Comma-separated, run before attaching" },
    },
    settings = function(c)
      local s = {
        build_cmd = nonempty(c.build), run_build_first = c.build_first,
        run_dir = nonempty(c.dir), run_cmd = join(c.deploy, c.qemu), run_logs = list(c.serial),
        debug_gdb = nonempty(c.gdb), debug_gdb_init = list(c.gdb_init),
      }
      if c.debug_what == DEBUG_WHAT[2] then
        s.debug_cmd, s.debug_target, s.debug_program = join(c.deploy, c.qemu_debug), nonempty(c.stub), nonempty(c.kernel)
        s.debug_remote = false
      elseif c.debug_what == DEBUG_WHAT[1] then
        s.debug_cmd, s.debug_remote = join(c.deploy, c.qemu), nonempty(c.gdbserver)
        s.debug_remote_exec, s.debug_program = nonempty(c.app_path), nonempty(c.app_symbols)
        s.debug_target = false
      end
      return s
    end,
  },
  {
    id = "ssh", title = "Remote host (SSH)",
    fields = {
      { "Before launch" },
      { key = "build_first", label = "Build first", kind = "bool", default = true },
      { key = "build", label = "Build", default = function() return g("build_cmd", "") end },
      { key = "upload", label = "Upload", default = "", help = "Comma-separated local paths, copied with rsync -R" },
      { "Host" },
      { key = "host", label = "Host", default = "", help = "user@host (or a Host from ~/.ssh/config)" },
      { key = "port", label = "SSH port", default = "22" },
      { key = "remote_dir", label = "Remote folder", default = "", help = "Where the uploads go and the program runs" },
      { "Launch" },
      { key = "command", label = "Run", default = "", help = "Runs there, in the remote folder" },
      { "Debug" },
      { key = "gdbserver_port", label = "gdbserver port", default = "2345" },
      { key = "program", label = "Program there", default = "" },
      { key = "symbols", label = "Local copy", default = "", help = "The same program, with symbols" },
      { key = "gdb", label = "gdb", default = function() return g("debug_gdb", "gdb") end },
    },
    settings = function(c)
      local host, port, dir = nonempty(c.host), nonempty(c.port) or "22", nonempty(c.remote_dir) or "."
      if not host then return { run_build_first = c.build_first, build_cmd = nonempty(c.build) } end
      local q = vim.fn.shellescape
      local ssh = ("ssh -p %s"):format(port)
      local upload
      if list(c.upload) then
        upload = ("rsync -azR -e %s %s %s"):format(q(ssh), table.concat(vim.tbl_map(q, list(c.upload)), " "), q(host .. ":" .. dir .. "/"))
      end
      local run = nonempty(c.command) and ("%s -t %s %s"):format(ssh, q(host), q("cd " .. dir .. " && " .. c.command))
      local gport = nonempty(c.gdbserver_port) or "2345"
      local bare = host:gsub("^.*@", "")
      return {
        build_cmd = nonempty(c.build), run_build_first = c.build_first,
        run_cmd = join(upload, run),
        debug_cmd = join(upload, ("%s %s %s"):format(ssh, q(host), q("gdbserver --multi :" .. gport))),
        debug_remote = bare .. ":" .. gport, debug_remote_exec = nonempty(c.program),
        debug_program = nonempty(c.symbols), debug_gdb = nonempty(c.gdb), debug_target = false,
      }
    end,
  },
  {
    id = "custom", title = "Custom",
    fields = {
      { "Before launch" },
      { key = "build_first", label = "Build first", kind = "bool", default = true },
      { key = "build", label = "Build", default = function() return g("build_cmd", "") end },
      { "Launch" },
      { key = "dir", label = "Directory", default = function() return g("run_dir", ".") end },
      { key = "run", label = "Run", default = function() return g("run_cmd", "") end },
      { key = "logs", label = "Logs", default = "", help = "Comma-separated files shown as tabs" },
      { "Debug" },
      { key = "debug_cmd", label = "Start for debugging", default = "" },
      { key = "target", label = "Attach to", default = "", help = "A gdb stub, e.g. localhost:1234; empty starts the program in gdb" },
      { key = "program", label = "Program", default = "" },
      { key = "gdb", label = "gdb", default = function() return g("debug_gdb", "gdb") end },
    },
    settings = function(c)
      return {
        build_cmd = nonempty(c.build), run_build_first = c.build_first, run_dir = nonempty(c.dir),
        run_cmd = nonempty(c.run), run_logs = list(c.logs), debug_cmd = nonempty(c.debug_cmd),
        debug_target = nonempty(c.target), debug_program = nonempty(c.program), debug_gdb = nonempty(c.gdb),
      }
    end,
  },
}
local KIND = {}
for _, k in ipairs(M.kinds) do KIND[k.id] = k end

local function default_of(f)
  if type(f.default) == "function" then return f.default() end
  return f.default
end

function M.new_config(kind_id, name)
  local c = { name = name, kind = kind_id }
  for _, f in ipairs(KIND[kind_id].fields) do
    if f.key then c[f.key] = default_of(f) end
  end
  return c
end

-- Saved state ------------------------------------------------------------------

local data -- { selected = name, configurations = { ... } } for root()
local data_root

local function load()
  local r = root()
  if data and data_root == r then return data end
  data_root, data = r, { selected = PROJECT, configurations = {} }
  local ok, text = pcall(vim.fn.readfile, file())
  if ok and #text > 0 then
    local ok2, d = pcall(vim.json.decode, table.concat(text, "\n"))
    if ok2 and type(d) == "table" then
      data.selected = d.selected or PROJECT
      data.configurations = type(d.configurations) == "table" and d.configurations or {}
    end
  end
  return data
end

-- A readable file: one key per line.
local function encode(d)
  local lines = { "{", ('  "selected": %s,'):format(vim.json.encode(d.selected)), '  "configurations": [' }
  for i, c in ipairs(d.configurations) do
    local keys = vim.tbl_keys(c)
    table.sort(keys, function(a, b)
      local rank = { name = 1, kind = 2 }
      if (rank[a] or 9) ~= (rank[b] or 9) then return (rank[a] or 9) < (rank[b] or 9) end
      return a < b
    end)
    lines[#lines + 1] = "    {"
    for j, k in ipairs(keys) do
      lines[#lines + 1] = ("      %s: %s%s"):format(vim.json.encode(k), vim.json.encode(c[k]), j < #keys and "," or "")
    end
    lines[#lines + 1] = "    }" .. (i < #d.configurations and "," or "")
  end
  lines[#lines + 1] = "  ]"
  lines[#lines + 1] = "}"
  return lines
end

local function save(d)
  data, data_root = d, root()
  if #d.configurations == 0 and d.selected == PROJECT and vim.fn.filereadable(file()) == 0 then return end
  vim.fn.writefile(encode(d), file())
end

function M.selected()
  local d = load()
  for _, c in ipairs(d.configurations) do
    if c.name == d.selected then return c end
  end
end

function M.selected_name()
  local c = M.selected()
  return c and c.name or PROJECT
end

-- What config/run.lua, debug.lua and build.lua read instead of vim.g.<name>:
-- the chosen configuration's value, else the .nvim.lua's. false in a
-- configuration means "not this" (an App debug config has no gdb stub).
M.settings = setmetatable({}, {
  __index = function(_, name)
    local c = M.selected()
    if c and KIND[c.kind] then
      local v = KIND[c.kind].settings(c)[name]
      if v == false then return nil end
      -- A relative folder is from the project root.
      if name == "run_dir" and type(v) == "string" and not v:match("^[/~]") then v = vim.fs.normalize(root() .. "/" .. v) end
      if v ~= nil then return v end
    end
    return vim.g[name]
  end,
})

function M.select(name)
  local d = load()
  d.selected = name
  save(d)
  vim.notify("Run configuration: " .. name)
end

-- The Run… / Debug… popup (Ctrl+Alt+R / Ctrl+Alt+D): pick a configuration and go.
function M.popup(debug)
  local d = load()
  local items = { { name = PROJECT } }
  for _, c in ipairs(d.configurations) do items[#items + 1] = { name = c.name, kind = KIND[c.kind] and KIND[c.kind].title } end
  items[#items + 1] = { name = "Edit Configurations…", edit = true }
  vim.ui.select(items, {
    prompt = debug and "Debug" or "Run",
    format_item = function(it)
      return (it.name == d.selected and "● " or "  ") .. it.name .. (it.kind and ("   " .. it.kind) or "")
    end,
  }, function(it)
    if not it then return end
    if it.edit then return M.edit() end
    M.select(it.name)
    local run = require("config.run")
    if debug then run.debug() else run.run() end
  end)
end

-- The dialog ---------------------------------------------------------------------

local ns = vim.api.nvim_create_namespace("rick_run_configs")
local LEFT = 26 -- width of the list column

function M.edit()
  local d = vim.deepcopy(load())
  local cur = d.selected -- the configuration shown on the right
  local rows = {} -- line (1-based) -> { list = name } | { field = f } | { kind = true } | { name = true }
  local buttons = {} -- { col_from, col_to, fn } on the buttons line
  local width = math.min(110, vim.o.columns - 4)
  local height = math.min(32, vim.o.lines - 4)
  local buf = vim.api.nvim_create_buf(false, true)
  vim.bo[buf].bufhidden = "wipe"
  vim.bo[buf].filetype = "rick_run_configs"
  local win = vim.api.nvim_open_win(buf, true, {
    relative = "editor", width = width, height = height, row = math.floor((vim.o.lines - height) / 2) - 1,
    col = math.floor((vim.o.columns - width) / 2), border = "rounded", style = "minimal",
    title = " Run/Debug Configurations ", title_pos = "center", zindex = 80,
  })
  vim.wo[win].cursorline = true
  vim.wo[win].wrap = false

  local function find(name)
    for i, c in ipairs(d.configurations) do if c.name == name then return c, i end end
  end

  local function pad(s, n)
    local w = vim.fn.strdisplaywidth(s)
    if w > n then return vim.fn.strcharpart(s, 0, n - 1) .. "…" end
    return s .. string.rep(" ", n - w)
  end

  local function render()
    rows, buttons = {}, {}
    local right = {} -- { text, hl, row = {...} }
    local c = find(cur)
    if not c then
      cur = PROJECT
      right[#right + 1] = { "Uses what the project's .nvim.lua sets:", "Comment" }
      right[#right + 1] = { "" }
      for _, k in ipairs({ "build_cmd", "run_dir", "run_cmd", "run_logs", "debug_cmd", "debug_target", "debug_remote",
        "debug_remote_exec", "debug_program", "debug_gdb" }) do
        local v = vim.g[k]
        local shown = type(v) == "function" and "(decided per file)" or (type(v) == "table" and table.concat(v, ", ") or (v and tostring(v) or "—"))
        right[#right + 1] = { pad(k, 18) .. shown, v == nil and "Comment" or nil }
      end
      right[#right + 1] = { "" }
      right[#right + 1] = { "Click + to add a configuration of your own.", "Comment" }
    else
      local kind = KIND[c.kind]
      right[#right + 1] = { pad("Name", 24) .. c.name, nil, { name = true } }
      right[#right + 1] = { pad("Kind", 24) .. kind.title .. "  ▾", nil, { kind = true } }
      for _, f in ipairs(kind.fields) do
        if not f.key then
          right[#right + 1] = { "" }
          right[#right + 1] = { f[1], "Title" }
        else
          local v = c[f.key]
          local shown
          if f.kind == "bool" then shown = v and "☑" or "☐"
          elseif f.kind == "choice" then shown = (v or "") .. "  ▾"
          else shown = (v == nil or v == "") and (f.help and ("(" .. f.help .. ")") or "—") or tostring(v) end
          right[#right + 1] = { "  " .. pad(f.label, 22) .. shown, (v == nil or v == "") and f.kind == nil and "Comment" or nil, { field = f } }
        end
      end
    end
    local left = { { " + Add   - Delete   Copy", "Special" }, { "" }, { PROJECT, nil, { list = PROJECT } } }
    for _, cc in ipairs(d.configurations) do left[#left + 1] = { cc.name, nil, { list = cc.name } } end
    local lines, hls = {}, {}
    local body = height - 2
    for i = 1, body do
      local l, r = left[i], right[i]
      local ltext = l and l[1] or ""
      if l and l[3] then ltext = (l[3].list == d.selected and " ● " or "   ") .. ltext end
      local line = pad(ltext, LEFT) .. "│ " .. (r and r[1] or "")
      lines[i] = line
      if l and l[2] then hls[#hls + 1] = { i - 1, 0, #pad(ltext, LEFT), l[2] } end
      if l and l[3] and l[3].list == cur then hls[#hls + 1] = { i - 1, 0, #pad(ltext, LEFT), "Visual" } end
      hls[#hls + 1] = { i - 1, #pad(ltext, LEFT), #pad(ltext, LEFT) + #"│", "FloatBorder" }
      if r and r[2] then hls[#hls + 1] = { i - 1, #pad(ltext, LEFT) + #"│ ", #line, r[2] } end
      rows[i] = { left = l and l[3], right = r and r[3] }
    end
    lines[#lines + 1] = string.rep("─", width)
    hls[#hls + 1] = { #lines - 1, 0, #lines[#lines], "FloatBorder" }
    local bl, col = "", 0
    for _, b in ipairs({ { "Run", function() M._finish(d, "run") end }, { "Debug", function() M._finish(d, "debug") end },
      { "OK", function() M._finish(d, "ok") end }, { "Cancel", function() M._finish(nil) end } }) do
      local label = "  " .. b[1] .. "  "
      buttons[#buttons + 1] = { col + 1, col + #label, b[2] }
      bl = bl .. label .. " "
      hls[#hls + 1] = { #lines, col, col + #label, b[1] == "OK" and "PmenuSel" or "Pmenu" }
      col = col + #label + 1
    end
    lines[#lines + 1] = bl
    vim.bo[buf].modifiable = true
    vim.api.nvim_buf_set_lines(buf, 0, -1, false, lines)
    vim.bo[buf].modifiable = false
    vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
    for _, h in ipairs(hls) do
      pcall(vim.api.nvim_buf_set_extmark, buf, ns, h[1], h[2], { end_col = math.min(h[3], #lines[h[1] + 1]), hl_group = h[4] })
    end
  end

  local function unique(base)
    local name, n = base, 2
    while find(name) or name == PROJECT do name, n = base .. " " .. n, n + 1 end
    return name
  end

  local function edit_field(c, f)
    if f.kind == "bool" then
      c[f.key] = not c[f.key]
      return render()
    end
    if f.kind == "choice" then
      return vim.ui.select(f.choices, { prompt = f.label }, function(v)
        if v then c[f.key] = v end
        render()
      end)
    end
    Snacks.input({ prompt = f.label, default = c[f.key] or "" }, function(v)
      if v then c[f.key] = v end
      render()
    end)
  end

  local actions = {}
  function actions.add()
    local titles = vim.tbl_map(function(k) return k.title end, M.kinds)
    vim.ui.select(titles, { prompt = "Add configuration" }, function(_, i)
      if not i then return end
      local k = M.kinds[i]
      local c = M.new_config(k.id, unique(k.title))
      table.insert(d.configurations, c)
      cur, d.selected = c.name, c.name
      render()
    end)
  end
  function actions.delete()
    local _, i = find(cur)
    if not i then return end
    table.remove(d.configurations, i)
    if d.selected == cur then d.selected = PROJECT end
    cur = (d.configurations[i] or d.configurations[i - 1] or { name = PROJECT }).name
    render()
  end
  function actions.copy()
    local c = find(cur)
    if not c then return end
    local copy = vim.deepcopy(c)
    copy.name = unique(c.name)
    table.insert(d.configurations, copy)
    cur, d.selected = copy.name, copy.name
    render()
  end

  local function act(row, col)
    local r = rows[row]
    if row == 1 and col <= LEFT then
      if col <= 7 then return actions.add() elseif col <= 17 then return actions.delete() else return actions.copy() end
    end
    if row == height then
      for _, b in ipairs(buttons) do if col >= b[1] and col <= b[2] then return b[3]() end end
      return
    end
    if not r then return end
    if col <= LEFT then
      if r.left then
        cur = r.left.list
        d.selected = cur
        render()
      end
      return
    end
    local c = find(cur)
    local rr = r.right
    if not (c and rr) then return end
    if rr.name then
      Snacks.input({ prompt = "Name", default = c.name }, function(v)
        if v and v ~= "" and v ~= c.name then
          v = unique(v)
          if d.selected == c.name then d.selected = v end
          c.name, cur = v, v
        end
        render()
      end)
    elseif rr.kind then
      local titles = vim.tbl_map(function(k) return k.title end, M.kinds)
      vim.ui.select(titles, { prompt = "Kind" }, function(_, i)
        if i and M.kinds[i].id ~= c.kind then
          local fresh = M.new_config(M.kinds[i].id, c.name)
          for k, v in pairs(c) do c[k] = nil end
          for k, v in pairs(fresh) do c[k] = v end
        end
        render()
      end)
    elseif rr.field then
      edit_field(c, rr.field)
    end
  end

  M._finish = function(result, how)
    pcall(vim.api.nvim_win_close, win, true)
    if not result then return end
    save(result)
    local run = require("config.run")
    if how == "run" then run.run() elseif how == "debug" then run.debug() end
  end

  local function k(lhs, fn) vim.keymap.set("n", lhs, fn, { buffer = buf, nowait = true }) end
  k("<Esc>", function() M._finish(nil) end)
  k("q", function() M._finish(nil) end)
  k("<CR>", function()
    local pos = vim.api.nvim_win_get_cursor(win)
    act(pos[1], pos[2] + 1)
  end)
  k("<LeftMouse>", function()
    local m = vim.fn.getmousepos()
    if m.winid ~= win then return M._finish(nil) end
    pcall(vim.api.nvim_win_set_cursor, win, { m.line, math.max(0, m.column - 1) })
    act(m.line, vim.fn.virtcol2col(win, m.line, m.column))
  end)
  k("<2-LeftMouse>", function() end)
  k("+", actions.add)
  k("a", actions.add)
  k("-", actions.delete)
  k("dd", actions.delete)
  render()
  vim.api.nvim_win_set_cursor(win, { 3, 3 })
end

function M.setup()
  vim.api.nvim_create_user_command("RunConfigurations", M.edit, { desc = "Edit Run/Debug Configurations" })
  local map = vim.keymap.set
  map("n", "<C-M-r>", function() M.popup(false) end, { desc = "Run… (pick a configuration)" })
  map("n", "<C-M-d>", function() M.popup(true) end, { desc = "Debug… (pick a configuration)" })
  map("n", "<leader>rc", M.edit, { desc = "Edit Run/Debug Configurations" })
end

return M
