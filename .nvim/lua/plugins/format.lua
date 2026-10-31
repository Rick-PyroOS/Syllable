-- conform.nvim: formatting with the tool for the buffer's language.
--
-- When you save (:w or Cmd+S, not auto-save), only the lines you changed (git
-- hunks, via gitsigns) are formatted, so decades-old code around your edit is
-- left alone and diffs stay clean. A file git doesn't track yet (a new file) is
-- all yours, so it's formatted whole. Files outside any git repo are never
-- formatted on save. Nothing is formatted while you type.
-- Space tf toggles it.
-- Space cf formats the whole file (or the visual selection) on demand.
-- C/C++ style comes from the nearest .clang-format.

-- Filetypes whose formatter can format a line range.
local on_save_ft = { c = true, cpp = true, lua = true }

-- Whether the file sits inside a git repo but git doesn't track it yet, asked
-- in the background when the buffer is entered (b:rick_untracked) so a save
-- doesn't wait on git.
local function check_untracked(buf)
  local path = vim.api.nvim_buf_get_name(buf)
  if path == "" or vim.bo[buf].buftype ~= "" then return end
  path = vim.fs.abspath(path)
  local dir = vim.fs.dirname(path)
  vim.system({ "git", "-C", dir, "ls-files", "--error-unmatch", path }, {}, function(r)
    vim.schedule(function()
      if not vim.api.nvim_buf_is_valid(buf) then return end
      -- 128: not a repo (or no such folder); 1: in a repo, not tracked.
      vim.b[buf].rick_untracked = r.code == 1
    end)
  end)
end

vim.api.nvim_create_autocmd({ "BufReadPost", "BufEnter", "BufWritePost" }, {
  group = vim.api.nvim_create_augroup("rick_format_untracked", { clear = true }),
  callback = function(ev) check_untracked(ev.buf) end,
})

local function is_untracked_in_repo(buf)
  local known = vim.b[buf].rick_untracked
  if known ~= nil then return known end
  local path = vim.fs.abspath(vim.api.nvim_buf_get_name(buf))
  local r = vim.system({ "git", "-C", vim.fs.dirname(path), "ls-files", "--error-unmatch", path }):wait()
  vim.b[buf].rick_untracked = r.code == 1
  return r.code == 1
end

local function format_changed_lines(bufnr)
  local conform = require("conform")
  local path = vim.api.nvim_buf_get_name(bufnr)
  if path == "" then return end
  path = vim.fs.abspath(path)
  if is_untracked_in_repo(bufnr) then
    conform.format({ bufnr = bufnr, async = false, timeout_ms = 2000, lsp_format = "never" })
    return
  end
  local ok, gs = pcall(require, "gitsigns")
  if not ok then return end
  local hunks = gs.get_hunks(bufnr)
  if not hunks then return end -- not in git
  -- Bottom-up, so formatting one hunk doesn't shift the line numbers of the next.
  for i = #hunks, 1, -1 do
    local h = hunks[i]
    if h.type ~= "delete" and h.added.count > 0 then
      local first = h.added.start
      local last = first + h.added.count - 1
      local last_line = vim.api.nvim_buf_get_lines(bufnr, last - 1, last, true)[1] or ""
      conform.format({
        bufnr = bufnr,
        async = false,
        timeout_ms = 2000,
        lsp_format = "never",
        range = { start = { first, 0 }, ["end"] = { last, #last_line } },
      })
    end
  end
end

vim.g.format_changed_on_save = true

vim.api.nvim_create_autocmd("BufWritePre", {
  group = vim.api.nvim_create_augroup("rick_format_changed", { clear = true }),
  callback = function(ev)
    if vim.g.rick_autosaving then return end -- auto-save writes aren't formatted (autosave.lua)
    if vim.g.format_changed_on_save and on_save_ft[vim.bo[ev.buf].filetype] then
      format_changed_lines(ev.buf)
    end
  end,
})

return {
  "stevearc/conform.nvim",
  lazy = false,
  keys = {
    {
      "<leader>cf",
      function() require("conform").format({ async = true, lsp_format = "fallback" }) end,
      mode = { "n", "x" },
      desc = "Format file / selection",
    },
    {
      "<leader>tf",
      function()
        vim.g.format_changed_on_save = not vim.g.format_changed_on_save
        vim.notify("Format on save: " .. (vim.g.format_changed_on_save and "on" or "off"))
      end,
      desc = "Toggle format on save",
    },
  },
  opts = {
    formatters = {
      -- A buffer with no .clang-format above it (a scratch buffer, a file outside
      -- the repo) uses vim.g.clang_format_fallback, a path a project's .nvim.lua
      -- can set, instead of clang-format's LLVM default.
      clang_format = {
        prepend_args = function(_, ctx)
          if vim.fs.find(".clang-format", { upward = true, path = ctx.dirname })[1] then return {} end
          local fallback = vim.g.clang_format_fallback
          return fallback and vim.uv.fs_stat(fallback) and { "--style=file:" .. fallback } or {}
        end,
      },
    },
    formatters_by_ft = {
      c = { "clang_format" },
      cpp = { "clang_format" },
      lua = { "stylua" },
      sh = { "shfmt" },
      bash = { "shfmt" },
      python = { "ruff_format" },
      json = { "jq" },
      cmake = { "cmake_format" },
    },
  },
}
