-- Type hierarchy (Space ch, or right-click > Type Hierarchy), like CLion's
-- Ctrl+H: the class under the cursor with its base classes above it and the
-- classes derived from it below, indented by depth. Enter jumps to one.
-- Ctrl+H itself stays "window left" (keymaps.lua).

local M = {}
local MAX_DEPTH = 8

-- Follows "typeHierarchy/supertypes" or "/subtypes" from item; done(node)
-- gets { item, children } once every branch has answered.
local function walk(client, item, kind, depth, done)
  local node = { item = item, children = {} }
  if depth >= MAX_DEPTH then return done(node) end
  client:request("typeHierarchy/" .. kind, { item = item }, function(err, items)
    items = not err and items or {}
    local pending = #items
    if pending == 0 then return done(node) end
    for i, child in ipairs(items) do
      walk(client, child, kind, depth + 1, function(sub)
        node.children[i] = sub
        pending = pending - 1
        if pending == 0 then done(node) end
      end)
    end
  end)
end

local function entry(item, indent, root)
  local pos = (item.selectionRange or item.range).start
  return { text = item.name, name = item.name, file = vim.uri_to_fname(item.uri),
    pos = { pos.line + 1, pos.character }, indent = indent, root = root }
end

local function show(root, supers, subs)
  local bases, deepest = {}, 0
  local function collect(node, d)
    for _, c in ipairs(node.children) do
      bases[#bases + 1] = { item = c.item, depth = d }
      deepest = math.max(deepest, d)
      collect(c, d + 1)
    end
  end
  collect(supers, 1)
  -- Furthest base first, so the list reads top-down like CLion's.
  table.sort(bases, function(a, b) return a.depth > b.depth end)
  local items = {}
  for _, b in ipairs(bases) do items[#items + 1] = entry(b.item, deepest - b.depth, false) end
  items[#items + 1] = entry(root, deepest, true)
  local function derived(node, d)
    for _, c in ipairs(node.children) do
      items[#items + 1] = entry(c.item, deepest + d, false)
      derived(c, d + 1)
    end
  end
  derived(subs, 1)
  require("config.dialogs").open("type_hierarchy", function()
    return Snacks.picker({
      title = "Type Hierarchy: " .. root.name,
      items = items,
      sort = { fields = { "idx" } },
      format = function(item)
        return {
          { string.rep("  ", item.indent) },
          { item.name, item.root and "Special" or "SnacksPickerLabel" },
          { "  " .. vim.fn.fnamemodify(item.file, ":~:."), "Comment" },
        }
      end,
    })
  end)
end

function M.open()
  local client = vim.lsp.get_clients({ bufnr = 0, method = "textDocument/prepareTypeHierarchy" })[1]
  if not client then return vim.notify("Type hierarchy needs clangd in this file", vim.log.levels.WARN) end
  local params = vim.lsp.util.make_position_params(0, client.offset_encoding)
  client:request("textDocument/prepareTypeHierarchy", params, function(err, roots)
    local root = not err and roots and roots[1]
    if not root then return vim.notify("No class under the cursor", vim.log.levels.WARN) end
    local supers, subs
    local function both() if supers and subs then vim.schedule(function() show(root, supers, subs) end) end end
    walk(client, root, "supertypes", 0, function(n) supers = n; both() end)
    walk(client, root, "subtypes", 0, function(n) subs = n; both() end)
  end)
end

return M
