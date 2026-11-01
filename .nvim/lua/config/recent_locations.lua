-- Recent Locations, like CLion's (Cmd+Shift+E, Space fl): the places you were
-- reading or editing, newest first, with the function each one is in and the
-- line itself; the preview shows the code around it. Cmd+Shift+E again (or
-- Ctrl+E) inside the list switches to changed places only and back.
--
-- A place is remembered when you move away from it: to another file, or more
-- than a screenful-ish (JUMP lines) within the file. Editing marks it changed.
-- Kept across restarts in stdpath("state")/recent_locations.json.

local M = {}
local file = vim.fn.stdpath("state") .. "/recent_locations.json"
local MAX, JUMP, NEAR = 60, 12, 4

local list = (function()
  local ok, data = pcall(function() return vim.json.decode(table.concat(vim.fn.readfile(file), "\n")) end)
  return ok and type(data) == "table" and data or {}
end)()
local here -- { file, lnum, col, changed }

local function write()
  pcall(vim.fn.mkdir, vim.fs.dirname(file), "p")
  pcall(vim.fn.writefile, { vim.json.encode(list) }, file)
end

local function real_file(buf)
  if vim.bo[buf].buftype ~= "" or not vim.bo[buf].buflisted then return nil end
  local p = vim.api.nvim_buf_get_name(buf)
  return p ~= "" and vim.uv.fs_stat(p) and p or nil
end

-- "Window::Show", from the treesitter node around the position.
local FUNC = { function_definition = true, method_definition = true, function_declaration = true }
local function scope_name(buf, lnum, col)
  local ok, node = pcall(vim.treesitter.get_node, { bufnr = buf, pos = { lnum - 1, col }, ignore_injections = true })
  if not ok then return nil end
  while node do
    if FUNC[node:type()] then
      local d = node:field("declarator")[1]
      while d and d:field("declarator")[1] and d:type() ~= "qualified_identifier" and d:type() ~= "identifier"
        and d:type() ~= "field_identifier" and d:type() ~= "destructor_name" do
        d = d:field("declarator")[1]
      end
      local text = d and vim.treesitter.get_node_text(d, buf) or nil
      return text and text:gsub("%s+", " ") or nil
    end
    node = node:parent()
  end
end

-- Put a place at the front, replacing one nearby in the same file.
local function commit(loc)
  if not loc then return end
  for i = #list, 1, -1 do
    local o = list[i]
    if o.file == loc.file and math.abs(o.lnum - loc.lnum) <= NEAR then
      loc.changed = loc.changed or o.changed
      table.remove(list, i)
    end
  end
  table.insert(list, 1, loc)
  while #list > MAX do table.remove(list) end
end

local function current()
  local buf = vim.api.nvim_get_current_buf()
  local path = real_file(buf)
  if not path then return nil end
  local pos = vim.api.nvim_win_get_cursor(0)
  return { file = path, lnum = pos[1], col = pos[2], scope = scope_name(buf, pos[1], pos[2]), buf = buf }
end

local function moved()
  local now = current()
  if not now then return end
  -- An edited place stays where the edit was: moving a few lines off it starts a new place.
  local limit = here and here.changed and NEAR or JUMP
  if here and (here.file ~= now.file or math.abs(here.lnum - now.lnum) > limit) then
    here.buf = nil
    -- The top of a file you just opened and jumped away from isn't a place you were.
    if here.changed or here.lnum > 1 then commit(here) end
    here = now
  elseif not here then
    here = now
  else
    here.lnum, here.col = now.lnum, now.col -- small moves just update where you are
    here.scope = now.scope or here.scope
  end
end

local function edited()
  local now = current()
  if not now then return end
  if here and here.file == now.file and math.abs(here.lnum - now.lnum) <= (here.changed and NEAR or JUMP) then
    here.lnum, here.col, here.scope = now.lnum, now.col, now.scope or here.scope
    here.changed = true
  else
    moved()
    if here then here.changed = true end
  end
end

local function line_of(path, lnum)
  local b = vim.fn.bufnr(path)
  if b > 0 and vim.api.nvim_buf_is_loaded(b) then
    return vim.api.nvim_buf_get_lines(b, lnum - 1, lnum, false)[1] or ""
  end
  return vim.fn.readfile(path, "", lnum)[lnum] or ""
end

function M.items(changed_only)
  local all = vim.deepcopy(list)
  if here then local h = vim.deepcopy(here); h.buf = nil; table.insert(all, 1, h) end
  local items, seen = {}, {}
  for _, l in ipairs(all) do
    local key = l.file .. ":" .. l.lnum
    if not seen[key] and vim.uv.fs_stat(l.file) and (not changed_only or l.changed) then
      seen[key] = true
      local line = line_of(l.file, l.lnum)
      items[#items + 1] = {
        file = l.file, pos = { l.lnum, l.col or 0 }, changed = l.changed, scope = l.scope,
        line = vim.trim(line), text = vim.fn.fnamemodify(l.file, ":t") .. " " .. (l.scope or "") .. " " .. line,
      }
    end
  end
  return items
end

function M.open(changed_only)
  moved()
  local items = M.items(changed_only)
  if #items == 0 then
    return vim.notify(changed_only and "No changed places yet" or "No recent places yet")
  end
  local title = changed_only and "Recently Changed Locations" or "Recent Locations"
  local function flip(picker)
    picker:close()
    vim.schedule(function() M.open(not changed_only) end)
  end
  require("config.dialogs").open("recent_locations", function()
    return Snacks.picker({
      title = title .. "   (Cmd+Shift+E: " .. (changed_only and "show all" or "changed only") .. ")",
      items = items,
      sort = { fields = { "idx" } },
      format = function(item)
        return {
          { item.changed and "● " or "  ", "DiffChange" },
          { vim.fn.fnamemodify(item.file, ":t") .. ":" .. item.pos[1], "SnacksPickerFile" },
          { item.scope and ("  " .. item.scope) or "", "Function" },
          { "  " .. item.line, "Comment" },
        }
      end,
      actions = { flip_changed = flip },
      win = {
        input = { keys = { ["<D-E>"] = { "flip_changed", mode = { "n", "i" } }, ["<C-e>"] = { "flip_changed", mode = { "n", "i" } } } },
        list = { keys = { ["<D-E>"] = "flip_changed", ["<C-e>"] = "flip_changed" } },
      },
    })
  end)
end

function M.setup()
  local group = vim.api.nvim_create_augroup("rick_recent_locations", { clear = true })
  local timer = vim.uv.new_timer()
  vim.api.nvim_create_autocmd({ "CursorMoved", "BufEnter" }, {
    group = group,
    callback = function()
      timer:stop()
      timer:start(150, 0, vim.schedule_wrap(moved))
    end,
  })
  -- An edit counts once, where it happened: TextChanged also fires late for typing
  -- that was already counted when insert mode ended (e.g. after a picker).
  local function changed_here()
    if vim.b.rick_rl_tick == vim.b.changedtick then return end
    vim.b.rick_rl_tick = vim.b.changedtick
    edited()
  end
  vim.api.nvim_create_autocmd({ "TextChanged", "InsertLeave" }, { group = group, callback = changed_here })
  vim.api.nvim_create_autocmd({ "BufReadPost", "InsertEnter" }, {
    group = group,
    callback = function() vim.b.rick_rl_tick = vim.b.rick_rl_tick or vim.b.changedtick end,
  })
  -- Saving (auto-save, format on save) bumps the tick without you editing anything here.
  vim.api.nvim_create_autocmd("BufWritePost", {
    group = group,
    callback = function(ev) vim.b[ev.buf].rick_rl_tick = vim.b[ev.buf].changedtick end,
  })
  vim.api.nvim_create_autocmd("VimLeavePre", {
    group = group,
    callback = function() if here then here.buf = nil; commit(here) end write() end,
  })
  vim.keymap.set("n", "<D-E>", function() M.open(false) end, { desc = "Recent locations" }) -- Cmd+Shift+E
  vim.keymap.set("n", "<leader>fl", function() M.open(false) end, { desc = "Recent locations" })
end

return M
