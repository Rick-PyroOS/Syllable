return {
  {
    "folke/tokyonight.nvim",
    lazy = false,
    priority = 1000,
    config = function() vim.cmd.colorscheme("tokyonight-night") end,
  },
  {
    -- Can pop up the available keys after you press <leader> and pause. Off
    -- unless Space tk turns it on (config/prefs.lua).
    "folke/which-key.nvim",
    event = "VeryLazy",
    opts = {
      preset = "helix",
      delay = function(ctx) return require("config.prefs").key_popup_delay(ctx) end,
      spec = {
        { "<leader>a", group = "Claude" },
        { "<leader>b", group = "Tabs (buffers)" },
        { "<leader>c", group = "Code" },
        { "<leader>f", group = "Find" },
        { "<leader>g", group = "Git" },
        { "<leader>m", group = "Markdown" },
        { "<leader>r", group = "Build & Run" },
        { "<leader>t", group = "Toggle" },
      },
    },
  },
  {
    "lewis6991/gitsigns.nvim",
    event = { "BufReadPre", "BufNewFile" },
    opts = {
      current_line_blame = require("config.prefs").get("git_blame"), -- Space tg
      on_attach = function(buf)
        local gs = require("gitsigns")
        local function map(l, r, d) vim.keymap.set("n", l, r, { buffer = buf, desc = d }) end
        map("]h", function() gs.nav_hunk("next") end, "Next hunk")
        map("[h", function() gs.nav_hunk("prev") end, "Previous hunk")
        map("<leader>gp", gs.preview_hunk, "Preview hunk")
        map("<leader>gr", gs.reset_hunk, "Reset hunk")
        map("<leader>gB", gs.blame_line, "Blame line")
        map("<leader>ga", function() require("config.blame").toggle() end, "Annotate with git blame (toggle)")
      end,
    },
  },
  {
    -- Detects tabs vs spaces per file; the tree mixes both.
    "NMAC427/guess-indent.nvim",
    event = "BufReadPre",
    opts = {},
  },
}
