-- Doxygen: a browser preview of the docs for the file you're editing
-- (doxygen-previewer, Space cp). It runs doxygen on that file, opens the
-- result in the browser and refreshes it each time the file is saved;
-- :DoxygenStop ends it. It needs the doxygen program (install.sh installs it;
-- or brew install doxygen). The doxygen tags in comments (\param, @brief) are
-- colored in the editor by the treesitter "doxygen" parser.
return {
  {
    "hat0uma/doxygen-previewer.nvim",
    dependencies = { "hat0uma/prelive.nvim" },
    cmd = { "DoxygenOpen", "DoxygenUpdate", "DoxygenStop", "DoxygenLog", "DoxygenTempDoxyfileOpen" },
    opts = {
      -- The page title: the project folder's name (doxygen's default is "My Project").
      doxygen = { override_options = { PROJECT_NAME = function() return vim.fn.fnamemodify(vim.fn.getcwd(), ":t") end } },
    },
    keys = {
      {
        "<leader>cp",
        function()
          if vim.fn.executable("doxygen") == 0 then
            vim.notify("The doxygen preview needs doxygen: brew install doxygen", vim.log.levels.WARN)
            return
          end
          vim.cmd("DoxygenOpen")
        end,
        ft = { "c", "cpp" },
        desc = "Doxygen preview in the browser",
      },
    },
  },
}
