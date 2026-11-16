-- AI code completion, like CLion's full-line completion: when you stop typing
-- for a moment, a local model suggests how the line (or the next few lines)
-- goes on, in grey after the cursor. Tab accepts it, Alt+Right accepts the
-- next word, and anything else you type carries on as normal (the suggestion
-- goes away until you pause again). Space tA turns it on and off (remembered).
--
-- It asks Ollama with fill-in-the-middle (the code before and after the
-- cursor), so it needs a coder model that knows FIM. Small ones answer fast
-- enough to keep up with typing; Qwen 27B is too slow for this (use it in the
-- prompt window, Space ap):
--   vim.g.ai_complete_model = "qwen2.5-coder:3b"   -- default; 7b is better and slower
--   vim.g.ai_complete_url   = "http://127.0.0.1:11434"
-- When Ollama isn't running nothing is shown (the local model checks, Space tm,
-- start it).

local M = {}

local ns = vim.api.nvim_create_namespace("rick_ai_complete")
local timer = vim.uv.new_timer()
local job, shown -- the request in flight; { buf, row, col, text } on screen
local gen = 0

local function enabled() return require("config.prefs").get("ai_complete") end
local function model() return vim.g.ai_complete_model or "qwen2.5-coder:3b" end
local function url() return (vim.g.ai_complete_url or "http://127.0.0.1:11434"):gsub("/$", "") end

function M.clear()
  if shown and vim.api.nvim_buf_is_valid(shown.buf) then vim.api.nvim_buf_clear_namespace(shown.buf, ns, 0, -1) end
  shown = nil
end

local function cancel()
  gen = gen + 1
  timer:stop()
  if job then pcall(function() job:kill(15) end) end
  job = nil
end

local function draw(buf, row, col, text)
  M.clear()
  local lines = vim.split(text, "\n", { plain = true })
  local virt_lines = {}
  for i = 2, #lines do virt_lines[#virt_lines + 1] = { { lines[i], "RickAIGhost" } } end
  pcall(vim.api.nvim_buf_set_extmark, buf, ns, row, col, {
    virt_text = { { lines[1], "RickAIGhost" } }, virt_text_pos = "inline",
    virt_lines = #virt_lines > 0 and virt_lines or nil, hl_mode = "combine", priority = 200,
  })
  shown = { buf = buf, row = row, col = col, text = text }
end

-- Keep what the model wrote to a sensible size: up to 6 lines, and not past
-- the end of the block it started in.
local function trim(text, after)
  text = text:gsub("\r", ""):gsub("%s+$", "")
  local lines = vim.split(text, "\n", { plain = true })
  if #lines > 6 then lines = vim.list_slice(lines, 1, 6) end
  text = table.concat(lines, "\n")
  -- Don't repeat what's already after the cursor.
  local rest = vim.trim(after)
  if rest ~= "" and text:sub(-#rest) == rest then text = text:sub(1, #text - #rest) end
  return text
end

local function request()
  local buf = vim.api.nvim_get_current_buf()
  if vim.fn.mode() ~= "i" or vim.bo[buf].buftype ~= "" or not vim.bo[buf].modifiable then return end
  local ok_cmp, cmp = pcall(require, "blink.cmp")
  if ok_cmp and cmp.is_visible() then return end
  local row, col = unpack(vim.api.nvim_win_get_cursor(0))
  local line = vim.api.nvim_get_current_line()
  local after = line:sub(col + 1)
  -- Only at the end of a line, or before closing brackets / punctuation.
  if not after:match("^[%s%)%]};,]*$") then return end
  local first = math.max(0, row - 80)
  local before = vim.api.nvim_buf_get_lines(buf, first, row - 1, false)
  before[#before + 1] = line:sub(1, col)
  local next_lines = vim.api.nvim_buf_get_lines(buf, row, math.min(row + 40, vim.api.nvim_buf_line_count(buf)), false)
  local prefix = table.concat(before, "\n")
  local suffix = after .. "\n" .. table.concat(next_lines, "\n")
  if vim.trim(prefix) == "" then return end
  local body = vim.json.encode({
    model = model(), prompt = prefix, suffix = suffix, stream = false,
    options = { temperature = 0.1, num_predict = 96, stop = { "\n\n\n" } },
  })
  local my = gen
  job = vim.system({ "curl", "-s", "--noproxy", "*", "-m", "8", url() .. "/api/generate", "-d", body }, { text = true }, function(r)
    vim.schedule(function()
      if my ~= gen then return end
      job = nil
      if r.code ~= 0 then return end
      local ok, j = pcall(vim.json.decode, r.stdout or "")
      if not ok or type(j) ~= "table" or type(j.response) ~= "string" then return end
      local text = trim(j.response, after)
      if text == "" or vim.fn.mode() ~= "i" or vim.api.nvim_get_current_buf() ~= buf then return end
      local r2, c2 = unpack(vim.api.nvim_win_get_cursor(0))
      if r2 ~= row or c2 ~= col then return end
      draw(buf, row - 1, col, text)
    end)
  end)
end

local function schedule()
  cancel()
  M.clear()
  if not enabled() then return end
  local my = gen
  timer:start(350, 0, vim.schedule_wrap(function()
    if my == gen then request() end
  end))
end

-- Accept the suggestion (or its next word). Returns false when there's none.
function M.accept(word)
  if not shown then return false end
  local s = shown
  local text = s.text
  if word then text = text:match("^%s*[%w_]+") or text:match("^%s*%S") or text end
  M.clear()
  cancel()
  local lines = vim.split(text, "\n", { plain = true })
  vim.api.nvim_buf_set_text(s.buf, s.row, s.col, s.row, s.col, lines)
  local last = #lines == 1 and (s.col + #lines[1]) or #lines[#lines]
  vim.api.nvim_win_set_cursor(0, { s.row + #lines, last })
  if word and #text < #s.text then
    -- Show what's left.
    local r, c = unpack(vim.api.nvim_win_get_cursor(0))
    draw(s.buf, r - 1, c, s.text:sub(#text + 1))
  end
  return true
end

function M.visible() return shown ~= nil end

function M.setup()
  local function hl() vim.api.nvim_set_hl(0, "RickAIGhost", { link = "Comment", default = true }) end
  hl()
  local group = vim.api.nvim_create_augroup("rick_ai_complete", { clear = true })
  vim.api.nvim_create_autocmd("ColorScheme", { group = group, callback = hl })
  vim.api.nvim_create_autocmd({ "TextChangedI", "TextChangedP" }, { group = group, callback = schedule })
  vim.api.nvim_create_autocmd({ "InsertLeave", "BufLeave" }, { group = group, callback = function() cancel(); M.clear() end })
  vim.api.nvim_create_autocmd("CursorMovedI", {
    group = group,
    callback = function()
      if shown then
        local r, c = unpack(vim.api.nvim_win_get_cursor(0))
        if r - 1 ~= shown.row or c ~= shown.col then cancel(); M.clear() end
      end
    end,
  })
  vim.keymap.set("i", "<M-Right>", function()
    if not M.accept(true) then vim.api.nvim_feedkeys(vim.keycode("<M-Right>"), "n", false) end
  end, { desc = "Accept the next word of the AI suggestion" })
  require("config.prefs").add_toggle({ "ai_complete", "<leader>tA", "Toggle AI completion (local model)", "AI completion (local model)",
    function() cancel(); M.clear() end })
end

return M
