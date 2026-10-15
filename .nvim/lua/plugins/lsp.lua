return {
  -- Server definitions (cmd, filetypes, root markers) used by vim.lsp.enable().
  { "neovim/nvim-lspconfig", lazy = false },

  -- Installs language servers into Neovim's data dir and puts them on PATH.
  { "mason-org/mason.nvim", lazy = false, opts = {} },
  {
    "WhoIsSethDaniel/mason-tool-installer.nvim",
    lazy = false,
    dependencies = { "mason-org/mason.nvim" },
    opts = { ensure_installed = { "clangd", "lua-language-server", "clang-format", "stylua", "shfmt", "ruff" } },
  },
}
