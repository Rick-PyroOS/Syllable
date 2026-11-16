-- Keymap cheat sheet in a popup (Space ? or Cmd+Shift+/), built from the live
-- keymaps so it never goes stale: anything this config or a plugin maps with a
-- `desc` shows up. Neovim's own default mappings are left out.
-- q or Esc closes it; / searches inside it like any buffer.

local M = {}
local defaults = {}   -- lhs -> desc of Neovim's built-in mappings, captured before the config runs

-- Call first thing in init.lua, before any config or plugin adds mappings.
function M.snapshot()
  for _, km in ipairs(vim.api.nvim_get_keymap("n")) do defaults[km.lhs] = km.desc or "" end
end

-- Keys Neovim receives under other names (terminal encodings); shown via their real key.
local ALIASES = { ["<F14>"] = true, ["<F18>"] = true, ["<F55>"] = true, ["<F60>"] = true }
local NAMES = { BS = "Backspace", CR = "Enter", Esc = "Esc", Up = "Up", Down = "Down",
  Left = "Left", Right = "Right", Space = "Space", Tab = "Tab", lt = "<" }
local SHIFTED = { ["{"] = "[", ["}"] = "]", ["?"] = "/" }

-- "<D-M-o>" -> "Cmd+Alt+O", "<S-h>" -> "Shift+H", " ff" -> "Space f f"
local function pretty(lhs)
  local leader = vim.g.mapleader or "\\"
  local out, i = {}, 1
  while i <= #lhs do
    local tok
    if lhs:sub(i, i) == "<" then
      local j = lhs:find(">", i + 1, true)
      if j and j > i + 1 then tok = lhs:sub(i + 1, j - 1); i = j + 1 end
    end
    if tok then
      local mods, key = {}, tok
      while key:match("^[DMCSA]%-.") do
        local m = key:sub(1, 1)
        mods[m == "A" and "M" or m] = true
        key = key:sub(3)
      end
      if SHIFTED[key] then mods.S = true; key = SHIFTED[key] end
      if #key == 1 and key:match("%u") and not mods.C then mods.S = true end
      local parts = {}
      for _, m in ipairs({ { "C", "Ctrl" }, { "D", "Cmd" }, { "M", "Alt" }, { "S", "Shift" } }) do
        if mods[m[1]] then table.insert(parts, m[2]) end
      end
      table.insert(parts, NAMES[key] or (#key == 1 and key:upper()) or key)
      table.insert(out, table.concat(parts, "+"))
    else
      local c = lhs:sub(i, i)
      table.insert(out, c == leader and "Space" or (#lhs == 1 and c:match("%u")) and ("Shift+" .. c) or c)
      i = i + 1
    end
  end
  return table.concat(out, " ")
end

-- Every mapping with a description that this config or a plugin added (global,
-- plus the current buffer's LSP ones), one entry per description with all its
-- keys. The command palette (and Raycast) list these.
function M.actions()
  local list, by_desc = {}, {}
  local maps = vim.api.nvim_get_keymap("n")
  vim.list_extend(maps, vim.api.nvim_buf_get_keymap(0, "n"))
  for _, km in ipairs(maps) do
    local lhs, desc = km.lhs, km.desc
    if desc and desc ~= "" and defaults[lhs] ~= desc and not ALIASES[lhs] and not lhs:match("^<Plug>") then
      desc = desc:gsub("%s+", " ")
      local a = by_desc[desc]
      if not a then
        a = { desc = desc, lhs = lhs, keys = {} }
        by_desc[desc] = a
        table.insert(list, a)
      end
      local key = pretty(lhs)
      if not vim.tbl_contains(a.keys, key) then table.insert(a.keys, key) end
    end
  end
  return list
end

local function collect()
  local groups = { clion = {}, space = {}, other = {} }
  local seen = {}
  for _, km in ipairs(vim.api.nvim_get_keymap("n")) do
    local lhs, desc = km.lhs, km.desc
    if desc and desc ~= "" and defaults[lhs] ~= desc and not ALIASES[lhs] and not lhs:match("^<Plug>") then
      desc = desc:gsub("%s+", " ")
      local key = pretty(lhs)
      local id = key .. "\0" .. desc
      if not seen[id] then
        seen[id] = true
        local g = (lhs:sub(1, 1) == (vim.g.mapleader or "\\")) and "space"
          or (lhs:match("^<[^>]*[DM]%-") or lhs:match("^<[^>]*F%d+>")) and "clion"
          or "other"
        table.insert(groups[g], { key, desc })
      end
    end
  end
  for _, g in pairs(groups) do table.sort(g, function(a, b) return a[2]:lower() < b[2]:lower() end) end
  return groups
end

function M.open()
  local groups = collect()
  local lines = {}
  local function section(title, items)
    if #items == 0 then return end
    table.insert(lines, "## " .. title)
    table.insert(lines, "")
    for _, it in ipairs(items) do table.insert(lines, string.format("  %-24s %s", it[1], it[2])) end
    table.insert(lines, "")
  end
  section("CLion shortcuts (Cmd keys need the iTerm profile)", groups.clion)
  section("Space shortcuts", groups.space)
  section("Other", groups.other)
  Snacks.win({
    text = lines,
    ft = "markdown",
    width = 0.7,
    height = 0.85,
    border = "rounded",
    title = " Keymap ",
    title_pos = "center",
    wo = { wrap = false, conceallevel = 0, cursorline = true },
    keys = { q = "close", ["<Esc>"] = "close" },
  })
end

function M.setup()
  vim.keymap.set("n", "<leader>?", M.open, { desc = "Keymap cheat sheet" })
  vim.keymap.set("n", "<D-?>", M.open, { desc = "Keymap cheat sheet" })    -- Cmd+Shift+/
  vim.keymap.set("n", "<D-S-/>", M.open, { desc = "Keymap cheat sheet" })
end

return M
