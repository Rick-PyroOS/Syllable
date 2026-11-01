-- Command palette, like CLion's Find Action: every mapping with a description
-- (with its keys), plus commands like :Lazy or :Mason. Enter runs it; actions
-- you ran recently come first (marked ●). Space p or Cmd+Shift+P (keys in
-- clion_keys.lua and plugins/snacks.lua). The same list is Find's Actions tab
-- (search_everywhere.lua), which Space Space and Cmd+Shift+A open.

local M = {}

local recent_file = vim.fn.stdpath("state") .. "/command_palette.json"

local function read_recent()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(recent_file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end

local function remember(id)
  local list = { id }
  for _, r in ipairs(read_recent()) do
    if r ~= id and #list < 30 then list[#list + 1] = r end
  end
  pcall(vim.fn.mkdir, vim.fs.dirname(recent_file), "p")
  pcall(vim.fn.writefile, { vim.json.encode(list) }, recent_file)
end

local function run(item)
  remember(item.id)
  if item.cmd and item.nargs == "0" then vim.cmd(item.cmd)
  elseif item.cmd then vim.api.nvim_feedkeys(":" .. item.cmd .. " ", "n", false)
  else vim.api.nvim_feedkeys(vim.keycode(item.lhs), "m", false) end
end

local function items(buf)
  local list = {}
  vim.api.nvim_buf_call(buf, function()   -- so the file's LSP keys are included
    for _, a in ipairs(require("config.cheatsheet").actions()) do
      local keys = table.concat(a.keys, ", ")
      list[#list + 1] = { id = "map:" .. a.desc, text = a.desc .. " " .. keys, label = a.desc, hint = keys, lhs = a.lhs }
    end
  end)
  for name, c in pairs(vim.api.nvim_get_commands({})) do
    local def = (c.definition or ""):gsub("%s+", " ")
    list[#list + 1] = { id = "cmd:" .. name, text = name .. " " .. def, label = ":" .. name, hint = def:sub(1, 60),
                        cmd = name, nargs = c.nargs }
  end
  -- Recently run first, then the mappings A-Z, then the commands.
  local rank = {}
  for i, id in ipairs(read_recent()) do rank[id] = i end
  table.sort(list, function(a, b)
    local ra, rb = rank[a.id] or 1000, rank[b.id] or 1000
    if ra ~= rb then return ra < rb end
    if (a.cmd == nil) ~= (b.cmd == nil) then return a.cmd == nil end
    return a.label:lower() < b.label:lower()
  end)
  return list, rank
end

-- Picker options for the list (its items, how they look, Enter runs one), for
-- the file in buf.
function M.picker_opts(buf)
  local list, rank = items(buf)
  return {
    items = list,
    format = function(item)
      return {
        { rank[item.id] and "● " or "  ", "SnacksPickerComment" },
        { Snacks.picker.util.align(item.label, 44, { truncate = true }) }, { " " },
        { item.hint, "SnacksPickerComment" },
      }
    end,
    confirm = function(picker, item)
      picker:close()
      if item then vim.schedule(function() run(item) end) end
    end,
  }
end

function M.open()
  require("config.dialogs").open("command_palette", function() return Snacks.picker.pick(vim.tbl_extend("force", M.picker_opts(vim.api.nvim_get_current_buf()), {
    source = "command_palette",
    title = "Commands",
    matcher = { sort_empty = false },
    layout = {
      preview = false,
      layout = {
        backdrop = false, row = 2, width = 0.5, min_width = 70, max_width = 120, height = 0.45,
        box = "vertical", border = "rounded", title = " Commands ", title_pos = "center",
        { win = "input", height = 1, border = "bottom" },
        { win = "list", border = "none" },
      },
    },
  })) end)
end

return M
