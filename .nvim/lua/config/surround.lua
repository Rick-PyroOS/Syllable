-- Surround With (Cmd+Alt+T or Space cs), like CLion's: wraps the selected
-- lines (or the current line) in if, if/else, while, for, do/while, try/catch,
-- a { } block, #ifdef or #if 0, then puts the cursor where the condition goes.
-- Braces go on their own line when vim.g.surround_brace_newline is set
-- (Syllable/.nvim.lua sets it: Syllable writes "if( x )" then "{").

local M = {}

local function templates()
  local nl = vim.g.surround_brace_newline
  local sp = nl and "" or " "
  local function block(head) return nl and { head, "{" } or { head .. " {" } end
  local function close(tail) return { "}" .. (tail or "") } end
  local cond = nl and "( " or "("
  local endc = nl and " )" or ")"
  return {
    { "if", block("if" .. sp .. cond .. "|" .. endc), close() },
    { "if / else", block("if" .. sp .. cond .. "|" .. endc), nl and { "}", "else", "{", "}" } or { "} else {", "}" } },
    { "while", block("while" .. sp .. cond .. "|" .. endc), close() },
    { "for", block("for" .. sp .. cond .. "|;;" .. endc), close() },
    { "do / while", block("do"), close(" while" .. sp .. cond .. "|" .. endc .. ";") },
    { "try / catch", block("try"), nl and { "}", "catch( ... )", "{", "|", "}" } or { "} catch (...) {", "|", "}" } },
    { "{ }", { "{" }, close() },
    { "#ifdef", { "#ifdef |" }, { "#endif" }, raw = true },
    { "#if 0", { "#if 0" }, { "#endif" }, raw = true },
  }
end

local function wrap(t, l1, l2)
  local buf = 0
  local lines = vim.api.nvim_buf_get_lines(buf, l1 - 1, l2, false)
  local ind = (lines[1] or ""):match("^%s*")
  local step = vim.bo.expandtab and string.rep(" ", vim.fn.shiftwidth()) or "\t"
  local out, cursor = {}, nil
  local function add(s, raw)
    local text = raw and s or (ind .. s)
    local col = text:find("|", 1, true)
    if col then
      text = text:sub(1, col - 1) .. text:sub(col + 1)
      cursor = cursor or { l1 + #out, col - 1 }
    end
    out[#out + 1] = (text:match("^%s*$") and not col) and "" or text
  end
  for _, s in ipairs(t[2]) do add(s, t.raw) end
  for _, s in ipairs(lines) do out[#out + 1] = (t.raw or s == "") and s or (step .. s) end
  local close = vim.deepcopy(t[3])
  for i, s in ipairs(close) do
    -- a "|" line inside the closing part is an empty body line (catch / else)
    if s == "|" then close[i] = step .. "|" end
  end
  for _, s in ipairs(close) do add(s, t.raw) end
  vim.api.nvim_buf_set_lines(buf, l1 - 1, l2, false, out)
  if cursor then
    vim.api.nvim_win_set_cursor(0, cursor)
    -- at the end of the line (an empty body line) insert after the indent
    vim.cmd(cursor[2] >= #out[cursor[1] - l1 + 1] and "startinsert!" or "startinsert")
  end
end

function M.open()
  local mode = vim.fn.mode()
  local l1, l2 = vim.fn.line("."), vim.fn.line(".")
  if mode:match("[vV\22]") then
    l1, l2 = vim.fn.line("v"), vim.fn.line(".")
    if l1 > l2 then l1, l2 = l2, l1 end
    vim.api.nvim_feedkeys(vim.keycode("<Esc>"), "nx", false)
  end
  vim.ui.select(templates(), { prompt = "Surround With", format_item = function(t) return t[1] end }, function(t)
    if t then wrap(t, l1, l2) end
  end)
end

return M
