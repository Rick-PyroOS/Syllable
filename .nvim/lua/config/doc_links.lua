-- Links in documentation popups, like CLion's quick documentation: in the K /
-- F1, Cmd+P (parameter info) and mouse-hover popups, type and function names
-- are links to their definition. Click one, or step into the popup (press
-- the key again) and press Enter or Ctrl+] on any name. The names that look
-- like links are underlined: types (String, os::Window), calls (GetFlags())
-- and everything after "See also:" (not the folders of a path, like an
-- #include's). With several matches you pick one.
-- Ctrl+O comes back, as after any jump.

local M = {}

local ns = vim.api.nvim_create_namespace("rick_doc_links")

-- Capitalized words in the popups that are never symbols.
local SKIP = {}
for w in ([[Brief Details Parameters Returns Return Description Note Notes Warning Author See Type
  Value Since Bug Error Errors In Passed Template Throws Precondition Postcondition Deprecated
  Version Locks Required Taken NULL TRUE FALSE]]):gmatch("%S+") do SKIP[w] = true end
local KEYWORDS = {}
for w in ("const volatile unsigned signed int char bool void long short float double auto static virtual inline public private protected class struct enum union typename template namespace operator return"):gmatch("%S+") do KEYWORDS[w] = true end

-- Underline the link-like names in a popup's lines (the raw markdown the
-- buffer holds; render-markdown hides the backticks and fences).
local function mark(buf)
  vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
  local lines = vim.api.nvim_buf_get_lines(buf, 0, -1, false)
  local in_fence = false
  for i, line in ipairs(lines) do
    local function add(s, e)
      pcall(vim.api.nvim_buf_set_extmark, buf, ns, i - 1, s - 1, { end_col = e, hl_group = "RickDocLink" })
    end
    -- Capitalized identifiers and qualified names in a stretch of code.
    local function code(from, to)
      local text = line:sub(from, to)
      if text:find("[/\\]") or text:match("^[%w_%-]+%.%l%l?%l?%l?$") then return end -- a path or file name, not a symbol
      for s, word, e in text:gmatch("()([%a_][%w_]*[%w_:]*[%w_])()") do
        local qualified = word:find("::", 1, true)
        if (qualified or word:match("^%u")) and not SKIP[word] and not KEYWORDS[word] then
          add(from + s - 1, from + e - 2)
        end
      end
    end
    if line:match("^```") then
      in_fence = not in_fence
    elseif in_fence then
      if not line:match("^%s*//") then code(1, #line) end
    elseif not line:match("^#") then
      local sa = line:match("^%*%*See also:%*%*()")
      if sa then -- every entry: "GetFlags(), os::Button, View::KeyUp()"
        for s, item, e in line:sub(sa):gmatch("()([%a_~][%w_:~]*)()") do
          if not KEYWORDS[item] then add(sa + s - 1, sa + e - 2) end
        end
      else
        for s, inner, e in line:gmatch("`()([^`]+)()`") do code(s, e - 1) end
        for s, word, e in line:gmatch("()([%a_][%w_]*::[%a_][%w_:]*)()") do add(s, e - 1) end
        for s, word, e in line:gmatch("()([%a_][%w_]*)%(%)()") do add(s, s + #word - 1) end
      end
    end
  end
end

-- The name under the cursor, with its scope: "os::Button", "GetFlags".
local function name_at_cursor()
  local line = vim.api.nvim_get_current_line()
  local col = vim.api.nvim_win_get_cursor(0)[2] + 1
  if not line:sub(col, col):match("[%w_:~]") then return end
  local s, e = col, col
  while s > 1 and line:sub(s - 1, s - 1):match("[%w_:~]") do s = s - 1 end
  while e < #line and line:sub(e + 1, e + 1):match("[%w_:~]") do e = e + 1 end
  local name = line:sub(s, e):gsub("^:+", ""):gsub(":+$", "")
  return name ~= "" and name or nil
end

local function on_link()
  local pos = vim.api.nvim_win_get_cursor(0)
  local marks = vim.api.nvim_buf_get_extmarks(0, ns, { pos[1] - 1, 0 }, { pos[1] - 1, -1 }, { details = true })
  for _, m in ipairs(marks) do
    if pos[2] >= m[3] and pos[2] < m[4].end_col then return true end
  end
end

-- A normal window showing buf (the file the popup is about).
local function file_window(buf)
  local prev = vim.fn.win_getid(vim.fn.winnr("#"))
  if prev ~= 0 and vim.api.nvim_win_get_buf(prev) == buf and vim.api.nvim_win_get_config(prev).relative == "" then return prev end
  for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if vim.api.nvim_win_get_buf(w) == buf and vim.api.nvim_win_get_config(w).relative == "" then return w end
  end
end

local function go(loc, client, src)
  local float = vim.api.nvim_get_current_win()
  local win = file_window(src)
  if win then vim.api.nvim_set_current_win(win) end
  if vim.api.nvim_win_is_valid(float) and vim.api.nvim_win_get_config(float).relative ~= "" then
    pcall(vim.api.nvim_win_close, float, true)
  end
  vim.cmd("normal! m'")
  vim.lsp.util.show_document(loc, client.offset_encoding, { focus = true })
end

-- Look the name up in clangd's index and go to it.
function M.follow(name)
  name = name or name_at_cursor()
  if not name then return end
  local src = vim.w.lsp_floating_bufnr
  if not src or not vim.api.nvim_buf_is_valid(src) then src = vim.api.nvim_get_current_buf() end
  local client = vim.lsp.get_clients({ bufnr = src, method = "workspace/symbol" })[1]
  if not client then return vim.notify("No language server to look up " .. name, vim.log.levels.WARN) end
  local short = name:match("([%w_~]+)$")
  client:request("workspace/symbol", { query = name }, function(err, result)
    if err or not result then return vim.notify("Couldn't look up " .. name, vim.log.levels.WARN) end
    local exact, found = {}, {}
    for _, s in ipairs(result) do
      local full = ((s.containerName or "") ~= "" and s.containerName .. "::" or "") .. s.name
      if full == name or full:sub(-#name - 2) == "::" .. name then
        table.insert(exact, { full = full, s = s })
      elseif s.name == short then
        table.insert(found, { full = full, s = s })
      end
    end
    local list = #exact > 0 and exact or found
    if #list == 0 then
      return vim.notify("Can't find " .. name .. " in the project", vim.log.levels.INFO)
    elseif #list == 1 then
      return go(list[1].s.location, client, src)
    end
    vim.ui.select(list, {
      prompt = "Go to " .. name,
      format_item = function(it)
        local uri = it.s.location.uri
        return it.full .. "  " .. vim.fn.fnamemodify(vim.uri_to_fname(uri), ":~:.") .. ":" .. (it.s.location.range.start.line + 1)
      end,
    }, function(it) if it then go(it.s.location, client, src) end end)
  end, src)
end

local function attach(buf)
  mark(buf)
  local o = { buffer = buf, nowait = true, silent = true }
  vim.keymap.set("n", "<CR>", function() M.follow() end, vim.tbl_extend("force", o, { desc = "Go to the name under the cursor" }))
  vim.keymap.set("n", "<C-]>", function() M.follow() end, o)
  -- A click (not a drag to select) on an underlined name follows it.
  vim.keymap.set("n", "<LeftRelease>", function()
    if on_link() then vim.schedule(function() M.follow() end) return "" end
    return "<LeftRelease>"
  end, vim.tbl_extend("force", o, { expr = true }))
end

-- How wide a popup's text needs (its longest line), at most max_width.
function M.content_width(win, max_width)
  local width = 1
  for _, l in ipairs(vim.api.nvim_buf_get_lines(vim.api.nvim_win_get_buf(win), 0, -1, false)) do
    width = math.max(width, vim.fn.strdisplaywidth(l))
  end
  return math.min(width, max_width)
end

-- A popup Neovim squeezed narrower than its text (opened near the right edge,
-- or beside a cursor in a right-hand split) gets its full width back, moved
-- left as far as it needs to stay on screen.
local function fit(win, opts)
  local max_width = opts.max_width or math.min(100, math.floor(vim.o.columns * 0.6))
  local want = M.content_width(win, max_width)
  if vim.api.nvim_win_get_width(win) >= want then return end
  local pos = vim.api.nvim_win_get_position(win) -- screen row/col, borders excluded
  local border = vim.api.nvim_win_get_config(win).border
  local edge = (type(border) == "table" and #border > 0 and border[1] ~= "") and 1 or 0
  vim.api.nvim_win_set_config(win, {
    relative = "editor",
    row = pos[1] - edge,
    col = math.max(0, math.min(pos[2] - edge, vim.o.columns - want - 2 * edge)),
    width = want,
  })
  local lines = vim.o.lines - vim.o.cmdheight - (pos[1] - edge) - 2 * edge
  vim.api.nvim_win_set_height(win, math.max(1, math.min(vim.api.nvim_win_text_height(win, {}).all, lines)))
end

function M.setup()
  vim.api.nvim_set_hl(0, "RickDocLink", { underline = true, default = true })
  -- Code in the popups (clangd puts names and paths in backticks) keeps its
  -- color but not the shaded background, which looked like a selection. The
  -- shade comes from the colorscheme's markdown highlight, used by every
  -- popup (hover, signature help, completion docs), so it goes there; in a
  -- markdown file render-markdown still shades code with its own highlight.
  -- RickDocCode is that highlight in popups (plugins/markdown.lua).
  local function code_hl()
    local raw = vim.api.nvim_get_hl(0, { name = "@markup.raw.markdown_inline", link = false })
    if not raw.fg then raw = vim.api.nvim_get_hl(0, { name = "String", link = false }) end
    raw.bg, raw.ctermbg = nil, nil
    vim.api.nvim_set_hl(0, "@markup.raw.markdown_inline", raw)
    local float = vim.api.nvim_get_hl(0, { name = "NormalFloat", link = false })
    vim.api.nvim_set_hl(0, "RickDocCode", { fg = raw.fg, bg = float.bg })
    -- K / F1 also boxed the name in the code while its popup was open.
    vim.api.nvim_set_hl(0, "LspReferenceTarget", {})
  end
  code_hl()
  vim.api.nvim_create_autocmd("ColorScheme", {
    group = vim.api.nvim_create_augroup("rick_doc_links", { clear = true }),
    callback = code_hl,
  })
  local open = vim.lsp.util.open_floating_preview
  vim.lsp.util.open_floating_preview = function(contents, syntax, opts)
    local buf, win = open(contents, syntax, opts)
    if syntax == "markdown" and buf and vim.api.nvim_buf_is_valid(buf) and win and vim.api.nvim_win_is_valid(win)
      and vim.api.nvim_win_get_config(win).relative ~= "" then
      pcall(attach, buf)
      pcall(fit, win, opts or {})
      pcall(require("config.copy_button").add, win)
    end
    return buf, win
  end
end

return M
