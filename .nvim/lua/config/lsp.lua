-- LSP with Neovim 0.11's built-in client. nvim-lspconfig supplies the
-- default server definitions; we only override what matters here.

vim.lsp.config("*", {
  capabilities = require("blink.cmp").get_lsp_capabilities(),
})
-- clangd sends its fix-its along with each diagnostic, so the margin popup
-- can offer Apply fix (lua/config/diagnostics.lua).
vim.lsp.config("clangd", {
  capabilities = { textDocument = { publishDiagnostics = { codeActionsInline = true } } },
})

local clangd_on_init = (vim.lsp.config.clangd or {}).on_init -- nvim-lspconfig's, kept below
vim.lsp.config("clangd", {
  cmd = {
    "clangd",
    "--background-index",
    require("config.prefs").header_insertion(), -- iwyu: accepting a completion adds the #include it needs (Space ti)
    "--header-insertion-decorators", -- marks those completion items with a dot
    require("config.prefs").clang_tidy(), -- the project's .clang-tidy checks, in the margin and Problems (Space tk)
    "--completion-style=detailed",
    "--function-arg-placeholders=0", -- completing a call inserts foo() with the cursor inside; the parameters show above
    "--pch-storage=memory",
    "-j=6",
  },
  -- Turn doxygen comments (\par, \param, \sa ...) into headings
  -- and lists in hover and completion docs. See lua/config/doxygen.lua.
  -- A project's .clangd can turn on clangd's own doxygen layout, which needs
  -- clangd 22 or newer: say so once if this one is older.
  on_init = function(client, result)
    if clangd_on_init then clangd_on_init(client, result) end
    require("config.doxygen").attach(client)
    local major = tonumber(((result.serverInfo or {}).version or ""):match("(%d+)%.%d+"))
    local dotclangd = vim.fs.joinpath(client.root_dir or "", ".clangd")
    if major and major < 22 and not vim.g.rick_clangd_old_warned
      and vim.fn.filereadable(dotclangd) == 1 and table.concat(vim.fn.readfile(dotclangd), "\n"):find("CommentFormat") then
      vim.g.rick_clangd_old_warned = true
      vim.schedule(function()
        vim.notify(("clangd %d ignores the doxygen layout in .clangd (needs 22 or newer). Run :MasonToolsUpdate to update it."):format(major),
          vim.log.levels.WARN)
      end)
    end
  end,
})

vim.lsp.config("lua_ls", {
  settings = {
    Lua = {
      workspace = { checkThirdParty = false },
      diagnostics = { globals = { "vim", "Snacks" } },
      telemetry = { enable = false },
    },
  },
})

-- CMakeLists.txt and *.cmake: completion, hover and go to definition (config/cmake.lua).
vim.lsp.enable({ "clangd", "lua_ls", "neocmake" })

-- Errors: the E in the margin and an underline; the message shows when the
-- mouse rests on the E (lua/config/diagnostics.lua).
-- The margin letters are drawn in lua/config/scanners.lua, one per line.
vim.diagnostic.config({
  severity_sort = true,
  signs = false,
  virtual_text = false,
  float = { border = "rounded", source = true },
  underline = true,
})

-- Buffer-local keymaps once a server attaches.
-- Built in to 0.11 already: K hover, grn rename, gra code action,
-- grr references, gri implementation, gO symbols, <C-s> signature help (insert),
-- [d / ]d previous/next diagnostic.
vim.api.nvim_create_autocmd("LspAttach", {
  group = vim.api.nvim_create_augroup("rick_lsp", { clear = true }),
  callback = function(ev)
    local buf = ev.buf
    local function map(lhs, rhs, desc, mode)
      vim.keymap.set(mode or "n", lhs, rhs, { buffer = buf, desc = desc })
    end
    map("gd", function() require("config.define").definition() end, "Go to definition")
    map("gD", vim.lsp.buf.declaration, "Go to declaration")
    map("gy", vim.lsp.buf.type_definition, "Go to type definition")
    -- Ctrl+click: jump to what's under the mouse (header on an #include line,
    -- or a function/class/variable), like Cmd-click in CLion; on the name
    -- where it's declared or defined, its usages instead (config/usages.lua).
    -- Ctrl+o jumps back, as with gd.
    map("<C-LeftMouse>", "<LeftMouse><cmd>lua require('config.usages').ctrl_click()<cr>", "Go to definition / show usages (click)")
    map("<leader>cu", function() require("config.usages").show() end, "Show usages")
    map("<leader>cr", vim.lsp.buf.rename, "Rename symbol")
    map("<leader>ca", vim.lsp.buf.code_action, "Code action", { "n", "v" })
    map("<leader>th", function()
      vim.lsp.inlay_hint.enable(not vim.lsp.inlay_hint.is_enabled({ bufnr = buf }), { bufnr = buf })
    end, "Toggle inlay hints")

    local client = vim.lsp.get_client_by_id(ev.data.client_id)
    if client and client.name == "clangd" then
      map("<leader>o", "<cmd>LspClangdSwitchSourceHeader<cr>", "Switch source/header")
      -- Parameter names before arguments, like CLion ("x: 1"); Space th toggles.
      vim.lsp.inlay_hint.enable(true, { bufnr = buf })
    end
  end,
})

-- No format-on-save on purpose: reformatting decades-old files would bury
-- real changes in whitespace diffs.
