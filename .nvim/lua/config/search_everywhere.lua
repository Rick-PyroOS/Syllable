-- Find, like CLion's Search Everywhere: one finder with tabs for All, Classes,
-- Files, Symbols, Actions and Text. Space Space, Cmd+F (and Shift Shift, with
-- the Karabiner rule in extras/karabiner/) open it on All; the CLion shortcuts
-- (Cmd-O, Cmd-Shift-O, Cmd-Alt-O, Cmd-Shift-A, Cmd-Shift-F) open their own
-- tab. Actions are the command palette's list (lua/config/command_palette.lua).
--
-- Inside it:
--   Tab / Shift-Tab   next / previous tab (what you typed is kept)
--   Alt-n             include non-project items (hidden and git-ignored files,
--                     system and .sysroot symbols)
--   Shift-Enter       open in a right split (Ctrl-v works too)
--   the tab's own CLion shortcut again   same as Alt-n, like in CLion
--
-- It's one snacks picker whose source is swapped in place when the tab
-- changes, so nothing you type while switching gets lost.

local M = {}

local TYPES = { "Class", "Struct", "Enum", "Interface" }
local state = { tab = 1, all = false }

-- The All tab's file list without line numbers (an open file's line is still
-- where it opens).
local function file_no_line(item, picker)
  local pos = item.pos
  item.pos = nil
  local ret = Snacks.picker.format.file(item, picker)
  item.pos = pos
  return ret
end

-- Classes and Symbols: only the project's own code unless non-project items
-- are on (clangd also indexes the system and .sysroot headers).
-- (Runs inside the finder's async loop, so no vim.fn here.)
local function project_only()
  if state.all then return nil end
  local root = vim.fs.normalize(vim.uv.cwd()) .. "/"
  return function(item)
    local path = item.file and vim.fs.normalize(item.file)
    if path and (path:sub(1, #root) ~= root or path:find("/.sysroot/", 1, true)) then return false end
  end
end

-- name, the CLion shortcut for it, whether typing searches again (clangd,
-- ripgrep) rather than filtering a list, and its snacks picker options.
local TABS = {
  { name = "All", opts = function(all)
      return { multi = { "buffers", "recent", { source = "files", hidden = all, ignored = all } },
        format = file_no_line, transform = "unique_file" }
    end },
  { name = "Classes", key = "<D-o>", live = true, opts = function()
      return { source = "lsp_workspace_symbols", filter = { default = TYPES }, transform = project_only() }
    end },
  { name = "Files", key = "<D-O>", opts = function(all)
      return { source = "files", hidden = all, ignored = all }
    end },
  { name = "Symbols", key = "<D-M-o>", live = true, opts = function()
      return { source = "lsp_workspace_symbols", transform = project_only() }
    end },
  { name = "Actions", key = "<D-A>", opts = function(_, main)
      local o = require("config.command_palette").picker_opts(main)
      o.run, o.confirm = o.confirm, nil -- Enter runs the action (the confirm action below)
      return o
    end },
  { name = "Text", key = "<D-F>", live = true, opts = function(all)
      return { source = "grep", hidden = all, ignored = all }
    end },
}

local NBSP = "\194\160" -- padding that survives snacks trimming the title

local function tab_bar()
  local chunks = {}
  for i, t in ipairs(TABS) do
    chunks[#chunks + 1] = { NBSP .. t.name .. NBSP, i == state.tab and "SnacksPickerSearchTabActive" or "SnacksPickerSearchTab" }
  end
  if state.all then chunks[#chunks + 1] = { NBSP .. "+ non-project items" .. NBSP, "SnacksPickerSearchTabFlag" } end
  return chunks
end

-- Options that belong to the window, not the tab; kept when switching.
local KEEP = { layout = true, win = true, actions = true, title = true, main = true, matcher = true,
  formatters = true, on_show = true, on_close = true, focus = true, enter = true }

-- Switch the open picker to another tab (or flip non-project items), keeping the query.
local function switch(picker, tab, all)
  local query = picker.input.filter[picker.opts.live and "search" or "pattern"] or ""
  state.tab, state.all = tab, all
  local main = picker.main and vim.api.nvim_win_is_valid(picker.main) and vim.api.nvim_win_get_buf(picker.main) or 0
  local cfg = Snacks.picker.config.get(TABS[tab].opts(all, main))
  cfg.live, cfg.supports_live = TABS[tab].live or false, TABS[tab].live or false
  for k in pairs(picker.opts) do
    if not KEEP[k] and cfg[k] == nil then picker.opts[k] = nil end
  end
  for k, v in pairs(cfg) do
    if not KEEP[k] then picker.opts[k] = v end
  end
  picker.finder = require("snacks.picker.core.finder").new(Snacks.picker.config.finder(picker.opts.finder)
    or function() return picker.opts.items or {} end)
  picker.format = Snacks.picker.config.format(picker.opts)
  picker.title = TABS[tab].name
  picker.layout.root.meta.title_tpl = tab_bar()
  if picker.opts.live then picker.input:set("", query) else picker.input:set(query, "") end
  picker.list:set_target()
  picker:find()
end

--- Open Find on a tab ("All", "Classes", "Files", "Symbols", "Actions", "Text").
function M.open(name)
  state.tab, state.all = 1, false
  for i, t in ipairs(TABS) do
    if t.name == name then state.tab = i end
  end
  vim.api.nvim_set_hl(0, "SnacksPickerSearchTab", { link = "FloatTitle", default = true })
  vim.api.nvim_set_hl(0, "SnacksPickerSearchTabActive", { link = "PmenuSel", default = true })
  vim.api.nvim_set_hl(0, "SnacksPickerSearchTabFlag", { link = "DiagnosticHint", default = true })

  local keys = {
    ["<Tab>"] = { "se_next", mode = { "n", "i" } },
    ["<S-Tab>"] = { "se_prev", mode = { "n", "i" } },
    ["<M-n>"] = { "se_all", mode = { "n", "i" } },
    ["<S-CR>"] = { "edit_vsplit", mode = { "n", "i" } },
  }
  local acts = {
    se_next = function(p) switch(p, state.tab % #TABS + 1, state.all) end,
    se_prev = function(p) switch(p, (state.tab - 2) % #TABS + 1, state.all) end,
    se_all = function(p) switch(p, state.tab, not state.all) end,
    -- The picker's actions are fixed when it opens, so Enter asks the tab.
    confirm = function(p, item, action)
      if p.opts.run then return p.opts.run(p, item) end
      return Snacks.picker.actions.confirm(p, item, action)
    end,
  }
  for i, t in ipairs(TABS) do
    if t.key then
      keys[t.key] = { "se_tab" .. i, mode = { "n", "i" } }
      acts["se_tab" .. i] = function(p)
        if i == state.tab then switch(p, i, not state.all) else switch(p, i, state.all) end
      end
    end
  end

  local opts = vim.tbl_deep_extend("force", TABS[state.tab].opts(state.all, vim.api.nvim_get_current_buf()), {
    title = TABS[state.tab].name,
    live = TABS[state.tab].live or false,
    supports_live = TABS[state.tab].live or false,
    actions = acts,
    win = { input = { keys = keys }, list = { keys = keys } },
    formatters = { file = { filename_first = true } },
    matcher = { cwd_bonus = true, frecency = true, sort_empty = true },
    layout = {
      hidden = { "preview" },
      layout = {
        backdrop = false,
        row = 2,
        width = 0.6,
        min_width = 80,
        max_width = 140,
        height = 0.5,
        box = "vertical",
        border = "rounded",
        title = tab_bar(),
        title_pos = "left",
        footer = {
          { " Tab ", "SnacksFooterKey" }, { " switch tab  ", "SnacksFooterDesc" },
          { " ⌥N ", "SnacksFooterKey" }, { " non-project items  ", "SnacksFooterDesc" },
          { " ⇧⏎ ", "SnacksFooterKey" }, { " open in right split ", "SnacksFooterDesc" },
        },
        footer_pos = "right",
        { win = "input", height = 1, border = "rounded", title = "{live} {flags}", title_pos = "right" },
        { win = "list", border = "none" },
        { win = "preview", border = "top", height = 0.4 },
      },
    },
  })
  require("config.dialogs").open("search_everywhere", function()
    state.picker = Snacks.picker.pick(opts)
    return state.picker
  end)
end

-- Clicking a tab name in the title switches to that tab: the title row is
-- read off the screen to see which name the mouse is on.
vim.on_key(function(k, typed)
  if (typed ~= "" and typed or k) ~= vim.keycode("<LeftMouse>") then return end
  local picker = state.picker
  if not picker or picker.closed then return end
  local m = vim.fn.getmousepos()
  local cells = {}
  for col = 1, vim.o.columns do cells[col] = vim.fn.screenstring(m.screenrow, col) end
  local tab
  for i, t in ipairs(TABS) do
    local word = NBSP .. t.name .. NBSP
    local n = vim.fn.strchars(word)
    for col = 1, #cells - n + 1 do
      if table.concat(cells, "", col, col + n - 1) == word and m.screencol >= col and m.screencol < col + n then
        tab = i
      end
    end
  end
  -- Only on the title: all the names in a row there.
  local row = table.concat(cells)
  for _, t in ipairs(TABS) do
    if not row:find(NBSP .. t.name .. NBSP, 1, true) then return end
  end
  if not tab then return end
  vim.schedule(function()
    if picker.closed then return end
    if tab ~= state.tab then switch(picker, tab, state.all) end
    picker:focus("input")
  end)
  return ""
end, vim.api.nvim_create_namespace("rick_search_everywhere_tabs"))

return M
