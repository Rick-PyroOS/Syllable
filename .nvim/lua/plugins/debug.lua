-- The debugger (config/debug.lua drives it; config/run.lua starts sessions):
-- nvim-dap talks to gdb, which speaks DAP itself (gdb 14+), and nvim-dap-view
-- is the Debug window at the bottom: Variables, Watches, Threads (the call
-- stack), Breakpoints and the gdb console, with step buttons, and each
-- variable's value shown at the end of its line while stopped.
return {
  { "mfussenegger/nvim-dap", commit = "cfa2d58f4537aca6ca83e2de1a0d9f1491121264", lazy = true },
  {
    "igorlfs/nvim-dap-view",
    commit = "210b2c0e4acc08aad23fbd5a7989dd0169f716d0", -- v1.2.1
    lazy = true,
    dependencies = { "mfussenegger/nvim-dap" },
    opts = {
      winbar = {
        sections = { "scopes", "watches", "threads", "breakpoints", "repl" },
        default_section = "scopes",
        base_sections = {
          scopes = { label = "Variables", keymap = "V" },
          repl = { label = "gdb", keymap = "G" },
        },
        controls = { enabled = true, position = "right" },
      },
      windows = { size = 0.3, position = "below" },
      virtual_text = { enabled = true, position = "eol" },
    },
  },
}
