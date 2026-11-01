-- Breadcrumbs, like CLion's: the file's folders and the class > function the
-- cursor is in, across the top of each editor window (under the split's tabs).
-- Clicking a folder crumb opens a menu of its files and folders (folders
-- first, A-Z; a folder there opens its own menu, a file opens), the file's
-- crumb the files next to it, and class > function the symbols around it. The folders are listed here
-- in Lua (vim.uv.fs_scandir), not with ls or file, and nothing is previewed
-- in the editor while the mouse moves over a menu (that swapped the window's
-- buffer back and forth). Space tr turns them off (remembered, config/prefs.lua).

-- Not listed in the menus.
local HIDE = { [".git"] = true, [".DS_Store"] = true, [".cache"] = true }

-- A folder's entries: { name, dir }, folders first, then A-Z.
local function entries(dir)
  local list = {}
  local handle = vim.uv.fs_scandir(dir)
  while handle do
    local name, kind = vim.uv.fs_scandir_next(handle)
    if not name then break end
    if kind == "link" then
      local stat = vim.uv.fs_stat(dir .. "/" .. name)
      kind = stat and stat.type or kind
    end
    if not HIDE[name] then list[#list + 1] = { name = name, dir = kind == "directory" } end
  end
  table.sort(list, function(a, b)
    if a.dir ~= b.dir then return a.dir end
    return a.name:lower() < b.name:lower()
  end)
  return list
end

local crumb

-- A crumb's menu: a folder's contents, or for the file the files next to it.
-- In a menu, a folder opens its own menu beside it and a file opens.
local function open_menu(symbol)
  local prev_win, prev_buf
  if symbol.bar then
    symbol.bar:update_current_context_hl(symbol.bar_idx)
    prev_win, prev_buf = symbol.bar.win, symbol.bar.buf
  elseif symbol.entry and symbol.entry.menu then
    symbol.entry.menu:update_current_context_hl(symbol.entry.idx)
    prev_win, prev_buf = symbol.entry.menu.win, symbol.entry.menu.buf
  end
  if symbol.menu then
    symbol.menu:toggle({ prev_win = prev_win, prev_buf = prev_buf })
    return
  end
  local path, is_dir = symbol.data.path, symbol.data.dir
  local dir = is_dir and path or vim.fs.dirname(path)
  local list = entries(dir)
  if #list == 0 then return end
  local cursor
  for i, e in ipairs(list) do
    if not is_dir and e.name == symbol.name then cursor = { i, 0 } end
  end
  local menu = require("dropbar.menu")
  local indicator = require("dropbar.configs").opts.icons.ui.menu.indicator
  local blank = string.rep(" ", vim.fn.strdisplaywidth(indicator))
  symbol.menu = menu.dropbar_menu_t:new({
    prev_win = prev_win,
    prev_buf = prev_buf,
    cursor = cursor,
    entries = vim.tbl_map(function(e)
      local sym = crumb(dir .. "/" .. e.name, e.dir, symbol.buf, symbol.win)
      local function open_file()
        local root = symbol.menu and symbol.menu:root()
        if root then root:close(false) end
        sym:jump()
      end
      local arrow = { name = "", icon = e.dir and indicator or blank, icon_hl = "DropBarIconUIIndicator" }
      if not e.dir then arrow.on_click = false end -- only a folder's ▸ opens a menu
      return menu.dropbar_menu_entry_t:new({
        components = { sym:merge(arrow), sym:merge({ on_click = not e.dir and open_file or nil }) },
      })
    end, list),
  })
  symbol.menu:toggle()
end

function crumb(path, is_dir, buf, win)
  local configs = require("dropbar.configs")
  local kinds = configs.opts.icons.kinds
  local icon, icon_hl = configs.eval(is_dir and kinds.dir_icon or kinds.file_icon, path)
  return require("dropbar.bar").dropbar_symbol_t:new({
    buf = buf,
    win = win,
    name = vim.fs.basename(path),
    icon = icon,
    icon_hl = icon_hl,
    name_hl = is_dir and "DropBarKindDir" or "DropBarKindFile",
    data = { path = path, dir = is_dir },
    on_click = open_menu,
    jump = function(self)
      if not is_dir then vim.cmd.edit(vim.fn.fnameescape(self.data.path)) end
    end,
  })
end

-- The folders from the project root down to the file.
local folders = {
  get_symbols = function(buf, win)
    buf = vim._resolve_bufnr(buf)
    if not vim.api.nvim_buf_is_valid(buf) or not vim.api.nvim_win_is_valid(win) then return {} end
    local ok, cwd = pcall(vim.fn.getcwd, win)
    local root = vim.fs.normalize(ok and cwd or vim.fn.getcwd())
    local path = vim.fs.normalize(vim.api.nvim_buf_get_name(buf))
    local symbols, is_dir = {}, false
    while path ~= "" and path ~= "." and path ~= root and path ~= vim.fs.dirname(path) and #symbols < 16 do
      table.insert(symbols, 1, crumb(path, is_dir, buf, win))
      path, is_dir = vim.fs.dirname(path), true
    end
    return symbols
  end,
}

return {
  "Bekaboo/dropbar.nvim",
  event = { "BufReadPost", "BufNewFile" },
  opts = function()
    local configs = require("dropbar.configs")
    local default = configs.opts.bar.enable
    return {
      bar = {
        enable = function(buf, win, info)
          buf = vim._resolve_bufnr(buf)
          return require("config.prefs").get("breadcrumbs") and vim.api.nvim_buf_is_valid(buf)
            and vim.bo[buf].buftype == "" and configs.eval(default, buf, win, info)
        end,
        sources = function(buf)
          local sources = require("dropbar.sources")
          if vim.bo[buf].ft == "markdown" then return { folders, sources.markdown } end
          return { folders, require("dropbar.utils").source.fallback({ sources.lsp, sources.treesitter }) }
        end,
      },
      menu = { preview = false },
      sources = { path = { preview = false } },
    }
  end,
}
