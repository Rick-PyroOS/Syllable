-- Syntax highlighting from real parsers. Needs the tree-sitter CLI and a C compiler.
return {
  {
    "nvim-treesitter/nvim-treesitter",
    branch = "main",
    lazy = false,
    build = ":TSUpdate",
    config = function()
      require("nvim-treesitter").install(require("config.parsers"))
      vim.api.nvim_create_autocmd("FileType", {
        group = vim.api.nvim_create_augroup("rick_treesitter", { clear = true }),
        callback = function(ev)
          if not pcall(vim.treesitter.start, ev.buf) then return end
          -- Folds from the parser (the margin arrows, config/options.lua). Set
          -- here, after the parser starts, or the folds come up empty.
          vim.wo[0][0].foldmethod = "expr"
          vim.wo[0][0].foldexpr = "v:lua.vim.treesitter.foldexpr()"
        end,
      })
    end,
  },
  -- Sticky header, like CLion's: the function (class, namespace) you're in
  -- stays pinned at the top while you scroll. Space tx turns it off.
  {
    "nvim-treesitter/nvim-treesitter-context",
    event = "VeryLazy",
    opts = {
      enable = true,
      max_lines = 3, -- class, function and one block at most
      multiline_threshold = 1, -- a long signature shows as its first line
      trim_scope = "outer",
      mode = "topline", -- what's above the first visible line, as you scroll
      on_attach = function() return require("config.prefs").get("sticky_header") end,
    },
  },
}
