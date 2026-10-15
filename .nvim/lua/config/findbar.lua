-- CLion's find bar, optional (Space t/ switches it on, remembered). With it
-- on, / and Cmd+F open a small box at the top right of the editor: matches
-- highlight as you type, "3 of 12" counts them, Enter / Shift+Enter (or
-- Down / Up) step through, Esc closes it and leaves the cursor on the match.
-- Toggles, clickable or by key as in CLion: Aa Match Case (Alt+C), W Words
-- (Alt+W), .* Regex (Alt+X). Cmd+R (or Ctrl+R in the bar) adds the Replace row:
-- Enter there replaces the current match and moves on, Replace All does the file.
-- With it off, / is plain Vim search and Cmd+F opens Search Everywhere.

local M = {}
local ns = vim.api.nvim_create_namespace("rick_findbar")
local opts = { case = false, words = false, regex = false }
local st = {} -- target win/buf, bar win/buf, start cursor, replace row on?
local hits = {} -- clickable spans: { row, from, to, action }

function M.enabled() return require("config.prefs").get("find_bar") end
local function open_bar() return st.win and vim.api.nvim_win_is_valid(st.win) end

local function query() return (vim.api.nvim_buf_get_lines(st.buf, 0, 1, false)[1] or "") end
local function replacement() return (vim.api.nvim_buf_get_lines(st.buf, 1, 2, false)[1] or "") end

-- The Vim pattern for what's typed, honouring the three toggles.
function M.pattern(text, o)
  o = o or opts
  if text == "" then return nil end
  local p = o.regex and text or ("\\V" .. text:gsub("\\", "\\\\"))
  if o.words then p = (o.regex and "\\<\\(" .. p .. "\\)\\>" or "\\<" .. p .. "\\>") end
  return (o.case and "\\C" or "\\c") .. p
end

local function count()
  local pat = M.pattern(query())
  if not pat then return nil end
  local ok, c = pcall(vim.api.nvim_win_call, st.target, function()
    return vim.fn.searchcount({ pattern = pat, maxcount = 9999, timeout = 200, recompute = true })
  end)
  return ok and c or nil
end

local function chip(label, on, action, out, row)
  out[#out + 1] = { " " .. label .. " ", on and "RickFindOn" or "RickFindOff", action }
  out[#out + 1] = { " ", "RickFind" }
end

-- Status, toggles and buttons at the right of each row; remember where they are for clicks.
local function draw()
  if not open_bar() then return end
  vim.api.nvim_buf_clear_namespace(st.buf, ns, 0, -1)
  hits = {}
  local width = vim.api.nvim_win_get_width(st.win)
  local function place(row, parts)
    local vt, total = {}, 0
    for _, p in ipairs(parts) do total = total + vim.fn.strdisplaywidth(p[1]) end
    local col = width - total
    for _, p in ipairs(parts) do
      local w = vim.fn.strdisplaywidth(p[1])
      if p[3] then hits[#hits + 1] = { row = row, from = col, to = col + w, action = p[3] } end
      col = col + w
      vt[#vt + 1] = { p[1], p[2] }
    end
    vim.api.nvim_buf_set_extmark(st.buf, ns, row, 0, { virt_text = vt, virt_text_pos = "right_align" })
  end
  local c, q = count(), query()
  local status
  if q == "" then status = { "", "Comment" }
  elseif not c or c.total == 0 then status = { "0 results  ", "RickFindNone" }
  else status = { (c.current > 0 and c.current or "?") .. " of " .. c.total .. (c.incomplete == 1 and "+" or "") .. "  ", "Comment" } end
  local parts = { status, { " ↑ ", "RickFindOff", "prev" }, { " ↓ ", "RickFindOff", "next" }, { "  ", "RickFind" } }
  chip("Aa", opts.case, "case", parts)
  chip("W", opts.words, "words", parts)
  chip(".*", opts.regex, "regex", parts)
  parts[#parts + 1] = { " ✕ ", "RickFindOff", "close" }
  place(0, parts)
  if st.replace then
    place(1, { { " Replace ", "RickFindOn", "replace" }, { " ", "RickFind" }, { " Replace All ", "RickFindOn", "replace_all" }, { " ", "RickFind" } })
  end
end

-- Move the editor's cursor to a match: dir 1 next, -1 previous, 0 first from where you started.
local function go(dir)
  local pat = M.pattern(query())
  if not pat then
    vim.api.nvim_win_call(st.target, function() vim.api.nvim_win_set_cursor(0, st.start) end)
    vim.o.hlsearch = false
    return draw()
  end
  vim.fn.setreg("/", pat)
  vim.o.hlsearch = true
  vim.api.nvim_win_call(st.target, function()
    if dir == 0 then vim.api.nvim_win_set_cursor(0, st.start) end
    local flags = dir < 0 and "bw" or (dir == 0 and "cw" or "w")
    if vim.fn.search(pat, flags) == 0 and dir == 0 then vim.api.nvim_win_set_cursor(0, st.start) end
    vim.cmd("normal! zv")
  end)
  vim.cmd("redraw")
  draw()
end

function M.toggle(name)
  opts[name] = not opts[name]
  go(0)
end

local function replace_one()
  local pat = M.pattern(query())
  if not pat then return end
  local repl = replacement()
  vim.api.nvim_win_call(st.target, function()
    local pos = vim.api.nvim_win_get_cursor(0)
    -- Only if the cursor sits on a match; otherwise just find the next one.
    local m = vim.fn.matchstrpos(vim.fn.getline("."), pat, pos[2])
    if m[2] == pos[2] then
      local line = vim.fn.getline(".")
      vim.api.nvim_buf_set_text(0, pos[1] - 1, m[2], pos[1] - 1, m[3], { repl })
      vim.api.nvim_win_set_cursor(0, { pos[1], m[2] + #repl })
      if line == "" then return end
    end
    vim.fn.search(pat, "w")
  end)
  vim.cmd("redraw")
  draw()
end

local function replace_all()
  local pat = M.pattern(query())
  if not pat then return end
  vim.g.rick_find_repl = replacement()
  local n = 0
  vim.api.nvim_win_call(st.target, function()
    local c = vim.fn.searchcount({ pattern = pat, maxcount = 0 })
    n = c.total or 0
    if n > 0 then vim.cmd("keeppatterns %s/" .. pat:gsub("/", "\\/") .. "/\\=g:rick_find_repl/ge") end
  end)
  vim.notify(("Replaced %d occurrence%s"):format(n, n == 1 and "" or "s"))
  draw()
end

function M.close()
  if not open_bar() then return end
  local target = st.target
  vim.cmd("stopinsert")
  pcall(vim.api.nvim_win_close, st.win, true)
  st.win = nil
  vim.o.hlsearch = false -- CLion drops the highlights with the bar; n / N still find the next one
  if target and vim.api.nvim_win_is_valid(target) then vim.api.nvim_set_current_win(target) end
end

local ACTIONS = {
  prev = function() go(-1) end, next = function() go(1) end, close = M.close,
  case = function() M.toggle("case") end, words = function() M.toggle("words") end, regex = function() M.toggle("regex") end,
  replace = replace_one, replace_all = replace_all,
}

local function layout()
  local tw = vim.api.nvim_win_get_width(st.target)
  local width = math.min(tw - 4, 78)
  return {
    relative = "win", win = st.target, anchor = "NE", row = 0, col = tw - 1, width = width,
    height = st.replace and 2 or 1, border = "rounded", style = "minimal", zindex = 55,
    title = { { st.replace and " Replace " or " Find ", "RickFindTitle" } }, title_pos = "left",
  }
end

function M.open(with_replace)
  if open_bar() then
    vim.api.nvim_set_current_win(st.win)
    if with_replace then
      if not st.replace then
        st.replace = true
        if vim.api.nvim_buf_line_count(st.buf) < 2 then vim.api.nvim_buf_set_lines(st.buf, 1, 1, false, { "" }) end
        vim.api.nvim_win_set_config(st.win, layout())
      end
      vim.api.nvim_win_set_cursor(st.win, { 2, 0 })
    end
    vim.cmd("startinsert!")
    return draw()
  end
  local target = vim.api.nvim_get_current_win()
  local word
  -- A one-line selection fills the box, as in CLion.
  local mode = vim.fn.mode()
  if mode == "v" then
    local a, b = vim.fn.getpos("v"), vim.fn.getpos(".")
    if a[2] == b[2] then
      local l = vim.fn.getline(a[2])
      word = l:sub(math.min(a[3], b[3]), math.max(a[3], b[3]))
    end
    vim.api.nvim_feedkeys(vim.keycode("<Esc>"), "nx", false)
  end
  st = { target = target, start = vim.api.nvim_win_get_cursor(target), replace = with_replace, buf = st.buf }
  if not (st.buf and vim.api.nvim_buf_is_valid(st.buf)) then
    st.buf = vim.api.nvim_create_buf(false, true)
    vim.bo[st.buf].bufhidden = "hide"
    vim.bo[st.buf].filetype = "rick_findbar"
    vim.b[st.buf].completion = false -- no completion menu over the bar
    local b = st.buf
    local function k(modes, lhs, fn) vim.keymap.set(modes, lhs, fn, { buffer = b, nowait = true }) end
    local ni = { "n", "i" }
    k(ni, "<Esc>", M.close)
    k("n", "q", M.close)
    k(ni, "<CR>", function()
      if st.replace and vim.api.nvim_win_get_cursor(0)[1] == 2 then replace_one() else go(1) end
    end)
    for _, lhs in ipairs({ "<S-CR>", "<Up>", "<D-G>" }) do k(ni, lhs, function() go(-1) end) end
    for _, lhs in ipairs({ "<Down>", "<D-g>", "<F3>" }) do k(ni, lhs, function() go(1) end) end
    k(ni, "<M-c>", function() M.toggle("case") end)
    k(ni, "<M-w>", function() M.toggle("words") end)
    k(ni, "<M-x>", function() M.toggle("regex") end)
    k(ni, "<C-r>", function() M.open(true) end)
    k(ni, "<D-r>", function() M.open(true) end)
    k(ni, "<Tab>", function() if st.replace then vim.api.nvim_win_set_cursor(0, { vim.fn.line(".") == 1 and 2 or 1, 0 }) vim.cmd("startinsert!") end end)
    k(ni, "<M-a>", replace_all)
    vim.api.nvim_create_autocmd({ "TextChangedI", "TextChanged" }, {
      buffer = b,
      callback = function()
        if vim.api.nvim_buf_line_count(b) > (st.replace and 2 or 1) then -- no new lines from a paste
          local l = vim.api.nvim_buf_get_lines(b, 0, -1, false)
          vim.api.nvim_buf_set_lines(b, 0, -1, false, { l[1], st.replace and (l[2] or "") or nil })
        end
        if vim.api.nvim_win_get_cursor(0)[1] == 1 then go(0) else draw() end
      end,
    })
    vim.api.nvim_create_autocmd("WinLeave", { buffer = b, callback = function() vim.schedule(function()
      if open_bar() and vim.api.nvim_get_current_win() ~= st.win then M.close() end
    end) end })
  end
  local keep = vim.api.nvim_buf_get_lines(st.buf, 0, -1, false)
  vim.api.nvim_buf_set_lines(st.buf, 0, -1, false, { word or keep[1] or "", st.replace and (keep[2] or "") or nil })
  st.win = vim.api.nvim_open_win(st.buf, true, layout())
  vim.wo[st.win].winhighlight = "NormalFloat:RickFind,FloatBorder:RickFindBorder,Search:None,CurSearch:None,IncSearch:None"
  vim.wo[st.win].wrap = false
  vim.api.nvim_win_set_cursor(st.win, { 1, 0 })
  vim.cmd("startinsert!")
  go(0)
  if with_replace and query() ~= "" then vim.api.nvim_win_set_cursor(st.win, { 2, 0 }) end
end

function M.setup()
  local function hl()
    vim.api.nvim_set_hl(0, "RickFind", { link = "NormalFloat", default = true })
    vim.api.nvim_set_hl(0, "RickFindBorder", { link = "FloatBorder", default = true })
    vim.api.nvim_set_hl(0, "RickFindTitle", { link = "FloatTitle", default = true })
    vim.api.nvim_set_hl(0, "RickFindOn", { link = "PmenuSel", default = true })
    vim.api.nvim_set_hl(0, "RickFindOff", { link = "Pmenu", default = true })
    vim.api.nvim_set_hl(0, "RickFindNone", { link = "DiagnosticError", default = true })
  end
  hl()
  vim.api.nvim_create_autocmd("ColorScheme", { group = vim.api.nvim_create_augroup("rick_findbar", { clear = true }), callback = hl })

  -- Clicks on the toggles and buttons.
  local press, release = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
  local swallow = false
  vim.on_key(function(key)
    if key == release and swallow then swallow = false; return "" end
    if key ~= press or not open_bar() then return end
    local m = vim.fn.getmousepos()
    if m.winid ~= st.win then return end
    for _, h in ipairs(hits) do
      if m.line - 1 == h.row and m.wincol - 1 >= h.from and m.wincol - 1 < h.to then
        swallow = true
        vim.schedule(ACTIONS[h.action])
        return ""
      end
    end
  end, ns)

  -- Plain / and ? when the bar is off (counts and operators like d/ keep working).
  local function search_key(key)
    return function()
      if not M.enabled() or vim.fn.mode(1):match("^no") then return key end
      vim.schedule(function() M.open(false) end)
      return "<Ignore>"
    end
  end
  vim.keymap.set({ "n", "x", "o" }, "/", search_key("/"), { expr = true, desc = "Find (find bar when on)" })
  vim.keymap.set({ "n", "x", "o" }, "?", search_key("?"), { expr = true, desc = "Find backwards (find bar when on)" })
  vim.keymap.set({ "n", "x" }, "<D-f>", function()
    if M.enabled() then M.open(false) else require("config.search_everywhere").open("All") end
  end, { desc = "Find (find bar, or Search Everywhere when it's off)" })
  vim.keymap.set({ "n", "x" }, "<D-r>", function()
    if M.enabled() then M.open(true) else vim.api.nvim_feedkeys(":%s/", "n", false) end
  end, { desc = "Replace in file" })
end

return M
