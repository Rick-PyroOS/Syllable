-- CSV and TSV files as a table (csvview): the columns line up with │ between
-- them, and the header row stays at the top while you scroll. It's on when
-- the file opens; Space tv turns it off and on. Tab / Shift-Tab move to the
-- next / previous field, and "if" / "af" select a field (vif, daf). The file
-- itself isn't changed: the spacing is only drawn.
return {
  {
    "hat0uma/csvview.nvim",
    ft = { "csv", "tsv" },
    cmd = { "CsvViewEnable", "CsvViewDisable", "CsvViewToggle", "CsvViewInfo" },
    opts = {
      view = { display_mode = "border" },
      keymaps = {
        textobject_field_inner = { "if", mode = { "o", "x" } },
        textobject_field_outer = { "af", mode = { "o", "x" } },
        jump_next_field_end = { "<Tab>", mode = { "n", "v" } },
        jump_prev_field_end = { "<S-Tab>", mode = { "n", "v" } },
      },
    },
    config = function(_, opts)
      local csvview = require("csvview")
      csvview.setup(opts)
      local function enable(buf)
        if vim.api.nvim_buf_is_valid(buf) and not csvview.is_enabled(buf) then csvview.enable(buf) end
      end
      vim.api.nvim_create_autocmd("FileType", {
        group = vim.api.nvim_create_augroup("rick_csvview", { clear = true }),
        pattern = { "csv", "tsv" },
        callback = function(ev) enable(ev.buf) end,
      })
      -- The file that loaded the plugin is already open.
      for _, buf in ipairs(vim.api.nvim_list_bufs()) do
        if vim.bo[buf].filetype == "csv" or vim.bo[buf].filetype == "tsv" then enable(buf) end
      end
    end,
    keys = {
      { "<leader>tv", "<cmd>CsvViewToggle<cr>", ft = { "csv", "tsv" }, desc = "Toggle CSV table view" },
    },
  },
}
