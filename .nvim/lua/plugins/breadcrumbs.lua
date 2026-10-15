-- Breadcrumbs, like CLion's: the file's folders and the class > function the
-- cursor is in, across the top of each editor window (under the split's tabs).
-- They're for reading only: clicking a crumb does nothing (its menu of
-- folders and symbols was slow, flickered the folder listing and could close
-- the project tree). Space tr turns them off (remembered, config/prefs.lua).
return {
  "Bekaboo/dropbar.nvim",
  event = { "BufReadPost", "BufNewFile" },
  opts = function()
    local configs = require("dropbar.configs")
    local default = configs.opts.bar.enable
    return {
      bar = {
        enable = function(buf, win, info)
          buf = vim._resolve_bufnr(buf)
          return require("config.prefs").get("breadcrumbs") and vim.api.nvim_buf_is_valid(buf)
            and vim.bo[buf].buftype == "" and configs.eval(default, buf, win, info)
        end,
        hover = false, -- nothing to click, so no hover highlight either
      },
      symbol = { on_click = false },
    }
  end,
}
