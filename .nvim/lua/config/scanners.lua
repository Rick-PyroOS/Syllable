-- Background scanners for C and C++: cppcheck and semgrep run on a file when
-- it opens and each time it's saved (auto-save included), without blocking
-- the editor. What they find shows as an S in the margin, next to clangd's E,
-- red for errors and yellow for warnings; rest the mouse on the S for the message.
-- This file also draws clangd's E (see draw_signs).
-- Space tc / Space ts turn cppcheck / semgrep on and off (remembered).
--
-- Silence a finding with the tools' own comments: "// cppcheck-suppress <id>"
-- or "// nosemgrep: <id>" on the line above it.
-- A project can switch to the local model's "// nomodel" style instead by
-- setting vim.g.scanner_nomodel = true in its .nvim.lua (Syllable does):
-- "// nocppcheck" and "// nosemgrep", at the end of the line or alone on the
-- line above, ": id, id" for only those findings, "-begin" / "-end" around
-- several lines. A note after "--" keeps the finding as a green shield
-- showing the note:
--   return *p;   // nocppcheck: nullPointer -- p is checked by the caller
-- cppcheck-suppress still works then, too.
-- semgrep uses the project's .semgrep.yml (or .semgrep/) when there is one,
-- otherwise semgrep/c-cpp.yml in this config.

local M = {}

local sev = vim.diagnostic.severity
local state_file = vim.fn.stdpath("state") .. "/scanners.json"
local filetypes = { c = true, cpp = true }

local function read_state()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(state_file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end

local function project_root(file)
  return vim.fs.root(file, { ".git", "compile_commands.json", ".semgrep.yml" }) or vim.fs.dirname(file)
end

-- Underline the word a finding points at rather than one character.
local function word_end(buf, lnum, col)
  local line = vim.api.nvim_buf_get_lines(buf, lnum, lnum + 1, false)[1] or ""
  local s, e = line:find("^[%w_]+", col + 1)
  return s and e or math.min(col + 1, #line)
end

local tools = {}

tools.cppcheck = {
  key = "<leader>tc",
  install = "brew install cppcheck",
  cmd = function(file, ft)
    return {
      "cppcheck", "--quiet", "--inline-suppr", "--max-configs=1",
      "--enable=warning,performance,portability",
      "--language=" .. (ft == "c" and "c" or "c++"),
      "--template={file}\t{line}\t{column}\t{severity}\t{id}\t{message}",
      file,
    }
  end,
  -- Findings come on stderr, one per line, from the template above.
  parse = function(res, buf, file)
    local map = { error = sev.ERROR, warning = sev.WARN, performance = sev.WARN, portability = sev.WARN,
                  style = sev.INFO, information = sev.HINT }
    local out = {}
    for line in (res.stderr or ""):gmatch("[^\n]+") do
      local f, l, c, s, id, msg = line:match("^(.-)\t(%d+)\t(%d+)\t(%a+)\t(%S+)\t(.*)$")
      if f and vim.fs.normalize(vim.fn.fnamemodify(f, ":p")) == file and tonumber(l) > 0 then
        local lnum, col = tonumber(l) - 1, math.max(tonumber(c) - 1, 0)
        out[#out + 1] = { lnum = lnum, col = col, end_col = word_end(buf, lnum, col),
                          severity = map[s] or sev.WARN, message = msg, code = id,
                          user_data = { advice = require("config.cppcheck_advice").for_finding(id, msg) } }
      end
    end
    return out
  end,
}

tools.semgrep = {
  key = "<leader>ts",
  install = "brew install semgrep",
  cmd = function(file, _, root)
    local config = vim.g.semgrep_config
    if not config then
      for _, name in ipairs({ ".semgrep.yml", ".semgrep.yaml", ".semgrep" }) do
        if vim.uv.fs_stat(root .. "/" .. name) then config = root .. "/" .. name break end
      end
    end
    config = config or vim.fn.stdpath("config") .. "/semgrep/c-cpp.yml"
    local cmd = { "semgrep", "scan", "--config", config, "--json", "--quiet", "--metrics=off",
                  "--disable-version-check", file }
    -- With scanner_nomodel, scan() reads nosemgrep itself (notes, blocks).
    if vim.g.scanner_nomodel then table.insert(cmd, #cmd, "--disable-nosem") end
    return cmd
  end,
  parse = function(res, buf)
    local ok, data = pcall(vim.json.decode, res.stdout or "")
    if not ok or type(data) ~= "table" then return nil end
    local map = { ERROR = sev.ERROR, WARNING = sev.WARN, INFO = sev.INFO }
    local out = {}
    for _, r in ipairs(data.results or {}) do
      -- A finding that spans lines (use after free) is marked on its last
      -- line, where the mistake is.
      local lnum = r["end"].line - 1
      local col = r.start.col - 1
      if r.start.line ~= r["end"].line then
        local text = vim.api.nvim_buf_get_lines(buf, lnum, lnum + 1, false)[1] or ""
        col = (text:find("%S") or 1) - 1
      end
      out[#out + 1] = { lnum = lnum, col = col, end_col = r["end"].col - 1,
                        severity = map[r.extra.severity] or sev.WARN,
                        message = vim.trim(r.extra.message), code = r.check_id:match("[^.]+$"),
                        -- the rule's fix: line, and the exact text it replaces (Apply fix)
                        user_data = r.extra.fix and { fix = r.extra.fix, replace = {
                          r.start.line - 1, r.start.col - 1, r["end"].line - 1, r["end"].col - 1 } } or nil }
    end
    return out
  end,
}

local saved = read_state()
for name, tool in pairs(tools) do
  tool.name = name
  tool.ns = vim.api.nvim_create_namespace("rick_scan_" .. name)
  tool.enabled = saved[name] ~= false
  tool.jobs = {}   -- buf -> running vim.system process
end

function M.enabled(name) return tools[name].enabled end

local function scan(tool, buf)
  if not tool.enabled or not vim.api.nvim_buf_is_valid(buf) then return end
  local ft, file = vim.bo[buf].filetype, vim.api.nvim_buf_get_name(buf)
  if not filetypes[ft] or vim.bo[buf].buftype ~= "" or not vim.uv.fs_stat(file) then return end
  if vim.fn.executable(tool.name) == 0 then
    if not tool.warned then
      tool.warned = true
      vim.notify(tool.name .. " isn't installed, so it won't scan: " .. tool.install
        .. " (" .. tool.key:gsub("<leader>", "Space ") .. " turns it off)", vim.log.levels.WARN)
    end
    return
  end
  file = vim.fs.normalize(file)
  if tool.jobs[buf] then pcall(function() tool.jobs[buf]:kill(15) end) end -- a newer save wins
  local root = project_root(file)
  local job
  job = vim.system(tool.cmd(file, ft, root), { cwd = root, text = true }, vim.schedule_wrap(function(res)
    if tool.jobs[buf] ~= job then return end -- superseded
    tool.jobs[buf] = nil
    if not tool.enabled or not vim.api.nvim_buf_is_valid(buf) or res.signal ~= 0 then return end
    local items = tool.parse(res, buf, file)
    if not items then return end
    local kept = {}
    local silenced = vim.g.scanner_nomodel and require("config.mcp").silenced(buf, "no" .. tool.name)
      or function() end
    for _, d in ipairs(items) do
      d.source = tool.name
      d.user_data = vim.tbl_extend("force", d.user_data or {}, { scanner = true })
      local said = silenced(d.lnum, d.code or "")
      if type(said) == "string" then -- kept, green, with your note
        d.user_data.checked, d.user_data.severity, d.severity = said, d.severity, sev.HINT
      end
      if said ~= true then kept[#kept + 1] = d end
    end
    vim.diagnostic.set(tool.ns, buf, kept)
  end))
  tool.jobs[buf] = job
end

local function scan_all(tool)
  for _, buf in ipairs(vim.api.nvim_list_bufs()) do
    if vim.api.nvim_buf_is_loaded(buf) then scan(tool, buf) end
  end
end

function M.toggle(name)
  local tool = tools[name]
  tool.enabled = not tool.enabled
  local state = {}
  for n, t in pairs(tools) do state[n] = t.enabled end
  pcall(vim.fn.mkdir, vim.fs.dirname(state_file), "p")
  pcall(vim.fn.writefile, { vim.json.encode(state) }, state_file)
  if tool.enabled then
    tool.warned = false
    scan_all(tool)
  else
    for buf, job in pairs(tool.jobs) do pcall(function() job:kill(15) end); tool.jobs[buf] = nil end
    vim.diagnostic.reset(tool.ns)
  end
  vim.notify(name .. " scanning: " .. (tool.enabled and "on" or "off"))
end

-- The margin: one sign per line for clangd, the worst of its findings there
-- (an error, warning, info or hint icon; its popup steps through them all),
-- one shield for both scanners and one robot for the local model checks
-- (mcp.lua), each colored by the worst of theirs (green when every model
-- finding on the line is marked checked). With
-- vim.g.rick_margin_letters they are E/W/I/H, S and M instead. They keep their
-- columns (clangd, then scanners, then model), so a line without a clangd
-- finding gets a blank first column, and so on. Higher priority sits further
-- left; gitsigns (6) gives way to all three. lsp.lua turns off the built-in
-- signs, which draw one per finding.
local sign_ns = vim.api.nvim_create_namespace("rick_margin_signs")
local mcp_ns = vim.api.nvim_create_namespace("rick_mcp")
local SIGN_HL = { "DiagnosticSignError", "DiagnosticSignWarn", "DiagnosticSignInfo", "DiagnosticSignHint",
  "RickChecked" } -- 5: a finding marked checked with a "// nomodel -- note" (mcp.lua)

-- The margin's symbols and the popup's: Nerd Font icons, or plain letters with
-- vim.g.rick_margin_letters = true (for a font without the icons).
local ICONS = { severity = { "", "", "", "󰌵" }, scanner = "", model = "󰚩", copy = "", checked = "" }
local LETTERS = { severity = { "E", "W", "I", "H" }, scanner = "S", model = "M", copy = "", checked = "✓" }
function M.glyphs() return vim.g.rick_margin_letters and LETTERS or ICONS end

-- Which margin column a finding is drawn in: 1 clangd, 2 cppcheck/semgrep,
-- 3 the local model and the thread check. diagnostics.lua uses it so each column's popup lists
-- only its own findings.
function M.column(d)
  if d.namespace == mcp_ns or d.namespace == require("config.threads").ns then return 3 end
  for _, tool in pairs(tools) do
    if tool.ns == d.namespace then return 2 end
  end
  return 1
end

local function draw_signs(buf)
  if not vim.api.nvim_buf_is_valid(buf) then return end
  vim.api.nvim_buf_clear_namespace(buf, sign_ns, 0, -1)
  if not vim.diagnostic.is_enabled({ bufnr = buf }) then return end
  local cols = { {}, {}, {} } -- E, S, M: line -> worst severity
  for _, d in ipairs(vim.diagnostic.get(buf)) do
    local c = cols[M.column(d)]
    local s = d.user_data and d.user_data.checked and 5 or d.severity
    c[d.lnum] = math.min(c[d.lnum] or 9, s)
  end
  local last = {} -- line -> its rightmost column with a sign
  for i, c in ipairs(cols) do
    for lnum in pairs(c) do last[lnum] = math.max(last[lnum] or 0, i) end
  end
  local PRIORITY = { 10, 8, 7 }
  for lnum, n in pairs(last) do
    for i = 1, n do
      local s = cols[i][lnum]
      local g = M.glyphs()
      local text = not s and " " or i == 1 and g.severity[s] or i == 2 and g.scanner or g.model
      pcall(vim.api.nvim_buf_set_extmark, buf, sign_ns, lnum, 0,
        { sign_text = text, sign_hl_group = s and SIGN_HL[s] or nil, priority = PRIORITY[i] })
    end
  end
end

function M.setup()
  local group = vim.api.nvim_create_augroup("rick_scanners", { clear = true })
  vim.api.nvim_create_autocmd("DiagnosticChanged", {
    group = group,
    callback = function(ev) vim.schedule(function() draw_signs(ev.buf) end) end,
  })
  for name, tool in pairs(tools) do
    vim.diagnostic.config({ signs = false, virtual_text = false }, tool.ns)
    vim.api.nvim_create_autocmd({ "FileType", "BufWritePost" }, {
      group = group,
      callback = function(ev)
        if ev.event == "FileType" and not filetypes[ev.match] then return end
        scan(tool, ev.buf)
      end,
    })
    vim.api.nvim_create_autocmd("BufDelete", {
      group = group,
      callback = function(ev)
        if tool.jobs[ev.buf] then pcall(function() tool.jobs[ev.buf]:kill(15) end); tool.jobs[ev.buf] = nil end
      end,
    })
    vim.keymap.set("n", tool.key, function() M.toggle(name) end, { desc = "Toggle " .. name .. " scanning" })
  end
end

return M
