-- Show Usages, like CLion's Cmd+Alt+F7: a small list, right under the name
-- under the cursor, of every place that function, class, variable or macro
-- is used in the project (clangd finds them). Type to filter; arrows and
-- Enter, or a click, jump there; Esc closes. If the only usage is somewhere
-- else it jumps straight there, and with none it says so.
--
-- It opens from Cmd+Alt+F7 (with the iTerm profile), Space cu, Show Usages
-- in the right-click menu, and Ctrl+click on a name where it's declared or
-- defined. Ctrl+click on a use of a name still goes to its definition, as in
-- CLion. Alt+F7 (Find Usages) is the bigger list with a preview.

local M = {}

local function real(path)
  return vim.uv.fs_realpath(path) or vim.fs.normalize(path)
end

-- textDocument/references for the cursor, as quickfix-style items sorted by
-- file and line; cb(nil) when no server here can find references.
local function references(include_declaration, cb)
  local buf, win = vim.api.nvim_get_current_buf(), vim.api.nvim_get_current_win()
  if #vim.lsp.get_clients({ bufnr = buf, method = "textDocument/references" }) == 0 then return cb(nil) end
  vim.lsp.buf_request_all(buf, "textDocument/references", function(client)
    local params = vim.lsp.util.make_position_params(win, client.offset_encoding)
    params.context = { includeDeclaration = include_declaration }
    return params
  end, function(results)
    local items, seen = {}, {}
    for id, res in pairs(results) do
      local client = vim.lsp.get_client_by_id(id)
      if client and type(res.result) == "table" then
        for _, it in ipairs(vim.lsp.util.locations_to_items(res.result, client.offset_encoding)) do
          it.filename = real(it.filename)
          local key = it.filename .. ":" .. it.lnum .. ":" .. it.col
          if not seen[key] then
            seen[key] = true
            items[#items + 1] = it
          end
        end
      end
    end
    table.sort(items, function(a, b)
      if a.filename ~= b.filename then return a.filename < b.filename end
      if a.lnum ~= b.lnum then return a.lnum < b.lnum end
      return a.col < b.col
    end)
    cb(items)
  end)
end

-- The cursor, and the name under it with the column it starts at (1-based).
local function here()
  local pos = vim.api.nvim_win_get_cursor(0)
  local line = vim.api.nvim_get_current_line()
  local start = pos[2] + 1
  while start > 1 and line:sub(start - 1, start - 1):match("[%w_]") do start = start - 1 end
  return {
    file = real(vim.api.nvim_buf_get_name(0)),
    lnum = pos[1],
    col = pos[2], -- 0-based byte
    start = start,
    name = vim.fn.expand("<cword>"),
  }
end

-- Whether one of the items is the name under the cursor.
local function covers(items, h)
  for _, it in ipairs(items) do
    local stop = (it.end_lnum == it.lnum and it.end_col and it.end_col > it.col) and it.end_col - 1 or it.col
    if it.filename == h.file and it.lnum == h.lnum and h.col >= it.col - 1 and h.col < stop then return true end
  end
  return false
end

-- Where the popup goes: just under the name (above it if there's no room),
-- in screen cells (0-based, the border's corner).
local function layout(count, h, want)
  local pos = vim.fn.screenpos(0, h.lnum, h.start)
  local width = math.max(50, math.min(want, vim.o.columns - 2))
  local list = math.min(count, 12)
  local height = list + 4 -- the borders, the filter line and the line under it
  local bottom = vim.o.lines - vim.o.cmdheight
  local row = pos.row -- the line under the name
  if row + height > bottom and pos.row - 1 - height >= 0 then row = pos.row - 1 - height end
  local col = math.max(0, math.min(pos.col - 2, vim.o.columns - width))
  return {
    hidden = { "preview" },
    layout = {
      backdrop = false,
      row = row,
      col = col,
      width = width - 2,
      height = height - 2,
      box = "vertical",
      border = "rounded",
      title = "{title}",
      title_pos = "left",
      { win = "input", height = 1, border = "bottom" },
      { win = "list", border = "none" },
    },
  }
end

-- Fit text into width cells: cut at the end (code) or the start (paths) with "…".
local function fit(text, width, from_start)
  if vim.api.nvim_strwidth(text) <= width then return text end
  if width <= 1 then return "…" end
  local chars = vim.fn.split(text, "\\zs")
  local out, used = {}, 1
  for i = from_start and #chars or 1, from_start and 1 or #chars, from_start and -1 or 1 do
    local w = vim.api.nvim_strwidth(chars[i])
    if used + w > width then break end
    used = used + w
    table.insert(out, from_start and 1 or #out + 1, chars[i])
  end
  local cut = table.concat(out)
  return from_start and "…" .. cut or cut .. "…"
end

local function pad(text, width)
  return text .. string.rep(" ", math.max(0, width - vim.api.nvim_strwidth(text)))
end

-- The full path shown while the mouse rests on a cut-off folder.
local tip = { timer = vim.uv.new_timer() }

local function close_tip()
  tip.timer:stop()
  if tip.win and vim.api.nvim_win_is_valid(tip.win) then pcall(vim.api.nvim_win_close, tip.win, true) end
  tip.win, tip.item = nil, nil
end

local function show_tip(item, m)
  close_tip()
  local text = " " .. item.u_path .. " "
  local buf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, { text })
  vim.bo[buf].bufhidden = "wipe"
  local width = vim.api.nvim_strwidth(text)
  tip.item = item
  tip.win = vim.api.nvim_open_win(buf, false, {
    relative = "editor", row = m.screenrow, col = math.max(0, math.min(m.screencol - 2, vim.o.columns - width - 2)),
    width = width, height = 1, style = "minimal", border = "rounded",
    focusable = false, mouse = false, zindex = 250, noautocmd = true,
  })
end

-- Mouse moves while the popup is open: over a cut-off folder, its full path
-- after a moment; anywhere else, no tip.
local function on_mouse_move()
  local c = tip.cols
  local picker = c and c.picker
  local win = picker and not picker.closed and picker.list and picker.list.win and picker.list.win.win
  if not win or not vim.api.nvim_win_is_valid(win) then return close_tip() end
  local m = vim.fn.getmousepos()
  local item
  if m.winid == win and m.wincol - 1 >= c.from and m.wincol - 1 < c.from + c.width then
    item = picker.list:get(picker.list:row2idx(m.winrow))
    if item and vim.api.nvim_strwidth(item.u_dir) <= c.width then item = nil end -- shown in full already
  end
  if item and item == tip.item then return end
  close_tip()
  if not item then return end
  tip.item = item
  tip.timer:start(require("config.prefs").get("hover_delay"), 0, vim.schedule_wrap(function()
    if tip.item == item and tip.cols and tip.cols.picker == picker and not picker.closed then show_tip(item, m) end
  end))
end

-- Rows in three columns: the file and line, its folder (cut at the start to
-- keep the popup narrow; rest the mouse on it for the whole path), and the
-- code there.
local DIR_MAX, CODE_MAX = 30, 70
local function popup(items, h)
  local cwd = vim.uv.cwd()
  local list, w_file, w_dir, w_code = {}, 0, 0, 0
  for _, it in ipairs(items) do
    local rel = vim.fs.relpath and vim.fs.relpath(cwd, it.filename) or nil
    local path = rel or vim.fn.fnamemodify(it.filename, ":~")
    local dir = vim.fs.dirname(path)
    if dir == "." then dir = "" end
    local name = vim.fs.basename(it.filename)
    local raw = it.text or ""
    local lead = #raw - #raw:gsub("^%s+", "")
    local code = vim.trim(raw):gsub("\t", " ") -- a tab for a space keeps byte columns
    local item = {
      file = it.filename,
      pos = { it.lnum, it.col - 1 },
      end_pos = it.end_lnum and { it.end_lnum, it.end_col - 1 } or nil,
      text = name .. " " .. dir .. " " .. code,
      u_name = name,
      u_lnum = tostring(it.lnum),
      u_dir = dir,
      u_path = path,
      u_code = code,
      u_hl = { it.col - 1 - lead, (it.end_lnum == it.lnum and it.end_col or it.col + #h.name) - 1 - lead },
    }
    list[#list + 1] = item
    w_file = math.max(w_file, vim.api.nvim_strwidth(name .. ":" .. item.u_lnum) + 2)
    w_dir = math.max(w_dir, vim.api.nvim_strwidth(dir))
    w_code = math.max(w_code, vim.api.nvim_strwidth(code))
  end
  local only_elsewhere = #items == 1 and not covers(items, h)
  local word = #items == 1 and "usage" or "usages"
  local title = ("Usages of %s · %d %s"):format(h.name, #items, word)

  -- Columns: file, folder, code, with two spaces between them. The folder
  -- is at most DIR_MAX cells and the code CODE_MAX (less on a narrow screen).
  local GAP = 2
  local room = vim.o.columns - 2 - 4 -- the borders, and cells the list keeps free
  w_dir = math.min(w_dir, DIR_MAX, math.max(10, room - w_file - 2 * GAP - 20))
  w_code = math.max(10, math.min(w_code, CODE_MAX, room - w_file - w_dir - 2 * GAP))
  local want = math.max(w_file + w_dir + w_code + 2 * GAP + 4, vim.api.nvim_strwidth(title) + 8)

  local function format(item, picker)
    local ret = {}
    local icon, icon_hl = Snacks.util.icon(item.u_name, "file")
    local file = item.u_name .. ":" .. item.u_lnum
    ret[#ret + 1] = { pad(icon or " ", 2), icon_hl or "SnacksPickerIcon" }
    ret[#ret + 1] = { item.u_name, "SnacksPickerFile" }
    ret[#ret + 1] = { ":" .. item.u_lnum, "SnacksPickerDimmed" }
    ret[#ret + 1] = { string.rep(" ", w_file - 2 - vim.api.nvim_strwidth(file) + GAP) }
    ret[#ret + 1] = { pad(fit(item.u_dir, w_dir, true), w_dir + GAP), "SnacksPickerDir" }
    local code = fit(item.u_code, w_code)
    local offset = Snacks.picker.highlight.offset(ret)
    Snacks.picker.highlight.format(item, code, ret)
    local s, e = item.u_hl[1], math.min(item.u_hl[2], #code)
    if s >= 0 and e > s then
      ret[#ret + 1] = { col = offset + s, end_col = offset + e, hl_group = "SnacksPickerMatch" }
    end
    return ret
  end

  tip.cols = { from = w_file + GAP, width = w_dir }
  tip.cols.picker = Snacks.picker.pick({
    source = "show_usages",
    title = title,
    items = list,
    format = format,
    auto_confirm = only_elsewhere,
    jump = { tagstack = true, reuse_win = true },
    layout = layout(#items, h, want),
    on_close = function() close_tip(); tip.cols = nil end,
    win = {
      list = {
        keys = { ["<LeftRelease>"] = "confirm" }, -- one click jumps
        wo = { wrap = false },
      },
    },
  })
end

local function none(name)
  vim.notify(("No usages of %s"):format(name ~= "" and name or "this"), vim.log.levels.INFO, { title = "Show Usages" })
end

-- Cmd+Alt+F7, Space cu, the right-click menu.
function M.show()
  local h = here()
  references(false, function(items)
    if not items then
      vim.notify("No language server for this file", vim.log.levels.WARN, { title = "Show Usages" })
    elseif #items == 0 then
      none(h.name)
    else
      popup(items, h)
    end
  end)
end

-- Ctrl+click (the click has already moved the cursor): on the name where
-- it's declared or defined, its usages; anywhere else, go to the definition.
function M.ctrl_click()
  local h = here()
  references(true, function(all)
    if not all or not covers(all, h) then return require("config.define").definition() end
    references(false, function(uses)
      if not uses or covers(uses, h) then return require("config.define").definition() end -- a use, not a declaration
      if #uses == 0 then return none(h.name) end
      popup(uses, h)
    end)
  end)
end

function M.setup()
  local MOUSE_MOVE = vim.keycode("<MouseMove>")
  vim.on_key(function(k, typed)
    k = typed and typed ~= "" and typed or k
    if k == MOUSE_MOVE and tip.cols then vim.schedule(on_mouse_move) end
  end, vim.api.nvim_create_namespace("rick_usages_tip"))
  -- Right-click menu: Show Usages right after Go to definition. Menu entries
  -- with the same priority keep the order they were added in, so those two
  -- (and Open in web browser above them) are re-added just ahead of the rest.
  pcall(vim.cmd, [[aunmenu PopUp.Open\ in\ web\ browser]])
  pcall(vim.cmd, [[aunmenu PopUp.Go\ to\ definition]])
  vim.cmd([[
    amenu      .497 PopUp.Open\ in\ web\ browser  gx
    anoremenu  .498 PopUp.Go\ to\ definition      <Cmd>lua require('config.define').definition()<CR>
    anoremenu  .499 PopUp.Show\ Usages            <Cmd>lua require('config.usages').show()<CR>
  ]])
  vim.api.nvim_create_autocmd("MenuPopup", {
    group = vim.api.nvim_create_augroup("rick_usages_menu", { clear = true }),
    callback = function()
      local lsp = #vim.lsp.get_clients({ bufnr = 0, method = "textDocument/references" }) > 0
      vim.cmd(lsp and [[anoremenu enable PopUp.Show\ Usages]] or [[anoremenu disable PopUp.Show\ Usages]])
    end,
  })
end

return M
