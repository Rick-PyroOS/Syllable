-- Editor preferences you can flip from the command palette (Space Space) or
-- with their Space t keys. Each is remembered across restarts
-- (stdpath("state")/prefs.json).
--   Space tw  word wrap                                 (off)
--   Space tz  Ctrl+Z suspends Neovim                    (off: Ctrl+Z does nothing)
--   Space td  double-click selects the word (triple: the line) (off: it just places the cursor)
--   Space tk  popup listing the keys after Space, " or ' (off; Space Space still works)
--   Space tq  quick documentation when the mouse rests on a name (on)
--   Space tx  sticky header: the function you're in stays at the top (on)
--   Space tg  git blame (author, date, commit) at the end of the current line (off)
--   Space ti  accepting a completion adds the #include it needs (on)
--   Space tr  breadcrumbs: folders and class > function at the top (on)
-- Settings (Cmd+, or Space T) lists every Space t toggle in one popup, each
-- with its on/off, plus the hover delay (how long the mouse rests before a
-- popup shows: quick documentation, the margin's findings, tab and usage tips;
-- 500 ms like CLion). Enter on it picks from a list; :HoverDelay 250 sets any value.

local M = {}

local file = vim.fn.stdpath("state") .. "/prefs.json"
local defaults = { wrap = false, ctrl_z = false, double_click = false, key_popup = false, mouse_docs = true, sticky_header = true,
  git_blame = false, auto_include = true, breadcrumbs = true, balloon = true, find_bar = false, images_inline = true,
  thread_check = true, hover_delay = 500, ollama_autostart = true }

-- Hover delays the Settings list offers (ms); :HoverDelay <ms> sets any other.
local HOVER_DELAYS = { 100, 200, 300, 400, 500, 700, 1000, 1500, 2000 }

local values = (function()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end)()

function M.get(name)
  local v = values[name]
  if v == nil then v = defaults[name] end
  return v
end

local function save()
  pcall(vim.fn.mkdir, vim.fs.dirname(file), "p")
  pcall(vim.fn.writefile, { vim.json.encode(values) }, file)
end

-- Word wrap applies to every file window now and to the ones opened later.
local function apply_wrap()
  local on = M.get("wrap")
  vim.o.wrap = on
  for _, win in ipairs(vim.api.nvim_list_wins()) do
    if vim.api.nvim_win_get_config(win).relative == "" and vim.bo[vim.api.nvim_win_get_buf(win)].buftype == "" then
      vim.wo[win].wrap = on
    end
  end
end

local toggles = {
  { "wrap", "<leader>tw", "Toggle word wrap", "Word wrap", apply_wrap },
  { "ctrl_z", "<leader>tz", "Toggle Ctrl+Z suspending Neovim", "Ctrl+Z suspends Neovim" },
  { "double_click", "<leader>td", "Toggle double-click selecting the word", "Double-click selects the word" },
  { "key_popup", "<leader>tk", "Toggle key popup after Space", "Key popup after Space" },
  { "mouse_docs", "<leader>tq", "Toggle quick documentation on mouse hover", "Quick documentation on mouse hover",
    function() require("config.quick_docs").close() end },
  { "sticky_header", "<leader>tx", "Toggle sticky function header", "Sticky header",
    function() pcall(vim.cmd, M.get("sticky_header") and "TSContext enable" or "TSContext disable") end },
  { "git_blame", "<leader>tg", "Toggle git blame on the current line", "Git blame on the current line",
    function() pcall(function() require("gitsigns").toggle_current_line_blame(M.get("git_blame")) end) end },
  { "auto_include", "<leader>ti", "Toggle adding #includes on completion", "Add #includes on completion",
    function() M.restart_clangd() end },
  { "breadcrumbs", "<leader>tr", "Toggle breadcrumbs", "Breadcrumbs", function() M.apply_breadcrumbs() end },
  { "balloon", "<leader>tn", "Toggle the notification balloon (off: corner toasts)", "Notification balloon",
    function() require("config.balloon").close() end },
  { "find_bar", "<leader>t/", "Toggle the find bar for / and Cmd+F (off: plain Vim search)", "Find bar" },
  { "images_inline", "<leader>tI", "Toggle showing images in the editor (off: Preview)", "Images in the editor" },
}

function M.apply_breadcrumbs()
  local ok, bar = pcall(require, "dropbar.utils.bar")
  for _, win in ipairs(vim.api.nvim_list_wins()) do
    if not M.get("breadcrumbs") then
      if vim.wo[win].winbar:find("dropbar", 1, true) then vim.wo[win].winbar = "" end
    elseif ok then
      bar.attach(vim.api.nvim_win_get_buf(win), win)
    end
  end
end

local function map_toggle(t)
  local name, lhs, desc, label, apply = t[1], t[2], t[3], t[4], t[5]
  vim.keymap.set("n", lhs, function()
    values[name] = not M.get(name)
    save()
    if apply then apply() end
    vim.notify(label .. ": " .. (M.get(name) and "on" or "off"))
  end, { desc = desc })
end

-- A toggle a project turns on from its .nvim.lua (the thread check): same
-- shape as the ones above; adding it twice changes nothing.
function M.add_toggle(t)
  for _, have in ipairs(toggles) do
    if have[1] == t[1] then return end
  end
  toggles[#toggles + 1] = t
  map_toggle(t)
end

-- On/off of the Space t toggles kept in other files, by key. src is the
-- buffer Settings was opened from (inlay hints and the CSV view are per file).
local outside = {
  a = function() return vim.g.autosave end,
  b = function() return vim.o.laststatus ~= 0 end,
  c = function() return require("config.scanners").enabled("cppcheck") end,
  s = function() return require("config.scanners").enabled("semgrep") end,
  m = function() return require("config.mcp").enabled end,
  f = function() return vim.g.format_changed_on_save end,
  e = function() return require("config.explorer").remember end,
  l = function() return require("config.explorer").follow end,
  h = function(src) return vim.lsp.inlay_hint.is_enabled({ bufnr = src }) end,
  v = function(src) return require("csvview").is_enabled(src) end,
}
local not_settings = { t = true } -- Space tt opens a terminal

-- Settings with no Space t key (Enter flips them in the list).
local plain = {
  { "ollama_autostart", "Start Ollama when model checks need it" }, -- mcp.lua
}

-- One popup with every Space t toggle and its on/off (Enter flips it and the
-- list stays open), and the hover delay (Enter steps to the next one).
function M.settings(cursor, src)
  src = src or vim.api.nvim_get_current_buf()
  local by_lhs = {}
  for _, t in ipairs(toggles) do by_lhs[t[2]] = t end
  local items, seen = {}, {}
  local leader = vim.g.mapleader or "\\"
  local maps = vim.list_extend(vim.api.nvim_buf_get_keymap(0, "n"), vim.api.nvim_get_keymap("n"))
  for _, km in ipairs(maps) do
    local key = km.lhs:sub(1, #leader + 1) == leader .. "t" and #km.lhs == #leader + 2 and km.lhs:sub(-1)
    if key and km.desc and not seen[key] and not not_settings[key] then
      seen[key] = true
      local t = by_lhs["<leader>t" .. key]
      local label = t and t[4] or km.desc:gsub("^Toggle ", ""):gsub("^%l", string.upper)
      local on
      if t then
        on = M.get(t[1])
      elseif outside[key] then
        local ok, v = pcall(outside[key], src)
        if ok then on = v == true end
      end
      items[#items + 1] = { text = label, label = label, key = key, state = on == nil and nil or (on and "on" or "off") }
    end
  end
  for _, p in ipairs(plain) do
    items[#items + 1] = { text = p[2], label = p[2], pref = p[1], state = M.get(p[1]) and "on" or "off" }
  end
  table.sort(items, function(a, b) return a.label < b.label end)
  local delay = M.get("hover_delay")
  items[#items + 1] = { text = "Hover delay", label = "Hover delay", value = delay .. " ms", delay = true }
  Snacks.picker({
    title = "Settings",
    items = items,
    sort = { fields = { "idx" } },
    layout = { preset = "select" },
    format = function(item)
      if item.delay then
        return { { "◷ ", "DiagnosticInfo" }, { item.label .. ": " }, { item.value, "DiagnosticInfo" }, { "  Enter picks another", "Comment" } }
      end
      local mark = item.state == "on" and "● " or item.state == "off" and "○ " or "  "
      return {
        { mark, item.state == "on" and "DiagnosticOk" or "Comment" },
        { item.label },
        { item.key and ("  Space t" .. item.key) or "", "Comment" },
      }
    end,
    on_show = function(picker) if cursor then picker.list:move(cursor, true) end end,
    confirm = function(picker, item)
      if not item then return end
      local idx = picker.list.cursor
      picker:close()
      if item.delay then
        return vim.schedule(function() M.pick_hover_delay(function() M.settings(idx, src) end) end)
      end
      if item.pref then
        values[item.pref] = not M.get(item.pref)
        save()
        return vim.schedule(function() M.settings(idx, src) end)
      end
      vim.schedule(function()
        -- From the file it was opened on, so per-file toggles act on that file.
        local win = vim.fn.bufwinid(src)
        if win ~= -1 then vim.api.nvim_set_current_win(win) end
        vim.api.nvim_feedkeys(vim.keycode("<leader>t" .. item.key), "m", false)
        vim.defer_fn(function() M.settings(idx, src) end, 150)
      end)
    end,
  })
end

function M.set_hover_delay(ms)
  values.hover_delay = math.max(0, math.floor(ms))
  save()
  vim.notify("Hover delay: " .. values.hover_delay .. " ms")
end

-- A list of delays to pick from (the current one marked); then calls done.
function M.pick_hover_delay(done)
  local cur = M.get("hover_delay")
  local labels = {}
  for i, d in ipairs(HOVER_DELAYS) do labels[i] = (d == cur and "● " or "  ") .. d .. " ms" end
  vim.ui.select(labels, { prompt = "Hover delay" }, function(_, i)
    if i then M.set_hover_delay(HOVER_DELAYS[i]) end
    if done then done() end
  end)
end

-- clangd's --header-insertion follows the auto_include pref (lsp.lua).
function M.header_insertion() return "--header-insertion=" .. (M.get("auto_include") and "iwyu" or "never") end

function M.restart_clangd()
  local cfg = vim.lsp.config.clangd
  if not cfg or not cfg.cmd then return end
  local cmd = vim.deepcopy(cfg.cmd)
  for i, arg in ipairs(cmd) do
    if arg:find("^%-%-header%-insertion=") then cmd[i] = M.header_insertion() end
  end
  vim.lsp.config("clangd", { cmd = cmd })
  for _, c in ipairs(vim.lsp.get_clients({ name = "clangd" })) do c:stop() end
  -- enable() re-attaches clangd to the open C/C++ buffers.
  vim.defer_fn(function() vim.lsp.enable("clangd", false); vim.lsp.enable("clangd") end, 500)
end

function M.setup()
  apply_wrap()
  vim.keymap.set("n", "<D-,>", function() M.settings() end, { desc = "Settings" })
  vim.api.nvim_create_user_command("HoverDelay", function(o)
    if o.args == "" then return M.pick_hover_delay() end
    local ms = tonumber(o.args)
    if not ms then return vim.notify("HoverDelay: give a number of milliseconds", vim.log.levels.WARN) end
    M.set_hover_delay(ms)
  end, { nargs = "?", desc = "How long the mouse rests before hover popups show (ms)" })
  vim.keymap.set("n", "<leader>T", function() M.settings() end, { desc = "Settings (all Space t toggles)" })
  for _, t in ipairs(toggles) do map_toggle(t) end

  -- Ctrl+Z normally suspends Neovim (back to the shell; fg returns).
  vim.keymap.set({ "n", "x" }, "<C-z>", function() return M.get("ctrl_z") and "<C-z>" or "" end, { expr = true })

  -- Double-click normally selects the word (visual mode), a triple click the
  -- line. When that's off, the first click has already placed the cursor and
  -- the extra clicks do nothing.
  for _, key in ipairs({ "<2-LeftMouse>", "<3-LeftMouse>", "<4-LeftMouse>" }) do
    vim.keymap.set({ "n", "x", "i" }, key, function()
      return M.get("double_click") and key or ""
    end, { expr = true })
  end
end

-- which-key's delay (plugins/ui.lua): never, unless the popup is turned on.
function M.key_popup_delay(ctx)
  if not M.get("key_popup") then return 1e7 end
  return ctx.plugin and 0 or 200
end

return M
