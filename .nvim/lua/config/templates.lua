-- Live templates, like CLion's. Type a template's name (for, oswin, doc,
-- handlemsg...) and pick it from the completion menu, or press Cmd+J (Space cj)
-- for the list of templates alone. Tab / Shift+Tab move between the fields.
--
-- Templates are plain VS Code-style JSON files, one per language:
--   yours:          snippets/<language>.json in this config (c.json, cpp.json)
--   the project's:  .nvim-snippets/<language>.json at the project root (Syllable keeps its own)
-- C templates also work in C++. Space cT opens a template file to edit; saving reloads them.

local M = {}

function M.search_paths()
  local paths = { vim.fn.stdpath("config") .. "/snippets" }
  local root = vim.fs.root(vim.fn.getcwd(), { ".nvim-snippets", ".nvim.lua", ".git" }) or vim.fn.getcwd()
  paths[#paths + 1] = root .. "/.nvim-snippets"
  return paths
end

function M.insert()
  local cmp = require("blink.cmp")
  local function show() cmp.show({ providers = { "snippets" } }) end
  if vim.fn.mode() == "i" then return show() end
  vim.cmd("startinsert")
  vim.schedule(show)
end

function M.edit()
  local ft = vim.bo.filetype ~= "" and vim.bo.filetype or "cpp"
  local paths = M.search_paths()
  local choices = {
    { label = "Your " .. ft .. " templates", path = paths[1] .. "/" .. ft .. ".json" },
    { label = "This project's " .. ft .. " templates", path = paths[2] .. "/" .. ft .. ".json" },
  }
  vim.ui.select(choices, {
    prompt = "Edit live templates",
    format_item = function(c) return c.label .. "  (" .. vim.fn.fnamemodify(c.path, ":~:.") .. ")" end,
  }, function(c)
    if not c then return end
    if not vim.uv.fs_stat(c.path) then
      vim.fn.mkdir(vim.fs.dirname(c.path), "p")
      vim.fn.writefile({
        "{",
        '  "example": {',
        '    "prefix": "mytemplate",',
        '    "description": "Shown next to the name in the menu",',
        '    "body": ["first line with a ${1:field}", "\\t$0"]',
        "  }",
        "}",
      }, c.path)
    end
    vim.cmd.edit(vim.fn.fnameescape(c.path))
  end)
end

function M.setup()
  vim.keymap.set({ "n", "i" }, "<D-j>", M.insert, { desc = "Insert live template" })
  vim.keymap.set("n", "<leader>cj", M.insert, { desc = "Insert live template" })
  vim.keymap.set("n", "<leader>cT", M.edit, { desc = "Edit live templates" })
  vim.api.nvim_create_autocmd("BufWritePost", {
    group = vim.api.nvim_create_augroup("rick_templates", { clear = true }),
    pattern = "*/snippets/*.json",
    callback = function()
      pcall(function() require("blink.cmp").reload("snippets") end)
      vim.notify("Live templates reloaded")
    end,
  })
end

return M
