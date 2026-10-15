-- Completion. Accepting a function inserts just foo() with the cursor between
-- the parentheses (no parameter text to type over; clangd's
-- --function-arg-placeholders=0 in config/lsp.lua), and the parameters show
-- in the signature popup above. When the call ends the statement
-- ("x = foo(", "foo(" on its own line), the ; goes in too: foo(|);

-- After accepting item: add ";" after the call's ")" when nothing else
-- follows on the line and the parentheses balance, in C and C++.
local CALL_KINDS = { [2] = true, [3] = true } -- LSP Method, Function
local function add_semicolon(item)
  if not (item and CALL_KINDS[item.kind]) then return end
  local ft = vim.bo.filetype
  if ft ~= "c" and ft ~= "cpp" then return end
  local line = vim.api.nvim_get_current_line()
  local col = vim.api.nvim_win_get_cursor(0)[2]
  local before, after = line:sub(1, col), line:sub(col + 1)
  if before:match("^%s*#") then return end                  -- preprocessor line
  local _, opens = line:gsub("%(", "")
  local _, closes = line:gsub("%)", "")
  if opens ~= closes then return end                        -- inside if (...), for (...), an argument list
  if after:match("^%)%s*$") then                            -- foo(|)
    vim.api.nvim_set_current_line(before .. ");")
  elseif after:match("^%s*$") and before:match("%)$") then -- foo()| (no parameters)
    vim.api.nvim_set_current_line(before .. ";")
    vim.api.nvim_win_set_cursor(0, { vim.api.nvim_win_get_cursor(0)[1], col + 1 })
  end
end

return {
  "saghen/blink.cmp",
  version = "1.*",   -- release builds ship a prebuilt fuzzy matcher
  lazy = false,
  opts = {
    -- "enter": <CR> accepts, <C-n>/<C-p> or Up/Down move, <C-space> open, <C-e> close.
    -- Prefer Tab to accept? Change to "super-tab".
    keymap = {
      preset = "enter",
      ["<CR>"] = {
        function(cmp)
          local item = cmp.get_selected_item()
          return cmp.accept({ callback = function() add_semicolon(item) end })
        end,
        "fallback",
      },
    },
    appearance = { nerd_font_variant = "mono" },
    completion = {
      documentation = { auto_show = true, auto_show_delay_ms = 300 },
    },
    -- The parameters of the call you're typing, above the line.
    signature = { enabled = true, trigger = { show_on_accept = true } },
    sources = {
      default = { "lsp", "path", "snippets", "buffer" },
      providers = {
        -- Live templates (config/templates.lua): yours in snippets/, the project's in .nvim-snippets/.
        snippets = {
          opts = {
            friendly_snippets = false,
            search_paths = require("config.templates").search_paths(),
            extended_filetypes = { cpp = { "c" } },
          },
        },
      },
    },
    fuzzy = { implementation = "prefer_rust_with_warning" },
  },
}
