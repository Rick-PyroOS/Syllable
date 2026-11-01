-- Git diffs and the merge tool, under our toolbar and arrows (config/diff.lua).
return {
  "sindrets/diffview.nvim",
  cmd = { "DiffviewOpen", "DiffviewClose", "DiffviewFileHistory" },
  opts = {
    enhanced_diff_hl = true,
    view = {
      default = { layout = "diff2_horizontal", winbar_info = true },
      merge_tool = { layout = "diff3_horizontal", winbar_info = true, disable_diagnostics = true },
      file_history = { layout = "diff2_horizontal", winbar_info = true },
    },
    file_panel = { win_config = { width = 32 } },
    -- q closes the whole diff tab, from any of its panes.
    keymaps = {
      view = { { "n", "q", "<Cmd>DiffviewClose<CR>", { desc = "Close the diff" } } },
      file_panel = { { "n", "q", "<Cmd>DiffviewClose<CR>", { desc = "Close the diff" } } },
      file_history_panel = { { "n", "q", "<Cmd>DiffviewClose<CR>", { desc = "Close the diff" } } },
    },
  },
}
