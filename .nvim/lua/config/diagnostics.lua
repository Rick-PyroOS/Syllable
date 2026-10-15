-- Errors and argument hints, CLion style.
--
--  * Errors show in the margin and as a red underline, with no message at the
--    end of the line. Rest the mouse on a margin symbol to read that column's
--    findings (nothing pops up over the code itself, by mouse or cursor;
--    Space cd shows everything on the cursor line). The popup shows one
--    finding at a time, worst first, as  ◀ 1/3 ▶ : click an arrow, scroll the
--    wheel over it, or press Left / Right once you're in it (Space cd puts you
--    there). Its Copy buttons copy the message or the suggested fix. cppcheck
--    findings add a Recommendation (cppcheck_advice.lua); with local model
--    checks on, "Ask the model" asks it for a fix to a cppcheck or semgrep
--    finding. Click in
--    the popup to select text: drag over the text to copy it, or use y; Esc
--    or q goes back to the file.
--  * A call that is missing arguments shows their names in red where they go:
--      os::Window(cFrame, pzName, pzTitle)
--    (not while you're typing in that file: the signature popup shows them then,
--    and a just-completed foo() would look as if the names had been typed in)
--  * When there are several constructors (overloads), the popup shows one at a
--    time as  ◀ 1/3 ▶ . Scroll the mouse wheel over the popup, click an arrow,
--    or press ]o / [o on the line to step through them; the red hint follows.
--  * Parameter names before the arguments you did pass ("x: 1") come from
--    clangd's inlay hints (lsp.lua turns them on; Space th toggles).
--
-- clangd has no signature help for base-class constructors (": os::Window()"),
-- so the candidates come from clang's error notes, which point at each
-- declaration, and the parameters are read from the declaration itself.

local M = {}

local ns = vim.api.nvim_create_namespace("rick_missing_args")
local calls = {} -- [buf][key] = { lnum, row, col, have, cands = { {sig, params} }, sel }

local function key(d) return d.lnum .. ":" .. d.col end

-- Lines of a file, from the open buffer if it is loaded (0-based `from`).
local function read_lines(path, from, count)
  local buf = vim.fn.bufnr(path)
  if buf ~= -1 and vim.api.nvim_buf_is_loaded(buf) then
    return vim.api.nvim_buf_get_lines(buf, from, from + count, false)
  end
  local ok, lines = pcall(vim.fn.readfile, path, "", from + count)
  return ok and vim.list_slice(lines, from + 1) or {}
end

-- "const Rect& cFrame" -> { name = "cFrame", decl = ..., optional = false }
local function param(text)
  local p = vim.trim((text:gsub("%s+", " ")))
  if p == "" or p == "void" or p == "..." then return nil end
  local decl, default = p:match("^(.-)%s*=%s*(.+)$")
  decl = decl or p
  local name = decl:gsub("%[.-%]", ""):match("[%s%*&]([%a_][%w_]*)%s*$")
  return { name = name or decl, decl = p, optional = default ~= nil }
end

-- The function or constructor declared at `loc`: its name and parameters.
local function decl_at(loc)
  local s = loc.range.start
  local text = table.concat(read_lines(vim.uri_to_fname(loc.uri), s.line, 40), "\n")
  text = text:sub(s.character + 1):gsub("/%*.-%*/", " "):gsub("//[^\n]*", " ")
  local open = text:find("(", 1, true)
  if not open then return nil end
  local name = vim.trim(text:sub(1, open - 1))
  local out, cur, depth, quote = {}, {}, 0, nil
  for i = open + 1, #text do
    local c = text:sub(i, i)
    if quote then
      if c == quote and text:sub(i - 1, i - 1) ~= "\\" then quote = nil end
    elseif c == '"' or c == "'" then
      quote = c
    elseif c == "(" or c == "[" or c == "{" or c == "<" then
      depth = depth + 1
    elseif c == ")" and depth == 0 then
      out[#out + 1] = param(table.concat(cur))
      local decls = vim.tbl_map(function(p) return p.decl end, out)
      -- A copy or move constructor (Rect(const Rect&), Rect(Rect&&)) is rarely
      -- the call you're writing; it goes last instead of winning a tie.
      local short = name:match("([%w_]+)%s*$")
      local copy = #out == 1 and short ~= nil and out[1].decl:find("%f[%w_]" .. short .. "%f[^%w_]%s*&") ~= nil
      return { sig = name .. "(" .. table.concat(decls, ", ") .. ")", params = out, copy = copy }
    elseif (c == ")" or c == "]" or c == "}" or c == ">") and depth > 0 then
      depth = depth - 1
    elseif c == "," and depth == 0 then
      out[#out + 1] = param(table.concat(cur))
      cur = {}
      goto continue
    end
    cur[#cur + 1] = c
    ::continue::
  end
end

-- The call's argument list, for the call an error points at: "(...)" or,
-- for brace initialization like "Window w{...}", "{...}".
local ARGS = { argument_list = true, initializer_list = true }
local function argument_list(buf, lnum, col)
  local ok, parser = pcall(vim.treesitter.get_parser, buf)
  if not ok or not parser then return end
  parser:parse()
  local node = vim.treesitter.get_node({ bufnr = buf, pos = { lnum, col }, ignore_injections = true })
  for _ = 1, 6 do
    if not node then return end
    if ARGS[node:type()] then return node end
    for child in node:iter_children() do
      if ARGS[child:type()] then return child end
    end
    node = node:parent()
  end
end

-- The candidates clang lists for an error about a call, best first: those
-- that need more arguments than given (fewest first), then the rest.
local function call_for(buf, d)
  local lsp = d.user_data and d.user_data.lsp or {}
  local have = tonumber(d.message:match("have (%d+)"))
  local cands, seen = {}, {}
  for _, r in ipairs(lsp.relatedInformation or {}) do
    local m = r.message
    if not m:find("implicit") and (m:find("^[Cc]andidate") or m:find("declared here")) then
      local need, given = m:match("requires [%a ]-(%d+) arguments?, but (%d+) w")
      if not need and m:find("requires [%a ]-argument '") then -- "requires at least argument 'pzFormat'"
        need, given = 1, m:match("but (%d+) w") or (m:find("but no arguments") and 0)
      end
      have = have or tonumber(given)
      local c = decl_at(r.location)
      if c and not seen[c.sig] then
        seen[c.sig] = true
        c.need = tonumber(need)
        cands[#cands + 1] = c
      end
    end
  end
  if #cands == 0 then return end
  local args = argument_list(buf, d.lnum, d.col)
  if not args then return end
  if not have then
    have = 0
    for child in args:iter_children() do
      if child:named() and child:type() ~= "comment" then have = have + 1 end
    end
  end
  local function rank(c) return (c.copy and 2000 or 0) + ((c.need and c.need > have) and c.need or 1000 + #c.params) end
  table.sort(cands, function(a, b) return rank(a) < rank(b) end)
  local row, ecol = args:end_()
  return { lnum = d.lnum, row = row, col = ecol - 1, have = have, cands = cands, sel = 1, message = d.message }
end

-- Required parameters the selected candidate still needs.
local function missing_names(call)
  local names = {}
  for i = call.have + 1, #call.cands[call.sel].params do
    local p = call.cands[call.sel].params[i]
    if not p.optional then names[#names + 1] = p.name end
  end
  return names
end

local function draw(buf)
  vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
  local mode = vim.fn.mode()
  if (mode == "i" or mode == "R") and buf == vim.api.nvim_get_current_buf() then return end
  for _, call in pairs(calls[buf] or {}) do
    local names = missing_names(call)
    if #names > 0 then
      local text = (call.have > 0 and ", " or "") .. table.concat(names, ", ")
      pcall(vim.api.nvim_buf_set_extmark, buf, ns, call.row, call.col,
        { virt_text = { { text, "RickMissingArg" } }, virt_text_pos = "inline" })
    end
  end
end

function M.render(buf)
  if not vim.api.nvim_buf_is_valid(buf) then return end
  local old, new = calls[buf] or {}, {}
  for _, d in ipairs(vim.diagnostic.get(buf, { severity = vim.diagnostic.severity.ERROR })) do
    local call = call_for(buf, d)
    if call then
      local prev = old[key(d)]
      if prev and prev.cands[prev.sel] then  -- keep the overload you picked
        for i, c in ipairs(call.cands) do
          if c.sig == prev.cands[prev.sel].sig then call.sel = i end
        end
      end
      new[key(d)] = call
    end
  end
  calls[buf] = new
  draw(buf)
end

local function calls_on(buf, lnum)
  local out = {}
  for _, call in pairs(calls[buf] or {}) do
    if call.lnum == lnum then out[#out + 1] = call end
  end
  return out
end

---------------------------------------------------------------------------
-- The popup: the line's findings one at a time, worst first ( ◀ 1/3 ▶ ), with
-- Copy buttons; for a call, one candidate constructor at a time.

local popup = {}

local function close_popup()
  if popup.win and vim.api.nvim_win_is_valid(popup.win) then pcall(vim.api.nvim_win_close, popup.win, true) end
  if popup.group then pcall(vim.api.nvim_del_augroup_by_id, popup.group) end
  popup = {}
end

local SEV_HL = { "DiagnosticError", "DiagnosticWarn", "DiagnosticInfo", "DiagnosticHint" }

-- The findings the popup steps through: the line's, or one margin column's
-- (1-3, see scanners.column), errors first.
local function findings(buf, lnum, column)
  local diags = vim.diagnostic.get(buf, { lnum = lnum })
  if column then
    local of = require("config.scanners").column
    diags = vim.tbl_filter(function(d) return of(d) == column end, diags)
  end
  table.sort(diags, function(a, b)
    if a.severity ~= b.severity then return a.severity < b.severity end
    return (a.source or "") < (b.source or "")
  end)
  return diags
end

-- The message as the popup shows it; cppcheck and semgrep add the id that suppresses it.
local function message(d)
  return d.message .. (d.user_data and d.user_data.scanner and d.code and (" [" .. d.code .. "]") or "")
end

local function source(d)
  local s = d.source or ""
  return s:match("^clang") and "clangd" or s
end

local function copy(text, what)
  vim.fn.setreg("+", text)
  vim.notify("Copied " .. what)
end

-- An exact edit for the finding, as a function, or nil: semgrep's fix: text
-- over the matched code, or clangd's own fix-it (the first one it sends with
-- the diagnostic; lsp.lua asks for them with codeActionsInline).
local function apply_fix(buf, d)
  local ud = d.user_data or {}
  if ud.replace and type(ud.fix) == "string" then
    local r = ud.replace
    return function()
      local ok = pcall(vim.api.nvim_buf_set_text, buf, r[1], r[2], r[3], r[4], vim.split(ud.fix, "\n"))
      if not ok then vim.notify("The code changed since the scan; save to rescan", vim.log.levels.WARN) end
    end
  end
  local action = ud.lsp and ud.lsp.codeActions and ud.lsp.codeActions[1]
  if action and action.edit then
    local client = vim.lsp.get_clients({ bufnr = buf, name = "clangd" })[1]
    if not client then return nil end
    return function() vim.lsp.util.apply_workspace_edit(action.edit, client.offset_encoding) end
  end
end

-- Lines, highlights and buttons for one finding (page). A button is
-- { row, from, to, fn }: a 0-based row and the byte range you can click.
-- Returns them with the number of findings and the page shown.
local function content(buf, lnum, column, page)
  local diags = findings(buf, lnum, column)
  if #diags == 0 then return {}, {}, {}, 0, 1 end
  page = (page - 1) % #diags + 1
  local d = diags[page]
  local glyphs = require("config.scanners").glyphs()
  local copy_icon = glyphs.copy ~= "" and glyphs.copy .. " " or ""
  local lines, hls, buttons = {}, {}, {}
  local fix = d.user_data and type(d.user_data.fix) == "string" and d.user_data.fix ~= "" and d.user_data.fix
  local checked = d.user_data and d.user_data.checked  -- the note from "// nomodel -- note"
  local severity = checked and d.user_data.severity or d.severity
  if checked then fix = nil end  -- you've decided; a fix would only confuse

  -- The first line: the pager, what found it, the Copy buttons.
  local head = ""
  local function put(text, hl, fn)
    local from = #head
    head = head .. text
    if hl then hls[#hls + 1] = { 0, from, #head, hl } end
    if fn then buttons[#buttons + 1] = { 0, from, #head, fn } end
  end
  if #diags > 1 then
    put("◀", "Special", function() M.page(-1) end)
    put(" " .. page .. "/" .. #diags .. " ", "Special")
    put("▶", "Special", function() M.page(1) end)
    put("  ")
  end
  if checked then
    put(glyphs.checked .. " " .. source(d), "RickChecked")
  else
    put(glyphs.severity[d.severity] .. " " .. source(d), SEV_HL[d.severity])
  end
  put("   ")
  put(copy_icon .. "Copy", "Underlined", function()
    local advice = d.user_data and d.user_data.advice
    copy(source(d) .. ": " .. message(d) .. (advice and ("\nRecommendation: " .. advice) or "")
      .. (fix and ("\nSuggested fix:\n" .. fix) or "")
      .. (checked and ("\nChecked: " .. checked) or ""), "the message")
  end)
  if fix then
    put("  ")
    put(copy_icon .. "Copy fix", "Underlined", function() copy(fix, "the fix") end)
  end
  local apply = not checked and apply_fix(buf, d)
  if apply then
    put("  ")
    put("Apply fix", "Underlined", function() close_popup() apply() end)
  end
  -- A scanner finding (cppcheck, semgrep): the local model can suggest a fix.
  local mcp = package.loaded["config.mcp"]
  if not checked and not fix and d.user_data and d.user_data.scanner and not d.user_data.mcp and mcp and mcp.enabled then
    put("  ")
    put("Ask the model", "Underlined", function() close_popup() mcp.ask_fix(buf, d) end)
  end
  lines[1] = head

  for l in (message(d) .. "\n"):gmatch("(.-)\n") do
    lines[#lines + 1] = l
    hls[#hls + 1] = { #lines - 1, 0, -1, checked and "Comment" or SEV_HL[severity] }
  end
  local advice = not checked and d.user_data and d.user_data.advice
  if advice then  -- cppcheck_advice.lua
    lines[#lines + 1] = "Recommendation:"
    hls[#hls + 1] = { #lines - 1, 0, -1, "Comment" }
    lines[#lines + 1] = "  " .. advice
  end
  if fix then  -- the local model's suggestion
    lines[#lines + 1] = "Suggested fix:"
    hls[#hls + 1] = { #lines - 1, 0, -1, "Comment" }
    for l in (fix:gsub("\t", "    ") .. "\n"):gmatch("(.-)\n") do lines[#lines + 1] = "  " .. l end
  end
  if checked then
    lines[#lines + 1] = "Checked: " .. checked
    hls[#hls + 1] = { #lines - 1, 0, -1, "RickChecked" }
  end
  for _, call in ipairs(calls_on(buf, lnum)) do
    if call.message == d.message then
      local n = #call.cands
      local row = #lines
      local nav = ""
      if n > 1 then
        nav = "◀ " .. call.sel .. "/" .. n .. " ▶  "
        buttons[#buttons + 1] = { row, 0, #"◀", function() M.cycle(-1) end }
        local right = nav:find("▶", 1, true)
        buttons[#buttons + 1] = { row, right - 1, right - 1 + #"▶", function() M.cycle(1) end }
        hls[#hls + 1] = { row, 0, #nav, "Special" }
      end
      lines[#lines + 1] = nav .. call.cands[call.sel].sig
      local names = missing_names(call)
      lines[#lines + 1] = #names > 0 and ("Missing: " .. table.concat(names, ", ")) or "Nothing missing"
      hls[#hls + 1] = { #lines - 1, 0, -1, #names > 0 and "RickMissingArg" or "Comment" }
    end
  end
  return lines, hls, buttons, #diags, page
end

local function fill(lines, hls)
  vim.api.nvim_buf_set_lines(popup.buf, 0, -1, false, lines)
  vim.api.nvim_buf_clear_namespace(popup.buf, ns, 0, -1)
  for _, h in ipairs(hls) do
    pcall(vim.api.nvim_buf_set_extmark, popup.buf, ns, h[1], h[2],
      { end_col = h[3] >= 0 and h[3] or #lines[h[1] + 1], hl_group = h[4] })
  end
  local width = 10
  for _, l in ipairs(lines) do width = math.max(width, vim.fn.strdisplaywidth(l)) end
  width = math.min(width, math.max(40, math.floor(vim.o.columns * 0.6)))
  local height = 0
  for _, l in ipairs(lines) do height = height + math.max(1, math.ceil(vim.fn.strdisplaywidth(l) / width)) end
  return width, math.min(height, math.max(3, vim.o.lines - 6)) -- taller than the screen: scroll inside it
end

-- Redraw the open popup (another page, another candidate).
local function refresh()
  if not (popup.win and vim.api.nvim_win_is_valid(popup.win)) then return end
  local lines, hls, buttons, n, page = content(popup.src, popup.lnum, popup.column, popup.page)
  if n == 0 then return close_popup() end
  popup.buttons, popup.count, popup.page = buttons, n, page
  local width, height = fill(lines, hls)
  vim.api.nvim_win_set_config(popup.win, { width = width, height = height })
end

-- Show the popup for a line; relative is "cursor" or "mouse". column limits
-- it to one margin column; without it, the popup steps through everything on
-- the line. focus moves into it, so the arrow keys work right away.
local function open_popup(buf, lnum, relative, column, focus)
  close_popup()
  local lines, hls, buttons, n = content(buf, lnum, column, 1)
  if n == 0 then return end
  popup = { buf = vim.api.nvim_create_buf(false, true), src = buf, lnum = lnum, relative = relative,
    column = column, buttons = buttons, count = n, page = 1 }
  local width, height = fill(lines, hls)
  popup.win = vim.api.nvim_open_win(popup.buf, focus or false, {
    relative = relative, row = 1, col = 0, width = width, height = height,
    style = "minimal", border = "rounded", focusable = true, zindex = 60,
  })
  vim.wo[popup.win].wrap = true
  vim.wo[popup.win].linebreak = true -- wrap at word boundaries
  -- Selecting in the popup: a mouse drag copies what it covers; y copies too.
  local pbuf = popup.buf
  local typing = vim.fn.mode() == "i"
  local function back()
    local src_win = vim.fn.bufwinid(buf)
    close_popup()
    if src_win ~= -1 then pcall(vim.api.nvim_set_current_win, src_win) end
    if typing then vim.cmd.startinsert() end -- back to typing where you were
  end
  local function copied() vim.notify("Copied") end
  vim.keymap.set("x", "<LeftRelease>", function()
    vim.cmd('normal! "+y')
    copied()
  end, { buffer = pbuf })
  vim.keymap.set("x", "y", function()
    vim.cmd('normal! "+y')
    copied()
  end, { buffer = pbuf })
  vim.keymap.set("n", "yy", function()
    vim.cmd('normal! "+yy')
    copied()
  end, { buffer = pbuf })
  vim.keymap.set("n", "q", back, { buffer = pbuf })
  vim.keymap.set("n", "<Esc>", back, { buffer = pbuf })
  vim.keymap.set("n", "<Left>", function() M.page(-1) end, { buffer = pbuf })
  vim.keymap.set("n", "<Right>", function() M.page(1) end, { buffer = pbuf })
  -- Close when you move on, unless you're in the popup.
  popup.group = vim.api.nvim_create_augroup("rick_diag_popup", { clear = true })
  local win = popup.win
  vim.api.nvim_create_autocmd({ "CursorMoved", "CursorMovedI", "InsertEnter", "BufLeave", "WinLeave" }, {
    group = popup.group,
    callback = function()
      vim.schedule(function()
        if popup.win == win and vim.api.nvim_get_current_win() ~= win then close_popup() end
      end)
    end,
  })
  -- Clicking into it while typing: stop typing there (it's for reading and copying).
  vim.api.nvim_create_autocmd("WinEnter", {
    group = popup.group,
    callback = function()
      if vim.api.nvim_get_current_win() == win and vim.fn.mode() == "i" then vim.cmd.stopinsert() end
    end,
  })
end

-- Show the next/previous finding in the open popup.
function M.page(step)
  if not popup.win then return false end
  popup.page = popup.page + step
  refresh()
  return true
end

-- Step every call on the popup's line (or the cursor line) to the next/previous candidate.
function M.cycle(step)
  local buf, lnum = popup.src, popup.lnum
  if not buf then buf, lnum = vim.api.nvim_get_current_buf(), vim.api.nvim_win_get_cursor(0)[1] - 1 end
  local any = false
  for _, call in ipairs(calls_on(buf, lnum)) do
    call.sel = (call.sel - 1 + step) % #call.cands + 1
    any = true
  end
  if not any then return false end
  draw(buf)
  if popup.win and vim.api.nvim_win_is_valid(popup.win) then
    refresh()
  else
    open_popup(buf, lnum, "cursor")
  end
  return true
end

-- For Cmd+P: whether the cursor line has a call to show, and showing it.
-- Space cd passes focus, so the arrow keys step through the findings.
function M.has_call(buf, lnum) return #calls_on(buf, lnum) > 0 end
function M.show_cursor(focus)
  open_popup(vim.api.nvim_get_current_buf(), vim.api.nvim_win_get_cursor(0)[1] - 1, "cursor", nil, focus)
end

-- Whether the mouse is over the popup (its border included).
local function mouse_in_popup(m)
  if not (popup.win and vim.api.nvim_win_is_valid(popup.win)) then return false end
  local pos = vim.api.nvim_win_get_position(popup.win) -- of the border's top-left corner
  local w, h = vim.api.nvim_win_get_width(popup.win), vim.api.nvim_win_get_height(popup.win)
  local r, c = m.screenrow - 2 - pos[1], m.screencol - 2 - pos[2]
  return r >= -1 and c >= -1 and r <= h and c <= w
end

-- The popup button under the mouse, if any.
local function button_at(m)
  if m.winid ~= popup.win or m.line < 1 then return end
  for _, b in ipairs(popup.buttons or {}) do
    if m.line - 1 == b[1] and m.column - 1 >= b[2] and m.column - 1 < b[3] then return b end
  end
end


---------------------------------------------------------------------------
-- Mouse: rest it on a symbol in the margin for a moment to see that column's
-- findings; over the popup, the wheel or the arrows step through them.

local hover = { timer = vim.uv.new_timer() }

-- Which margin column (1-3, as scanners.lua draws them) the mouse is over,
-- or nil when it's elsewhere. The signs sit left of the line numbers, two
-- cells each, after the fold column if there is one.
local function sign_column(m)
  local info = vim.fn.getwininfo(m.winid)[1]
  if not info or m.wincol > info.textoff then return end
  local numbers = 0
  if vim.wo[m.winid].number or vim.wo[m.winid].relativenumber then
    local lines = vim.api.nvim_buf_line_count(vim.api.nvim_win_get_buf(m.winid))
    numbers = math.max(vim.wo[m.winid].numberwidth, #tostring(lines) + 1)
  end
  local sc = vim.wo[m.winid].signcolumn
  local signs = sc:match("^no") and 0 or 2 * (tonumber(sc:match(":(%d+)")) or 1)
  local x = m.wincol - (info.textoff - numbers - signs) -- 1-based within the signs
  if x < 1 or x > signs then return end
  local col = math.floor((x - 1) / 2) + 1
  return col <= 3 and col or nil
end

local function on_mouse_move()
  if require("config.dialogs").busy() then return end -- never over Find or another dialog
  local m = vim.fn.getmousepos()
  if mouse_in_popup(m) then return end
  local buf = m.winid ~= 0 and m.line > 0 and vim.api.nvim_win_is_valid(m.winid)
    and vim.api.nvim_win_get_buf(m.winid)
  local column = buf and sign_column(m)
  if buf and (vim.bo[buf].buftype ~= "" or not column) then buf = nil end
  local at = buf and (buf .. ":" .. (m.line - 1) .. ":" .. column)
  if at == hover.at then return end
  hover.at = at
  hover.timer:stop()
  -- Moving off the margin closes a popup the mouse opened (not one you've clicked into).
  if popup.relative == "mouse" and vim.api.nvim_get_current_win() ~= popup.win then close_popup() end
  if not buf then return end
  hover.timer:start(require("config.prefs").get("hover_delay"), 0, vim.schedule_wrap(function()
    local now = vim.fn.getmousepos()
    local mode = vim.fn.mode() -- normal or insert (typing): a menu or a selection is busy
    if now.winid ~= m.winid or now.line ~= m.line or sign_column(now) ~= column or (mode ~= "n" and mode ~= "i") then return end
    if popup.win and popup.src == buf and popup.lnum == m.line - 1 and popup.column == column then return end
    open_popup(buf, m.line - 1, "mouse", column)
  end))
end

local WHEEL_DOWN, WHEEL_UP = vim.keycode("<ScrollWheelDown>"), vim.keycode("<ScrollWheelUp>")
local CLICK, RELEASE = vim.keycode("<LeftMouse>"), vim.keycode("<LeftRelease>")
local MOUSE_MOVE = vim.keycode("<MouseMove>")

-- Over the popup, a click on a button (the arrows, Copy) does what it says,
-- and the mouse wheel steps through the findings (or, with one finding, a
-- call's candidates) instead of scrolling or clicking the text below it.
local function on_key(k)
  if k == MOUSE_MOVE then
    vim.schedule(on_mouse_move)
    return
  end
  if k ~= WHEEL_DOWN and k ~= WHEEL_UP and k ~= CLICK and k ~= RELEASE then return end
  local m = vim.fn.getmousepos()
  if not mouse_in_popup(m) then return end
  if k == CLICK then
    local b = button_at(m)
    if b then
      vim.schedule(b[4])
      popup.ate_click = true
      return ""
    end
    -- Anywhere else: let the click through, so it moves into the popup for selecting.
  elseif k == RELEASE then
    if popup.ate_click then
      popup.ate_click = nil
      return ""
    end
  else
    local step = k == WHEEL_DOWN and 1 or -1
    if (popup.count or 0) > 1 then
      vim.schedule(function() M.page(step) end)
      return ""
    elseif M.has_call(popup.src, popup.lnum) then
      vim.schedule(function() M.cycle(step) end)
      return ""
    end
  end
end

function M.setup()
  vim.api.nvim_set_hl(0, "RickMissingArg", { link = "DiagnosticError", default = true })
  vim.api.nvim_set_hl(0, "RickChecked", { link = "DiagnosticOk", default = true }) -- green

  local group = vim.api.nvim_create_augroup("rick_diagnostics", { clear = true })
  vim.api.nvim_create_autocmd("DiagnosticChanged", {
    group = group,
    callback = function(ev) vim.schedule(function() M.render(ev.buf) end) end,
  })
  vim.api.nvim_create_autocmd({ "InsertEnter", "InsertLeave" }, {
    group = group,
    callback = function(ev) vim.schedule(function() if vim.api.nvim_buf_is_valid(ev.buf) then draw(ev.buf) end end) end,
  })
  vim.keymap.set("n", "]o", function() M.cycle(1) end, { desc = "Next constructor in the hint" })
  vim.keymap.set("n", "[o", function() M.cycle(-1) end, { desc = "Previous constructor in the hint" })

  -- Mouse moves are watched in on_key rather than mapped: a mapped
  -- <MouseMove> closes right-click menus as soon as the mouse moves.
  vim.o.mousemoveevent = true
  vim.on_key(on_key, vim.api.nvim_create_namespace("rick_diag_popup_keys"))
end

-- The project's problems (Space fd, Show Problems in the project tree) in a
-- popup over the code: the list on top, the spot it points at below.
function M.picker(opts)
  Snacks.picker.diagnostics(vim.tbl_deep_extend("force", {
    layout = {
      layout = {
        backdrop = false, row = 2, width = 0.6, min_width = 80, max_width = 140, height = 0.6,
        box = "vertical", border = "rounded", title = " Problems ", title_pos = "center",
        { win = "input", height = 1, border = "bottom" },
        { win = "list", border = "none" },
        { win = "preview", title = "{preview}", height = 0.45, border = "top" },
      },
    },
  }, opts or {}))
end

-- Configure Diagnostics (right-click menu): a popup of the checks you can turn
-- on and off; each runs its Space t key, which says what it switched to.
function M.configure()
  local checks = {
    { "cppcheck", "<leader>tc" }, { "semgrep", "<leader>ts" },
    { "Local model checks", "<leader>tm" }, { "Inlay hints (clangd)", "<leader>th" },
  }
  vim.ui.select(checks, {
    prompt = "Turn on / off",
    format_item = function(c) return c[1] .. "  (Space " .. c[2]:sub(-2) .. ")" end,
  }, function(c)
    if c then vim.api.nvim_feedkeys(vim.keycode(c[2]), "m", false) end
  end)
end

M.close_popup = close_popup

return M
