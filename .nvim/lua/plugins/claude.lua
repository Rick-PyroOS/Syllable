-- claudecode.nvim runs the same MCP/WebSocket bridge the VS Code extension uses.
-- Claude Code (the `claude` CLI) connects to Neovim, sees your open file and
-- selection, and proposes edits as diffs you accept or reject here.
return {
  "coder/claudecode.nvim",
  dependencies = { "folke/snacks.nvim" },
  -- Load at startup so an external `claude` can find Neovim via /ide too.
  event = "VeryLazy",
  opts = {
    terminal = {
      split_side = "right",
      split_width_percentage = 0.40,
    },
  },
  keys = {
    { "<leader>ac", "<cmd>ClaudeCode<cr>", desc = "Toggle Claude" },
    { "<leader>af", "<cmd>ClaudeCodeFocus<cr>", desc = "Focus Claude" },
    { "<leader>ar", "<cmd>ClaudeCode --resume<cr>", desc = "Resume a session" },
    { "<leader>aC", "<cmd>ClaudeCode --continue<cr>", desc = "Continue last session" },
    { "<leader>am", "<cmd>ClaudeCodeSelectModel<cr>", desc = "Select model" },
    { "<leader>ab", "<cmd>ClaudeCodeAdd %<cr>", desc = "Add this file to context" },
    { "<leader>as", "<cmd>ClaudeCodeSend<cr>", mode = "v", desc = "Send selection" },
    { "<leader>aa", "<cmd>ClaudeCodeDiffAccept<cr>", desc = "Accept diff" },
    { "<leader>ad", "<cmd>ClaudeCodeDiffDeny<cr>", desc = "Reject diff" },
  },
}
