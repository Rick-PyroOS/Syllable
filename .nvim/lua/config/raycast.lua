-- Lets the Raycast extension in raycast/ drive this Neovim:
-- list and run actions (every mapping with a description, plus commands,
-- with arguments if you give them, or any command line you type),
-- switch tabs, and open files, like Raycast's Search Menu Items.
--
-- Each Neovim listens on a socket in ~/.local/state/nvim/raycast/<pid>.sock.
-- Raycast calls the functions below with
--   nvim --server <sock> --remote-expr "luaeval(\"require'config.raycast'.run(_A)\", json_decode('{...}'))"
-- The list functions return JSON; the run functions return at once and do
-- their work on the next tick, so Raycast never waits on the editor.

local M = {}

local dir = vim.fn.stdpath("state") .. "/raycast"
local sock = dir .. "/" .. vim.fn.getpid() .. ".sock"

local focused = os.time() -- last time this Neovim had focus; Raycast picks the latest

function M.setup()
  vim.fn.mkdir(dir, "p")
  pcall(vim.fn.serverstart, sock)
  local group = vim.api.nvim_create_augroup("rick_raycast", { clear = true })
  vim.api.nvim_create_autocmd("FocusGained", { group = group, callback = function() focused = os.time() end })
  vim.api.nvim_create_autocmd("VimLeavePre", {
    group = group,
    callback = function() pcall(vim.fn.serverstop, sock); os.remove(sock) end,
  })
end

-- The window files should open in: the current one if it holds a normal
-- buffer, else the last such window (not the tree, a picker or a float).
local function editor_win()
  local function ok(w)
    return vim.api.nvim_win_is_valid(w) and vim.api.nvim_win_get_config(w).relative == ""
      and vim.bo[vim.api.nvim_win_get_buf(w)].buftype == ""
  end
  if ok(vim.api.nvim_get_current_win()) then return vim.api.nvim_get_current_win() end
  local prev = vim.fn.win_getid(vim.fn.winnr("#"))
  if prev ~= 0 and ok(prev) then return prev end
  for _, w in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
    if ok(w) then return w end
  end
end

-- Keys go in as if typed (nvim_input), after anything already typed, so
-- they run in order: leave insert mode first, then the command.
local function type_keys(keys) vim.api.nvim_input(keys) end

-- Where you were typing when an action came in (insert mode), so run() can
-- put you back there afterwards.
local typing
local NS = vim.api.nvim_create_namespace("rick_raycast_typing")

-- Leave insert/visual/command-line mode and any picker before acting.
local function to_editor()
  local mode = vim.fn.mode()
  local in_file = vim.bo.buftype == "" and vim.api.nvim_win_get_config(0).relative == ""
  if typing then pcall(vim.api.nvim_buf_del_extmark, typing.buf, NS, typing.mark) end
  typing = nil
  if (mode == "i" or mode == "R") and in_file then
    -- An extmark follows the spot through edits the action makes (format on save).
    local pos = vim.api.nvim_win_get_cursor(0)
    typing = { buf = vim.api.nvim_get_current_buf(),
      mark = vim.api.nvim_buf_set_extmark(0, NS, pos[1] - 1, pos[2], {}) }
  end
  if mode ~= "n" then type_keys("<C-\\><C-n>") end
  for _, p in ipairs(Snacks and Snacks.picker.get() or {}) do
    if p.opts.source ~= "explorer" then p:close() end
  end
  local w = editor_win()
  if w then vim.api.nvim_set_current_win(w) end
end

local function later(fn)
  vim.schedule(function()
    local ok, err = pcall(fn)
    if not ok then vim.notify("Raycast: " .. tostring(err), vim.log.levels.ERROR) end
  end)
  return "ok"
end

-- api is bumped when the Raycast side needs something newer, so it can say
-- to restart Neovim.
function M.info()
  local w = editor_win()
  local name = w and vim.api.nvim_buf_get_name(vim.api.nvim_win_get_buf(w)) or ""
  return vim.json.encode({ api = 2, pid = vim.fn.getpid(), cwd = vim.fn.getcwd(), file = name, focused = focused })
end

-- Built-in commands worth reaching from Raycast; Neovim has no list of these
-- to read. nargs as in :command ("?" optional, "1" one, "+" one or more);
-- true when the argument is a file name, so Raycast offers the project's files.
local BUILTIN = {
  { "write", "?", "Save; with a name, save a copy under that name", true },
  { "saveas", "1", "Save as a new name and keep editing that file", true },
  { "wall", "0", "Save all" },
  { "edit", "?", "Open a file", true },
  { "vsplit", "?", "Open a file in a split on the right", true },
  { "split", "?", "Open a file in a split below", true },
  { "read", "1", "Insert a file's text below the cursor", true },
  { "cd", "?", "Change the working folder" },
  { "help", "?", "Open help on a topic" },
  { "set", "+", "Change an option" },
}

function M.actions()
  local items = {}
  local w = editor_win()
  local buf = w and vim.api.nvim_win_get_buf(w) or 0
  vim.api.nvim_buf_call(buf, function()   -- includes that file's LSP keys
    for _, a in ipairs(require("config.cheatsheet").actions()) do
      table.insert(items, { kind = "map", title = a.desc, keys = a.keys, lhs = a.lhs })
    end
  end)
  for name, c in pairs(vim.api.nvim_get_commands({})) do
    table.insert(items, { kind = "cmd", title = ":" .. name, cmd = name, nargs = c.nargs,
      detail = ((c.definition or ""):gsub("%s+", " ")):sub(1, 80) })
  end
  for _, b in ipairs(BUILTIN) do
    table.insert(items, { kind = "cmd", title = ":" .. b[1], cmd = b[1], nargs = b[2], detail = b[3],
      file = b[4] or nil })
  end
  return vim.json.encode(items)
end

function M.buffers()
  local cur = editor_win()
  cur = cur and vim.api.nvim_win_get_buf(cur)
  local list = {}
  for _, b in ipairs(vim.api.nvim_list_bufs()) do
    local name = vim.api.nvim_buf_get_name(b)
    if vim.bo[b].buflisted and vim.bo[b].buftype == "" and name ~= "" then
      table.insert(list, { buf = b, path = name, modified = vim.bo[b].modified, current = b == cur,
        lastused = vim.fn.getbufinfo(b)[1].lastused })
    end
  end
  return vim.json.encode(list)
end

-- After an action that came in while you were typing: back to insert mode,
-- where you left off if the action didn't move you, else at the cursor. Not
-- when the action left a picker, menu, prompt or : line open.
function M.resume()
  local was = typing
  typing = nil
  if not was or vim.fn.mode() ~= "n" then return end
  local win = vim.api.nvim_get_current_win()
  if vim.api.nvim_win_get_config(win).relative ~= "" or vim.bo.buftype ~= "" or not vim.bo.modifiable then return end
  local pos = vim.api.nvim_win_get_cursor(win)
  local mark = vim.api.nvim_buf_is_valid(was.buf) and vim.api.nvim_buf_get_extmark_by_id(was.buf, NS, was.mark, {}) or {}
  pcall(vim.api.nvim_buf_del_extmark, was.buf, NS, was.mark)
  local moved = vim.api.nvim_get_current_buf() ~= was.buf or not mark[1]
    or pos[1] ~= mark[1] + 1 or math.abs(pos[2] - mark[2]) > 1
  if not moved then -- the exact spot, including the end of the line
    local row, col = mark[1] + 1, mark[2]
    local len = #vim.api.nvim_buf_get_lines(0, row - 1, row, false)[1]
    if col >= len and len > 0 then
      vim.api.nvim_win_set_cursor(win, { row, len - 1 })
      return vim.api.nvim_feedkeys("a", "n", false)
    end
    vim.api.nvim_win_set_cursor(win, { row, col })
  end
  vim.api.nvim_feedkeys("i", "n", false)
end
local RESUME = "<Cmd>lua require('config.raycast').resume()<CR>"

-- Type a line on the : command line and press Enter, so it behaves exactly
-- as typed (":q" still asks about unsaved files, errors show in Neovim).
local function ex(line)
  type_keys(":" .. line:gsub("<", "<lt>") .. "<CR>" .. RESUME)
end

-- Run a mapping by its keys, a command (with item.args if given; without,
-- one that takes arguments is left on the : line for you to finish), or a
-- whole command line (kind "ex").
function M.run(item)
  return later(function()
    to_editor()
    if item.kind == "ex" then
      ex(item.line)
    elseif item.kind == "cmd" then
      if item.args then ex(item.cmd .. (item.args ~= "" and " " .. item.args or ""))
      elseif item.nargs == "0" then ex(item.cmd)
      else type_keys(":" .. item.cmd .. "<Space>") end
    else
      type_keys(item.lhs .. RESUME)
    end
  end)
end

function M.switch(buf)
  return later(function()
    to_editor()
    vim.api.nvim_set_current_buf(buf)
  end)
end

-- { path = "...", split = true } opens it in a split on the right.
function M.open(arg)
  return later(function()
    to_editor()
    vim.cmd((arg.split and "vsplit " or "edit ") .. vim.fn.fnameescape(arg.path))
  end)
end

return M
