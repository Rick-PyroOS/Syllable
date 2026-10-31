-- Local-model checks: a model running on this Mac (Ollama by default) reads
-- each function of a C or C++ file in the background, when the file opens and
-- after it's saved, and what it finds shows as an M in the margin, next to E
-- (clangd) and S (cppcheck/semgrep). Rest the mouse on the M for the message.
--
-- The checks are prompt files in mcp/checks/*.md (one check per file; add a
-- file to add a check; a "languages: cpp" line keeps one to those filetypes;
-- an "opt-in: yes" check runs only in a project whose .nvim.lua lists it, as
-- vim.g.mcp_checks = { "syllable-style" }).
-- mcp/server.py is an MCP server that runs them; this file starts it and talks
-- to it over stdin/stdout. Only functions that changed since the last run are
-- sent again.
--
-- Setup: ./install.sh --local-model (or brew install ollama, then
-- ollama serve  and  ollama pull qwen2.5-coder:7b). The installer saves the model
-- it pulled in state/mcp.json. Another model or endpoint (LM Studio:
-- http://localhost:1234/v1), which wins over that:
--   vim.g.rick_mcp_model = "..."   vim.g.rick_mcp_url = "..."   (before setup)
-- Space tm turns the checks on and off (remembered); Space cm runs them now.
-- With Ollama (the default endpoint), the checks start it when it isn't
-- running: the Ollama app on a Mac, else "ollama serve" in the background,
-- left running after Neovim quits. Settings (Cmd+,) > "Start Ollama when
-- model checks need it" turns that off. Turning the checks off (Space tm or
-- Settings) stops the model: the checks server quits, the model is unloaded
-- and an "ollama serve" started here is stopped.
--
-- Silence a finding you've checked with a comment, like semgrep's nosemgrep:
--   foo( new Bar );   // nomodel                 every check, this line
--   // nomodel: c-bugs                           on its own line: the next line,
--   foo( new Bar );                              only the c-bugs check
-- Add a note after "--" to keep the finding but mark it checked: the robot
-- turns green and the popup shows the note under the message.
--   foo( new Bar );   // nomodel: c-bugs -- Bar is owned by foo
-- Several lines: "// nomodel-begin" (with the same names and note) on a line
-- before them and "// nomodel-end" after; with no end it runs to the file's end.

local M = {}

M.ns = vim.api.nvim_create_namespace("rick_mcp") -- scanners.lua draws the M from this
local sev = vim.diagnostic.severity
local SEVERITY = { error = sev.ERROR, warning = sev.WARN, info = sev.INFO }
local SERVER = vim.fn.stdpath("config") .. "/mcp/server.py"
local state_file = vim.fn.stdpath("state") .. "/mcp.json"
local filetypes = { c = true, cpp = true }
local DEBOUNCE = 2000 -- after a save, so auto-save's saves don't each start a run
local MAX_LINES = 300 -- longer functions go in pieces

local function read_state()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(state_file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end

M.enabled = read_state().enabled ~= false

local warned = false
local noted = false -- the server's note (another model in use) is shown once
local function warn(msg)
  if warned then return end
  warned = true
  vim.notify("Local model checks: " .. msg .. "\n(Space tm turns them off)", vim.log.levels.WARN)
end

---------------------------------------------------------------------------
-- The MCP server

local client = { proc = nil, ready = false, id = 0, pending = {}, waiting = {}, partial = "" }

local function write(msg)
  if client.proc then pcall(function() client.proc:write(vim.json.encode(msg) .. "\n") end) end
end

local function request(method, params, cb)
  client.id = client.id + 1
  client.pending[client.id] = cb
  write({ jsonrpc = "2.0", id = client.id, method = method, params = params })
  return client.id
end

local function cancel(id)
  if not client.pending[id] then return end
  client.pending[id] = nil
  write({ jsonrpc = "2.0", method = "notifications/cancelled", params = { requestId = id } })
end

local function on_line(line)
  local ok, msg = pcall(vim.json.decode, line, { luanil = { object = true, array = true } })
  if not ok or type(msg) ~= "table" or not msg.id then return end
  local cb = client.pending[msg.id]
  client.pending[msg.id] = nil
  if cb then cb(msg) end
end

local function on_stdout(_, data)
  if not data then return end
  vim.schedule(function()
    client.partial = client.partial .. data
    while true do
      local nl = client.partial:find("\n", 1, true)
      if not nl then break end
      local line = client.partial:sub(1, nl - 1)
      client.partial = client.partial:sub(nl + 1)
      on_line(line)
    end
  end)
end

-- Run cb once the server is up (starting it the first time).
-- Ollama answering on its default port, or started (once a session) when it
-- isn't; then done(). Another endpoint (vim.g.rick_mcp_url) is left alone.
local OLLAMA = "http://127.0.0.1:11434"
local ollama_tried = false
local ollama_pid -- an "ollama serve" this Neovim started (stopped again when the checks are turned off)
local function ensure_ollama(done)
  local url = vim.g.rick_mcp_url or "http://localhost:11434/v1"
  if ollama_tried or not url:find(":11434") or not require("config.prefs").get("ollama_autostart")
    or vim.fn.executable("curl") == 0 then
    return done()
  end
  ollama_tried = true
  local function up(cb)
    vim.system({ "curl", "-s", "--noproxy", "*", "-m", "1", "-o", "/dev/null", "-w", "%{http_code}", OLLAMA .. "/api/version" },
      { text = true }, vim.schedule_wrap(function(r) cb(r.code == 0 and r.stdout == "200") end))
  end
  up(function(ok)
    if ok then return done() end
    local app = vim.fn.has("mac") == 1 and vim.fn.isdirectory("/Applications/Ollama.app") == 1
    if app then
      vim.system({ "open", "-g", "-a", "Ollama" })
    elseif vim.fn.executable("ollama") == 1 then
      local ok, proc = pcall(vim.system, { "ollama", "serve" }, { detach = true, stdout = false, stderr = false })
      if ok and proc then ollama_pid = proc.pid end
    else
      vim.notify("Local model checks: Ollama isn't installed (brew install ollama, then ollama pull qwen2.5-coder:7b)",
        vim.log.levels.WARN)
      return done()
    end
    vim.notify("Local model checks: starting Ollama")
    local tries = 0
    local function wait()
      up(function(ready)
        tries = tries + 1
        if ready or tries >= 30 then return done() end -- the server says if it's still not up
        vim.defer_fn(wait, 500)
      end)
    end
    vim.defer_fn(wait, 500)
  end)
end

local launch

local function with_server(cb)
  if client.ready then return cb() end
  table.insert(client.waiting, cb)
  if client.proc or client.starting then return end
  client.starting = true
  ensure_ollama(function()
    client.starting = false
    launch()
  end)
end

launch = function()
  if client.proc then return end
  if vim.fn.executable("python3") == 0 then return warn("python3 isn't installed") end
  local env = {}
  if vim.g.rick_mcp_url then env.RICK_MCP_URL = vim.g.rick_mcp_url end
  local model = vim.g.rick_mcp_model or read_state().model
  if type(model) == "string" and model ~= "" then env.RICK_MCP_MODEL = model end
  local ok, proc = pcall(vim.system, { "python3", SERVER }, {
    stdin = true, stdout = on_stdout, stderr = function() end, env = env,
  }, vim.schedule_wrap(function()
    client.proc, client.ready, client.partial = nil, false, ""
    local pending = client.pending
    client.pending = {}
    for _, cb in pairs(pending) do cb({ error = { message = "the checks server stopped" } }) end
  end))
  if not ok then return warn("couldn't start " .. SERVER) end
  client.proc = proc
  request("initialize", {
    protocolVersion = "2025-06-18", capabilities = {}, clientInfo = { name = "neovim", version = "1" },
  }, function(msg)
    if not msg.result then return warn("the checks server didn't start") end
    write({ jsonrpc = "2.0", method = "notifications/initialized" })
    client.ready = true
    local waiting = client.waiting
    client.waiting = {}
    for _, fn in ipairs(waiting) do fn() end
  end)
end

local function tool(name, args, cb)
  return request("tools/call", { name = name, arguments = args }, function(msg)
    local res = msg.result
    if not res then return cb(nil, msg.error and msg.error.message or "no answer") end
    local text = res.content and res.content[1] and res.content[1].text or ""
    if res.isError then return cb(nil, text) end
    local data = res.structuredContent
    if not data then
      local ok, decoded = pcall(vim.json.decode, text)
      data = ok and decoded or nil
    end
    cb(data, data == nil and "unreadable answer" or nil)
  end)
end

---------------------------------------------------------------------------
-- What gets checked: each function (or class, or other top-level definition)
-- on its own, found with treesitter. Namespaces, extern "C" blocks and #if
-- blocks are looked inside.

local CONTAINERS = {
  translation_unit = true, namespace_definition = true, declaration_list = true,
  linkage_specification = true, preproc_if = true, preproc_ifdef = true,
  preproc_else = true, preproc_elif = true, preproc_elifdef = true,
}
local SKIP = { comment = true, preproc_include = true, preproc_def = true, preproc_function_def = true,
               preproc_call = true }

local function pieces(buf)
  local lines = vim.api.nvim_buf_get_lines(buf, 0, -1, false)
  local out = {}
  local function add(s, e) -- 0-based rows, inclusive
    for a = s, e, MAX_LINES do
      local b = math.min(e, a + MAX_LINES - 1)
      out[#out + 1] = { start = a + 1, code = table.concat(lines, "\n", a + 1, b + 1) }
    end
  end
  local ok, parser = pcall(vim.treesitter.get_parser, buf)
  if not ok or not parser then
    if #lines > 0 then add(0, #lines - 1) end
    return out
  end
  local function walk(node)
    for child in node:iter_children() do
      local t = child:type()
      if CONTAINERS[t] then
        walk(child)
      elseif child:named() and not SKIP[t] then
        local s, _, e, ecol = child:range()
        if ecol == 0 and e > s then e = e - 1 end
        if e - s >= 2 then add(s, e) end -- three lines or more
      end
    end
  end
  walk(parser:parse()[1]:root())
  return out
end

---------------------------------------------------------------------------
-- Running the checks on a buffer

local bufs = {} -- buf -> { gen, ids = {}, cache = { key -> findings }, results = { key -> {check, start, findings} } }

-- What a "// nomodel" comment says about check on line lnum (0-based): one
-- at the end of the line itself, alone on the line above, or a
-- "nomodel-begin" ... "nomodel-end" pair around it (blocks, from
-- nomodel_blocks). "nomodel: a, b" covers only those checks. Returns nil when
-- no comment covers it, the note after "--" when there is one, otherwise true
-- (silenced).
local function covers(spec, check)
  local names, note = spec:match("^(.-)%-%-%s*(.-)%s*$")
  names = names or spec
  note = note and note:gsub("%s*%*/%s*$", "")
  if names:match("^%s*:") then
    local found = false
    for name in names:gmatch("[%w_%-]+") do found = found or name == check end
    if not found then return nil end
  end
  return note ~= nil and note ~= "" and note or true
end

-- The text after "nomodel" in a comment, but not "nomodel-begin" / "-end".
-- (word: the same comments for the scanners, "nocppcheck" and "nosemgrep".)
local function spec_of(line, alone, word)
  local spec = line:match((alone and "^%s*" or "") .. "/[/*]%s*" .. (word or "nomodel") .. "(.*)")
  if spec and not spec:match("^%-%a") then return spec end
end

-- The buffer's nomodel-begin/-end pairs as { first, last, spec } (0-based
-- lines between them). A begin with no end runs to the end of the file.
local function nomodel_blocks(buf, word)
  word = word or "nomodel"
  local blocks, open = {}, nil
  for i, l in ipairs(vim.api.nvim_buf_get_lines(buf, 0, -1, false)) do
    local spec = l:match("/[/*]%s*" .. word .. "%-begin(.*)")
    if spec then
      open = { first = i, last = math.huge, spec = spec }
      blocks[#blocks + 1] = open
    elseif open and l:match("/[/*]%s*" .. word .. "%-end") then
      open.last, open = i - 2, nil
    end
  end
  return blocks
end

local function nomodel(buf, lnum, check, blocks, word)
  local lines = vim.api.nvim_buf_get_lines(buf, math.max(lnum - 1, 0), lnum + 1, false)
  local spec = spec_of(lines[#lines] or "", false, word)
  local said = spec and covers(spec, check)
  if said == nil and lnum > 0 then
    spec = spec_of(lines[1] or "", true, word)
    said = spec and covers(spec, check) or nil
  end
  for _, b in ipairs(blocks) do
    if said ~= nil then break end
    if lnum >= b.first and lnum <= b.last then said = covers(b.spec, check) end
  end
  return said
end

-- For scanners.lua: what a "// <word>" comment (nocppcheck, nosemgrep) says
-- about finding id on line lnum: nil, true (silenced) or the note.
function M.silenced(buf, word)
  local blocks = nomodel_blocks(buf, word)
  return function(lnum, id) return nomodel(buf, lnum, id, blocks, word) end
end

local function publish(buf, st)
  if not vim.api.nvim_buf_is_valid(buf) then return end
  local count = vim.api.nvim_buf_line_count(buf)
  local items, blocks = {}, nomodel_blocks(buf)
  for _, r in pairs(st.results) do
    for _, f in ipairs(r.findings) do
      local lnum = r.start - 1 + f.line
      local said = lnum >= 0 and lnum < count and nomodel(buf, lnum, r.check, blocks)
      if lnum >= 0 and lnum < count and said ~= true then
        local text = vim.api.nvim_buf_get_lines(buf, lnum, lnum + 1, false)[1] or ""
        local severity = SEVERITY[f.severity] or sev.WARN
        items[#items + 1] = {
          lnum = lnum, col = (text:find("%S") or 1) - 1, end_col = #text,
          -- a checked finding sorts last and doesn't count as a warning
          severity = said and sev.HINT or severity, message = f.message,
          source = "local model (" .. r.check .. ")",
          user_data = { scanner = true, mcp = true, fix = f.fix, checked = said or nil, severity = severity },
        }
      end
    end
  end
  vim.diagnostic.set(M.ns, buf, items)
end

local function stop(buf)
  local st = bufs[buf]
  if not st then return end
  for id in pairs(st.ids) do cancel(id) end
  st.ids = {}
end

function M.run(buf)
  buf = buf or vim.api.nvim_get_current_buf()
  if not M.enabled or not vim.api.nvim_buf_is_valid(buf) then return end
  if not filetypes[vim.bo[buf].filetype] or vim.bo[buf].buftype ~= "" then return end
  local file = vim.api.nvim_buf_get_name(buf)
  with_server(function()
    tool("list_checks", {}, function(list, err)
      if not list then return warn(err) end
      if type(list.model) == "string" then client.model = list.model end
      if type(list.note) == "string" and list.note ~= "" and not noted then
        noted = true
        vim.notify("Local model checks: " .. list.note)
      end
      if not vim.api.nvim_buf_is_valid(buf) or not M.enabled then return end
      local st = bufs[buf] or { gen = 0, ids = {}, cache = {}, results = {} }
      bufs[buf] = st
      stop(buf)
      st.gen = st.gen + 1
      local gen, cache = st.gen, {}
      st.results = {}
      local ft = vim.bo[buf].filetype
      for _, check in ipairs(list.checks or {}) do
        local langs = check.languages or {}
        local wanted = (#langs == 0 or vim.tbl_contains(langs, ft))
          and (not check.opt_in or vim.tbl_contains(vim.g.mcp_checks or {}, check.name))
        for _, p in ipairs(wanted and pieces(buf) or {}) do
          -- The same code (wherever it now sits) gives the same findings, until
          -- the check's prompt or the model changes.
          local key = check.name .. "\0" .. (check.version or "") .. "\0" .. list.model .. "\0" .. vim.fn.sha256(p.code)
          if st.cache[key] then
            cache[key] = st.cache[key]
            st.results[key .. p.start] = { check = check.name, start = p.start, findings = st.cache[key] }
          else
            local id
            id = tool("run_check", {
              check = check.name, code = p.code, start_line = 1, path = file, language = vim.bo[buf].filetype,
            }, function(data, e)
              st.ids[id] = nil
              if not data then return st.gen == gen and warn(e) end
              warned = false
              local findings = {}
              for _, f in ipairs(data.findings or {}) do
                findings[#findings + 1] = { line = f.line - 1, severity = f.severity, message = f.message,
                  fix = type(f.fix) == "string" and f.fix ~= "" and f.fix or nil }
              end
              st.cache[key] = findings
              if st.gen ~= gen then return end
              st.results[key .. p.start] = { check = check.name, start = p.start, findings = findings }
              publish(buf, st)
            end)
            st.ids[id] = true
          end
        end
      end
      st.cache = cache -- only what this run used; its new findings are added as they come
      publish(buf, st)
    end)
  end)
end

-- Ask the model how to fix a scanner finding (the S popup's "Ask the model").
-- The answer shows in a popup over the code.
function M.ask_fix(buf, d)
  if not M.enabled then return vim.notify("Local model checks are off (Space tm)") end
  local first = math.max(0, d.lnum - 15)
  local lines = vim.api.nvim_buf_get_lines(buf, first, d.lnum + 15, false)
  for i, l in ipairs(lines) do lines[i] = ("%4d%s %s"):format(first + i, first + i == d.lnum + 1 and ">" or "|", l) end
  local prompt = ("%s reports on line %d of %s:\n  %s%s\n\nCode (the line is marked with >):\n%s\n\n"
    .. "Say in one or two sentences whether this is a real problem here, then show the corrected code "
    .. "for that line or the few lines around it, in the same style. Be brief.")
    :format(d.source or "A scanner", d.lnum + 1, vim.fs.basename(vim.api.nvim_buf_get_name(buf)), d.message,
      d.code and (" (" .. d.code .. ")") or "", table.concat(lines, "\n"))
  vim.notify("Asking the local model…")
  with_server(function()
    tool("ask", { prompt = prompt, system = "You are an expert C and C++ reviewer. Answer in Markdown." }, function(data, err)
      if not data then return vim.notify("Local model: " .. tostring(err), vim.log.levels.WARN) end
      local text = vim.split(("**%s, line %d**\n\n%s"):format(d.source or "Finding", d.lnum + 1, data.answer or ""), "\n")
      vim.lsp.util.open_floating_preview(text, "markdown", { border = "rounded", max_width = 100, max_height = 30,
        focus_id = "rick_model_fix", title = " Model's fix ", title_pos = "center" })
    end)
  end)
end

-- Turning the checks off stops the model working: the checks server goes
-- (Ollama drops the requests it was answering), the model is unloaded from
-- memory, and an "ollama serve" this Neovim started is stopped. The Ollama
-- app itself keeps running, idle.
local function shutdown()
  for buf in pairs(bufs) do stop(buf) end
  client.pending = {} -- no "the checks server stopped" warnings for what was cancelled
  if client.proc then pcall(function() client.proc:kill(15) end) end
  client.proc, client.ready, client.partial, client.waiting = nil, false, "", {}
  local model, pid = client.model, ollama_pid
  ollama_pid = nil
  local function stop_serve() if pid then pcall(vim.uv.kill, pid, 15) end end
  local url = vim.g.rick_mcp_url or "http://localhost:11434/v1"
  if model and url:find(":11434") and vim.fn.executable("curl") == 1 then
    vim.system({ "curl", "-s", "--noproxy", "*", "-m", "5", OLLAMA .. "/api/generate", "-d",
      vim.json.encode({ model = model, keep_alive = 0 }) }, { text = true }, stop_serve)
  else
    stop_serve()
  end
  ollama_tried = false -- turning them on again starts Ollama again if needed
end

function M.toggle()
  M.enabled = not M.enabled
  pcall(vim.fn.mkdir, vim.fs.dirname(state_file), "p")
  local state = read_state() -- keeps the model install.sh saved
  state.enabled = M.enabled
  pcall(vim.fn.writefile, { vim.json.encode(state) }, state_file)
  if M.enabled then
    warned = false
    for _, buf in ipairs(vim.api.nvim_list_bufs()) do
      if vim.api.nvim_buf_is_loaded(buf) then M.run(buf) end
    end
  else
    shutdown()
    vim.diagnostic.reset(M.ns)
  end
  vim.notify("Local model checks: " .. (M.enabled and "on" or "off"))
end

function M.setup()
  -- Only the M (drawn by scanners.lua) and the popup; no underline, since the
  -- model points at lines, not words.
  vim.diagnostic.config({ signs = false, virtual_text = false, underline = false }, M.ns)
  local group = vim.api.nvim_create_augroup("rick_mcp", { clear = true })
  local timers = {}
  vim.api.nvim_create_autocmd({ "FileType", "BufWritePost" }, {
    group = group,
    callback = function(ev)
      if ev.event == "FileType" and not filetypes[ev.match] then return end
      local buf = ev.buf
      timers[buf] = timers[buf] or vim.uv.new_timer()
      timers[buf]:stop()
      timers[buf]:start(ev.event == "FileType" and 200 or DEBOUNCE, 0, vim.schedule_wrap(function() M.run(buf) end))
    end,
  })
  vim.api.nvim_create_autocmd("BufDelete", {
    group = group,
    callback = function(ev)
      stop(ev.buf)
      bufs[ev.buf] = nil
      if timers[ev.buf] then timers[ev.buf]:stop(); timers[ev.buf]:close(); timers[ev.buf] = nil end
    end,
  })
  vim.api.nvim_create_autocmd("VimLeavePre", {
    group = group,
    callback = function() if client.proc then pcall(function() client.proc:kill(15) end) end end,
  })
  vim.keymap.set("n", "<leader>tm", M.toggle, { desc = "Toggle local model checks" })
  vim.keymap.set("n", "<leader>cm", function()
    if not M.enabled then return vim.notify("Local model checks are off (Space tm)") end
    M.run()
  end, { desc = "Run local model checks now" })
end

return M
