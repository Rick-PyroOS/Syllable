-- Markdown: headings, lists, tables and code blocks drawn in the editor
-- (render-markdown), and a live browser preview that also draws Mermaid
-- diagrams (markdown-preview, Space mp). Terminals can't draw the diagrams
-- themselves, so Mermaid only shows in the preview.
return {
  {
    "MeanderingProgrammer/render-markdown.nvim",
    ft = { "markdown" },
    dependencies = { "nvim-treesitter/nvim-treesitter" },
    opts = {
      -- Show the raw text on the line the cursor is on, so it can be edited.
      anti_conceal = { enabled = true },
      -- Popups (hover docs and the like): code without the shaded background
      -- (config/doc_links.lua), and clicking a line in one doesn't show its
      -- raw markdown (the ``` fences, the backticks).
      overrides = {
        buftype = {
          nofile = {
            render_modes = true, sign = { enabled = false }, anti_conceal = { enabled = false },
            code = { highlight_inline = "RickDocCode" },
          },
        },
      },
    },
  },
  {
    "iamcco/markdown-preview.nvim",
    ft = { "markdown" },
    cmd = { "MarkdownPreview", "MarkdownPreviewStop", "MarkdownPreviewToggle" },
    -- Downloads the preview server once, at install (no Node needed). The
    -- plugin has to be loaded first for its install function to exist.
    build = function()
      require("lazy").load({ plugins = { "markdown-preview.nvim" } })
      vim.fn["mkdp#util#install_sync"](true)
    end,
    init = function()
      vim.g.mkdp_filetypes = { "markdown" }
      vim.g.mkdp_auto_close = 0 -- keep the preview open when switching files
    end,
    keys = {
      { "<leader>mp", "<cmd>MarkdownPreviewToggle<cr>", ft = "markdown", desc = "Markdown preview in the browser (with Mermaid)" },
    },
  },
}
