-- CMake, like CLion's: a project with a CMakeLists.txt at its root builds with
-- CMake. Cmd+F9 configures it the first time (into cmake-build-debug, as CLion
-- does) and then builds; the output goes to the Build view (in Run) as with make.
-- Commands (also in Find > Actions):
--   :CMakeReload        configure again (CLion's Reload CMake Project)
--   :CMakeBuild [tgt]   build everything, or one target (Tab completes them)
--   :CMakeTarget        pick a target to build from a list
--   :CMakeProfile       switch Debug / Release / RelWithDebInfo / MinSizeRel
--   :CMakeClean         build the clean target
-- Configuring exports compile_commands.json and links it at the project root
-- for clangd, unless the root already has a real compile_commands.json of its
-- own (Syllable's), which is left alone.
-- A project's .nvim.lua can set:
--   vim.g.cmake_build_dir = "build"            -- default: cmake-build-<profile>
--   vim.g.cmake_args = { "-DFOO=ON" }          -- extra configure arguments
--   vim.g.cmake_generator = "Ninja"            -- default: Ninja if installed
-- CMakeLists.txt and *.cmake files get highlighting, completion and hover
-- (neocmakelsp) and Space cf / Cmd+Alt+L formatting (cmake-format).

local M = {}

local PROFILES = { "Debug", "Release", "RelWithDebInfo", "MinSizeRel" }

local function root()
  return vim.g.build_dir or vim.fs.root(0, { ".nvim.lua", ".git" }) or vim.fn.getcwd()
end

function M.is_project(dir) return vim.uv.fs_stat((dir or root()) .. "/CMakeLists.txt") ~= nil end

local function profile() return vim.g.cmake_profile or "Debug" end

local function build_dir(dir)
  local d = vim.g.cmake_build_dir or ("cmake-build-" .. profile():lower())
  return d:sub(1, 1) == "/" and d or ((dir or root()) .. "/" .. d)
end

local function have_cmake()
  if vim.fn.executable("cmake") == 1 then return true end
  vim.notify("cmake isn't installed. On a Mac: brew install cmake ninja", vim.log.levels.WARN, { title = "CMake" })
  return false
end

local function q(s) return vim.fn.shellescape(s) end

-- The shell command that configures: cmake -S -B, then the compile_commands link.
local function configure_cmd(dir)
  local bdir = build_dir(dir)
  local gen = vim.g.cmake_generator or (vim.fn.executable("ninja") == 1 and "Ninja" or nil)
  local parts = { "cmake", "-S", q(dir), "-B", q(bdir), "-DCMAKE_BUILD_TYPE=" .. profile(), "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" }
  if gen then vim.list_extend(parts, { "-G", q(gen) }) end
  for _, a in ipairs(vim.g.cmake_args or {}) do parts[#parts + 1] = q(a) end
  local cc = dir .. "/compile_commands.json"
  -- Ask CMake's file API for the target list (read by M.targets).
  local query = bdir .. "/.cmake/api/v1/query"
  return ("mkdir -p %s && touch %s && "):format(q(query), q(query .. "/codemodel-v2")) .. table.concat(parts, " ")
    .. (" && { [ ! -e %s ] || [ -L %s ]; } && ln -sfn %s %s || true"):format(q(cc), q(cc), q(bdir .. "/compile_commands.json"), q(cc))
end

local function build_cmd(dir, target)
  local bdir = build_dir(dir)
  local cores = #vim.uv.cpu_info()
  local b = ("cmake --build %s -j %d"):format(q(bdir), cores) .. (target and (" --target " .. q(target)) or "")
  if vim.uv.fs_stat(bdir .. "/CMakeCache.txt") then return b end
  return configure_cmd(dir) .. " && " .. b
end

-- What build.lua runs for Cmd+F9 in a CMake project (nil when it isn't one).
function M.default_build(dir)
  if not M.is_project(dir) or vim.fn.executable("cmake") ~= 1 then return nil end
  return build_cmd(dir)
end

function M.reload()
  if not have_cmake() then return end
  local dir = root()
  if not M.is_project(dir) then return vim.notify("No CMakeLists.txt at " .. vim.fn.fnamemodify(dir, ":~"), vim.log.levels.WARN) end
  require("config.build").run(configure_cmd(dir), "configuring " .. profile())
end

function M.build(target)
  if not have_cmake() then return end
  local dir = root()
  if not M.is_project(dir) then return vim.notify("No CMakeLists.txt at " .. vim.fn.fnamemodify(dir, ":~"), vim.log.levels.WARN) end
  require("config.build").run(build_cmd(dir, target ~= "" and target or nil), target and ("building " .. target) or nil)
end

-- The build's targets, from CMake's file API (what the last configure wrote),
-- plus the usual all and clean.
function M.targets()
  local reply = build_dir() .. "/.cmake/api/v1/reply"
  local index = vim.fn.glob(reply .. "/codemodel-v2-*.json", true, true)[1]
  if not index then return {} end
  local ok, model = pcall(vim.json.decode, table.concat(vim.fn.readfile(index), "\n"))
  local config = ok and model.configurations and model.configurations[1]
  if not config then return {} end
  local out = {}
  for _, t in ipairs(config.targets or {}) do out[#out + 1] = t.name end
  table.sort(out)
  table.insert(out, 1, "all")
  out[#out + 1] = "clean"
  return out
end

function M.pick_target()
  local list = M.targets()
  if #list == 0 then
    return vim.notify("No targets yet: configure first with :CMakeReload", vim.log.levels.WARN, { title = "CMake" })
  end
  vim.ui.select(list, { prompt = "Build target (" .. profile() .. ")" }, function(t) if t then M.build(t) end end)
end

function M.pick_profile()
  vim.ui.select(PROFILES, { prompt = "CMake profile (now " .. profile() .. ")" }, function(p)
    if not p then return end
    vim.g.cmake_profile = p
    vim.notify("CMake profile: " .. p .. " (" .. vim.fn.fnamemodify(build_dir(), ":~:.") .. ")", vim.log.levels.INFO, { title = "CMake" })
  end)
end

function M.setup()
  local cmd = vim.api.nvim_create_user_command
  cmd("CMakeReload", M.reload, { desc = "Reload CMake project" })
  cmd("CMakeBuild", function(o) M.build(o.args) end,
    { nargs = "?", complete = function() return M.targets() end, desc = "Build with CMake (a target, or all)" })
  cmd("CMakeTarget", M.pick_target, { desc = "Build a CMake target" })
  cmd("CMakeProfile", M.pick_profile, { desc = "Switch CMake profile (Debug / Release)" })
  cmd("CMakeClean", function() M.build("clean") end, { desc = "Clean the CMake build" })
end

return M
