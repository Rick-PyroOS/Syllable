-- The prompt window, for bigger questions and tasks than a completion: a chat
-- panel on the right (Space ap, Cmd+Shift+I, or :AIPrompt). Type in the box at
-- the bottom (Enter makes a new line, so you can write as much as you like)
-- and send with Ctrl+Enter or Cmd+Enter. The answer streams in above, and the
-- conversation carries on until you clear it (Ctrl+L or :AIPrompt clear).
-- Ctrl+C stops an answer. Opened from a selection, the selection goes along.
--
-- Click the model name in the box's title to switch:
--   Local model   Ollama (or llama.cpp's llama-server) on this Mac, Qwen 27B
--                 by default. The endpoint is OpenAI-style:
--                   vim.g.ai_url   = "http://127.0.0.1:11434/v1"   -- Ollama
--                                    "http://127.0.0.1:8080/v1"    -- llama-server
--                   vim.g.ai_model = "qwen3.8:27b"
--                 Ollama is started when it isn't running.
--   Claude        the claude CLI answers here (it can read the project, not
--                 change it)
--   Claude Code   sends the prompt to the Claude panel (Space ac), the agent
--                 that can edit files, with diffs to accept or reject
-- "+ file" in the title adds the current file's text to what's sent.

local M = {}

local BACKENDS = { "Local model", "Claude", "Claude Code" }
local st = {
  backend = BACKENDS[1], with_file = false, history = {}, session = nil,
  chat = nil, input = nil, chat_win = nil, input_win = nil, job = nil, source = nil,
}

local function url() return (vim.g.ai_url or "http://127.0.0.1:11434/v1"):gsub("/$", "") end
local function model() return vim.g.ai_model or "qwen3.8:27b" end

local function valid_win(w) return w and vim.api.nvim_win_is_valid(w) end

-- The chat buffer -----------------------------------------------------------------

local function chat_append(text)
  local b = st.chat
  if not (b and vim.api.nvim_buf_is_valid(b)) then return end
  vim.bo[b].modifiable = true
  local last = vim.api.nvim_buf_line_count(b)
  local tail = vim.api.nvim_buf_get_lines(b, last - 1, last, false)[1] or ""
  local parts = vim.split(text, "\n", { plain = true })
  parts[1] = tail .. parts[1]
  vim.api.nvim_buf_set_lines(b, last - 1, last, false, parts)
  vim.bo[b].modifiable = false
  if valid_win(st.chat_win) then
    pcall(vim.api.nvim_win_set_cursor, st.chat_win, { vim.api.nvim_buf_line_count(b), 0 })
  end
end

local function title()
  local who = st.backend == "Local model" and model() or st.backend
  local busy = st.job and "  %#DiagnosticWarn#● answering, Ctrl+C stops" or ""
  return ("%%#WinBar# %%@v:lua.RickAIBackend@%%#PmenuSel# %s ▾ %%X %%@v:lua.RickAIFile@%%#%s# + file %%X%s%%#WinBar#%%=%%#Comment#Ctrl+Enter sends "):format(
    who:gsub("%%", "%%%%"), st.with_file and "PmenuSel" or "Pmenu", busy)
end

local function refresh_title()
  if valid_win(st.input_win) then vim.wo[st.input_win].winbar = title() end
end

_G.RickAIBackend = function()
  vim.ui.select(BACKENDS, { prompt = "Answer with" }, function(b)
    if b then
      st.backend = b
      refresh_title()
    end
  end)
end
_G.RickAIFile = function()
  st.with_file = not st.with_file
  refresh_title()
end

-- Sending ----------------------------------------------------------------------------

-- What goes with the prompt, and how the transcript names it (nil: nothing worth a note).
local function context()
  local parts, what = {}, {}
  local src = st.source
  if src and vim.api.nvim_buf_is_valid(src.buf) then
    local name = vim.fn.fnamemodify(vim.api.nvim_buf_get_name(src.buf), ":~:.")
    local ft = vim.bo[src.buf].filetype
    if src.sel then
      parts[#parts + 1] = ("Selected code from %s (lines %d-%d):\n```%s\n%s\n```"):format(name, src.sel[1], src.sel[2], ft,
        table.concat(vim.api.nvim_buf_get_lines(src.buf, src.sel[1] - 1, src.sel[2], false), "\n"))
      what[#what + 1] = "the selection"
    end
    if st.with_file then
      parts[#parts + 1] = ("The file %s:\n```%s\n%s\n```"):format(name, ft, table.concat(vim.api.nvim_buf_get_lines(src.buf, 0, -1, false), "\n"))
      what[#what + 1] = name
    elseif not src.sel and name ~= "" then
      parts[#parts + 1] = ("(I'm working in %s.)"):format(name)
    end
  end
  return table.concat(parts, "\n\n"), #what > 0 and table.concat(what, " and ") or nil
end

local function finish(err)
  st.job = nil
  if err then chat_append("\n\n_" .. err .. "_") end
  chat_append("\n")
  refresh_title()
end

-- One line of a streamed reply (OpenAI style: data: {...}).
local function local_line(line, acc)
  local data = line:match("^data: (.*)")
  if not data or data == "[DONE]" then return end
  local ok, j = pcall(vim.json.decode, data)
  local delta = ok and j.choices and j.choices[1] and j.choices[1].delta
  local text = delta and delta.content
  if type(text) == "string" and text ~= "" then
    acc[#acc + 1] = text
    chat_append(text)
  end
end

local function stream(argv, on_line, done)
  local partial = ""
  local ok, obj = pcall(vim.system, argv, {
    text = true,
    stdout = function(_, data)
      if not data then return end
      local text = partial .. data
      local lines = vim.split(text, "\n", { plain = true })
      partial = table.remove(lines)
      vim.schedule(function() for _, l in ipairs(lines) do on_line(l) end end)
    end,
  }, function(res)
    vim.schedule(function()
      if partial ~= "" then on_line(partial) end
      done(res)
    end)
  end)
  if not ok then return nil, tostring(obj) end
  return obj
end

local ollama_checked = false
local function ensure_ollama(go)
  if ollama_checked or not url():find(":11434") or vim.fn.executable("ollama") == 0 then return go() end
  vim.system({ "curl", "-s", "--noproxy", "*", "-m", "1", "-o", "/dev/null", "-w", "%{http_code}", "http://127.0.0.1:11434/api/version" },
    { text = true }, function(r)
      vim.schedule(function()
        ollama_checked = true
        if r.stdout == "200" then return go() end
        chat_append("_Starting Ollama…_\n\n")
        vim.system({ "ollama", "serve" }, { detach = true })
        vim.defer_fn(go, 2500)
      end)
    end)
end

local function send_local(prompt)
  local messages = { { role = "system", content = "You are a senior C and C++ engineer helping in the user's editor. Be concrete; show code in fenced blocks." } }
  vim.list_extend(messages, st.history)
  messages[#messages + 1] = { role = "user", content = prompt }
  local body = vim.json.encode({ model = model(), messages = messages, stream = true })
  local acc = {}
  ensure_ollama(function()
    local job, err = stream({ "curl", "-sN", "--noproxy", "*", url() .. "/chat/completions", "-H", "Content-Type: application/json", "-d", body },
      function(l) local_line(l, acc) end,
      function(res)
        if st.job and st.job.rick_stopped then return finish("Stopped.") end
        if #acc == 0 then
          return finish(res.code ~= 0 and ("Couldn't reach the model at " .. url() .. " (is Ollama or llama-server running?)")
            or ("No answer from " .. model() .. " (is it pulled? ollama pull " .. model() .. ")"))
        end
        table.insert(st.history, { role = "user", content = prompt })
        table.insert(st.history, { role = "assistant", content = table.concat(acc) })
        finish()
      end)
    if not job then return finish(err) end
    st.job = job
    refresh_title()
  end)
end

local function send_claude(prompt)
  if vim.fn.executable("claude") == 0 then return finish("The claude CLI isn't installed.") end
  local argv = { "claude", "-p", "--output-format", "stream-json", "--verbose", "--include-partial-messages" }
  if st.session then vim.list_extend(argv, { "--resume", st.session }) end
  argv[#argv + 1] = prompt
  local got = false
  local job, err = stream(argv, function(l)
    local ok, j = pcall(vim.json.decode, l)
    if not ok or type(j) ~= "table" then return end
    if j.type == "stream_event" and j.event then
      local e = j.event
      if e.type == "content_block_delta" and e.delta and e.delta.type == "text_delta" then
        got = true
        chat_append(e.delta.text)
      elseif e.type == "content_block_start" and e.content_block and e.content_block.type == "tool_use" then
        chat_append("\n_(" .. (e.content_block.name or "tool") .. ")_\n")
      end
    elseif j.type == "result" then
      st.session = j.session_id or st.session
      if not got and type(j.result) == "string" then chat_append(j.result) end
    end
  end, function(res)
    if st.job and st.job.rick_stopped then return finish("Stopped.") end
    finish(res.code ~= 0 and not got and ("claude exited with " .. res.code) or nil)
  end)
  if not job then return finish(err) end
  st.job = job
  refresh_title()
end

local function send_claude_code(prompt)
  local ok = pcall(vim.cmd, "ClaudeCodeFocus")
  if not ok then return finish("The Claude panel isn't available (Space ac).") end
  vim.defer_fn(function()
    local buf = vim.api.nvim_get_current_buf()
    local job = vim.b[buf].terminal_job_id
    if not job then return finish("Couldn't find the Claude panel's terminal.") end
    -- Bracketed paste keeps the lines together; then Enter submits.
    vim.api.nvim_chan_send(job, "\27[200~" .. prompt .. "\27[201~")
    vim.defer_fn(function() pcall(vim.api.nvim_chan_send, job, "\r") end, 150)
    finish("Sent to the Claude panel.")
  end, 600)
end

function M.send()
  if st.job then return vim.notify("Still answering: Ctrl+C stops it") end
  local text = vim.trim(table.concat(vim.api.nvim_buf_get_lines(st.input, 0, -1, false), "\n"))
  if text == "" then return end
  local ctx, what = context()
  local prompt = ctx ~= "" and (text .. "\n\n" .. ctx) or text
  vim.api.nvim_buf_set_lines(st.input, 0, -1, false, { "" })
  local b = st.chat
  if vim.api.nvim_buf_line_count(b) > 1 or vim.api.nvim_buf_get_lines(b, 0, 1, false)[1] ~= "" then chat_append("\n") end
  chat_append("## You\n\n" .. text .. (what and ("\n\n_(with " .. what .. ")_") or "") .. "\n\n## " ..
    (st.backend == "Local model" and model() or st.backend) .. "\n\n")
  st.source.sel = nil -- a selection goes with the first message only
  if st.backend == "Local model" then send_local(prompt)
  elseif st.backend == "Claude" then send_claude(prompt)
  else send_claude_code(text .. (ctx ~= "" and ("\n\n" .. ctx) or "")) end
end

function M.stop()
  if st.job then
    st.job.rick_stopped = true
    pcall(function() st.job:kill(15) end)
  end
end

function M.clear()
  M.stop()
  st.history, st.session = {}, nil
  if st.chat and vim.api.nvim_buf_is_valid(st.chat) then
    vim.bo[st.chat].modifiable = true
    vim.api.nvim_buf_set_lines(st.chat, 0, -1, false, { "" })
    vim.bo[st.chat].modifiable = false
  end
end

-- The window ------------------------------------------------------------------------

local function buffers()
  if not (st.chat and vim.api.nvim_buf_is_valid(st.chat)) then
    st.chat = vim.api.nvim_create_buf(false, true)
    vim.api.nvim_buf_set_name(st.chat, "AI Chat")
    vim.bo[st.chat].filetype = "markdown"
    vim.bo[st.chat].bufhidden = "hide"
    vim.bo[st.chat].modifiable = false
  end
  if not (st.input and vim.api.nvim_buf_is_valid(st.input)) then
    st.input = vim.api.nvim_create_buf(false, true)
    vim.api.nvim_buf_set_name(st.input, "AI Prompt")
    vim.bo[st.input].filetype = "markdown"
    vim.bo[st.input].bufhidden = "hide"
    local function k(modes, lhs, fn) vim.keymap.set(modes, lhs, fn, { buffer = st.input, desc = "AI prompt" }) end
    k({ "n", "i" }, "<C-CR>", M.send)
    k({ "n", "i" }, "<D-CR>", M.send)
    k({ "n" }, "<CR>", M.send)
    k({ "n", "i" }, "<C-c>", M.stop)
    k({ "n", "i" }, "<C-l>", M.clear)
    vim.keymap.set("n", "<C-c>", M.stop, { buffer = st.chat })
    vim.keymap.set("n", "q", M.close, { buffer = st.chat })
  end
end

function M.close()
  for _, w in ipairs({ st.input_win, st.chat_win }) do
    if valid_win(w) then pcall(vim.api.nvim_win_close, w, true) end
  end
  st.input_win, st.chat_win = nil, nil
end

function M.open(opts)
  opts = opts or {}
  local buf = vim.api.nvim_get_current_buf()
  local sel
  if vim.fn.mode():match("^[vV\22]") then
    local a, b = vim.fn.line("v"), vim.fn.line(".")
    sel = { math.min(a, b), math.max(a, b) }
    vim.cmd("normal! \27")
  elseif opts.range and opts.range > 0 then
    sel = { opts.line1, opts.line2 }
  end
  if vim.bo[buf].buftype == "" then st.source = { buf = buf, sel = sel } end
  st.source = st.source or { buf = buf }
  buffers()
  if not valid_win(st.chat_win) then
    vim.cmd("botright vsplit")
    st.chat_win = vim.api.nvim_get_current_win()
    vim.api.nvim_win_set_width(st.chat_win, math.max(50, math.floor(vim.o.columns * 0.38)))
    vim.api.nvim_win_set_buf(st.chat_win, st.chat)
    vim.cmd("belowright split")
    st.input_win = vim.api.nvim_get_current_win()
    vim.api.nvim_win_set_buf(st.input_win, st.input)
    vim.api.nvim_win_set_height(st.input_win, 8)
    for _, w in ipairs({ st.chat_win, st.input_win }) do
      for opt, v in pairs({ number = false, relativenumber = false, signcolumn = "no", foldcolumn = "0", wrap = true,
        linebreak = true, statuscolumn = "", spell = false, winfixwidth = true }) do
        vim.wo[w][opt] = v
      end
    end
    vim.wo[st.input_win].winfixheight = true
    vim.wo[st.chat_win].winbar = "%#WinBar# AI Chat %#Comment#  Ctrl+L clears · q closes"
  end
  refresh_title()
  vim.api.nvim_set_current_win(st.input_win)
  vim.cmd.startinsert()
end

function M.toggle()
  if valid_win(st.input_win) or valid_win(st.chat_win) then return M.close() end
  M.open()
end

function M.setup()
  vim.api.nvim_create_user_command("AIPrompt", function(o)
    if o.args == "clear" then return M.clear() end
    M.open({ range = o.range, line1 = o.line1, line2 = o.line2 })
  end, { range = true, nargs = "?", complete = function() return { "clear" } end, desc = "Prompt window (local model or Claude)" })
  vim.keymap.set("n", "<leader>ap", M.toggle, { desc = "Prompt window" })
  vim.keymap.set("x", "<leader>ap", M.open, { desc = "Prompt window with the selection" })
  vim.keymap.set({ "n", "i" }, "<D-I>", M.toggle, { desc = "Prompt window" })
  vim.keymap.set("x", "<D-I>", M.open, { desc = "Prompt window with the selection" })
end

return M
