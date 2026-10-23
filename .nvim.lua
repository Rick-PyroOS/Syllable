-- Syllable settings for neovim-clion (Rick-PyroOS/neovim-clion), the CLion-style
-- Neovim setup. Neovim runs this when you start it in the Syllable checkout and
-- asks once whether to trust it. Install neovim-clion first: its install.sh.

-- Syllable file types.
vim.filetype.add({
  extension = { doxmod = "c", dox = "c", lnk = "ld" },
  filename = { ["Makeconf"] = "make", ["makefile.rules"] = "make", ["recurse.rules"] = "make" },
})

local root = vim.fn.fnamemodify(debug.getinfo(1, "S").source:sub(2), ":p:h")

-- Scratch buffers and files outside the tree still format in Syllable style.
vim.g.clang_format_fallback = root .. "/.clang-format"

-- The local-model errno check reads Syllable's errno.h (the default; set here
-- so it stays right if that default changes).
vim.env.NEOVIM_CLION_ERRNO_H = "system/sys/include/syllable/posix/errno.h"

-- cppcheck and semgrep findings are silenced the same way as the local model's
-- "// nomodel": "// nocppcheck: id -- note", "// nosemgrep", "-begin"/"-end".
vim.g.scanner_nomodel = true

-- Surround With (Cmd+Alt+T) puts braces on their own line, Syllable style.
vim.g.surround_brace_newline = true

-- Thread check (Space cx, Space cX, margin M): how Syllable starts threads,
-- which classes run on a looper thread, and how its code takes locks.
vim.g.thread_model = {
  -- spawn_thread( "name", func, ... ): func starts a thread called "name".
  spawn = { "spawn_thread", "spawn_kernel_thread", "create_thread" },
  -- request_irq( irq, handler, ... ): handler runs as an interrupt handler.
  irq = { "request_irq" },
  -- os::Looper and its subclasses have their own thread, which calls these.
  loopers = { "Looper", "Window", "Application", "Alert", "FileRequester", "ColorRequester", "FontRequester",
    "Dialog" },
  looper_callbacks = { "HandleMessage", "OkToQuit", "FrameMoved", "FrameSized", "WindowActivated",
    "ScreenModeChanged", "DesktopActivated", "TimerTick", "Idle", "Started", "Quit", "DispatchMessage", "Filter" },
  -- os::View and its subclasses belong to their window's thread, which calls these.
  views = { "View", "Control", "Button", "CheckBox", "RadioButton", "ListView", "TextView", "StringView", "Slider",
    "ProgressBar", "ScrollBar", "Menu", "MenuBar", "TabView", "LayoutView", "FrameView", "TreeView", "DropdownMenu",
    "Spinner", "ImageView", "ImageButton", "IconView", "ColorSelector", "ToolBar", "StatusBar", "SplitterView",
    "CheckMenu", "ImageMenuItem" },
  view_callbacks = { "Paint", "MouseDown", "MouseUp", "MouseMove", "KeyDown", "KeyUp", "FrameMoved", "FrameSized",
    "AttachedToWindow", "DetachedFromWindow", "AllAttached", "AllDetached", "Activated", "WheelMoved", "TimerTick",
    "HandleMessage", "ViewScrolled", "FontChanged", "GetPreferredSize", "Invoked", "SelectionChanged",
    "PostValueChange", "LabelChanged", "EnableStatusChanged" },
  -- An os::Handler that isn't a looper (a provider, a monitor's target) runs these
  -- on the thread of the looper it was added to.
  handlers = { "Handler" },
  handler_callbacks = { "HandleMessage", "TimerTick" },
  -- Lock calls and the call that ends each one (Looper::Lock, os::Locker::Lock, kernel locks).
  locks = { Lock = "Unlock", SafeLock = "Unlock", lock_semaphore = "unlock_semaphore", LOCK = "UNLOCK",
    spinlock = "spinunlock", spinlock_cli = "spinunlock_restore", mutex_lock = "mutex_unlock",
    pthread_mutex_lock = "pthread_mutex_unlock" },
  -- os::Locker / os::AutoLocker hold the lock for their scope.
  guards = { "Locker$" },
  atomic = { "^atomic_" },
  mutators = { "AddItem", "RemoveItem", "Clear" },
  member_prefix = "^m_", global_prefix = "^g_",
  lock_hint = "an os::Locker member",
  -- How to add a lock where there's none: an os::Locker member (its constructor
  -- needs a name), held for a scope by os::AutoLocker (util/locker.h).
  fix = { name = "m_cLock", decl = "os::Locker %s;", init = '%s( "%s" )', guard = "os::AutoLocker cLock( &%s );" },
  view_fix = "Lock it first: GetWindow()->Lock() ... Unlock(), or post the window a message.",
}

require("config.threads").setup()

-- Build, Run and Debug with Syllable-Build-System's ./sb, the CMake build in
-- Docker (the syllable-os-x86 image; Docker must be running). The checkout is
-- mounted read-only at /src and the build tree is the syllable-build volume at
-- /build, so compiler errors name those paths: build_path_map turns them into
-- this checkout's (and debug.lua uses it for the debugger's source paths).
--   Build (Cmd+F9, Space rb): ./sb build, which rebuilds what changed. Recompile
--     (Cmd+Shift+F9, Space rf) does the same: CMake builds targets, not files.
--   Run (Shift+F10, Space rr): ./sb image --debug (build, install, update the
--     disk image, with gdbserver starting at boot) then ./sb run --gdbserver,
--     QEMU in a window; the serial console (out/run-serial.log) is a tab in the
--     Run panel. Programs can be debugged while it runs.
--   Debug (Shift+F9, Space rd), by the file you're in:
--     - a kernel file (system/sys/kernel): QEMU stopped at the first
--       instruction, its gdb stub on localhost:1234, the kernel's symbols
--       (out/kernel.so);
--     - a program's file: that program (its CMakeLists.txt's syllable_program),
--       started in Syllable through gdbserver (localhost:2345) with the copy in
--       out/sysroot for its symbols. If Run's Syllable is up, it is debugged
--       there; if not, Debug starts one. :SyllableDebugApp picks the program
--       for files that aren't in one.
--   Source-level debugging of programs needs a build with debug info, once:
--     ./sb configure -DSYLLABLE_DEBUG_INFO=ON
--   and i386-elf-gdb (brew install i386-elf-gdb).
-- The old pyro build (Syllable-Build-System/os, being retired) is no longer used.
local sbs = vim.fs.dirname(root) .. "/Syllable-Build-System"
vim.g.build_dir = sbs
vim.g.build_cmd = "./sb build"
vim.g.build_file_cmd = "./sb build"
vim.g.build_path_map = {
  ["/src/"] = root .. "/",                                                     -- the checkout, read-only
  ["/repo/"] = vim.fs.dirname(root) .. "/",                                    -- Rick-PyroOS
}
vim.g.run_dir = sbs
vim.g.run_cmd = "./sb image --debug && ./sb run --gdbserver"
vim.g.run_logs = { "out/run-serial.log" }

-- The program a source file belongs to: the syllable_program() in the nearest
-- CMakeLists.txt above it with an INSTALL folder. { name, guest, local }:
-- its path inside Syllable and its copy with symbols in out/sysroot.
local function program_of(file)
  local dir = file ~= "" and vim.fs.dirname(file) or nil
  while dir and vim.startswith(dir, root .. "/system") do
    local cm = dir .. "/CMakeLists.txt"
    if vim.uv.fs_stat(cm) then
      local text = table.concat(vim.fn.readfile(cm), " ")
      local args = text:match("syllable_program%s*%((.-)%)")
      if args then
        local target = args:match("^%s*([%w_%-]+)")
        local out = args:match("OUTPUT_NAME%s+([%w_%-%.]+)") or target
        local inst = args:match("INSTALL%s+([%w_%-%./]+)")
        if inst then
          return { name = out, guest = "/boot/" .. inst .. "/" .. out, ["local"] = sbs .. "/out/sysroot/" .. inst .. "/" .. out }
        end
      end
    end
    local up = vim.fs.dirname(dir)
    if up == dir then break end
    dir = up
  end
end

local function is_kernel(file) return vim.startswith(file, root .. "/system/sys/kernel/") end

-- What Shift+F9 debugs: the kernel, the current file's program, or the one
-- picked last with :SyllableDebugApp.
local function debug_what()
  local file = vim.api.nvim_buf_get_name(0)
  if is_kernel(file) then return "kernel" end
  return program_of(file) or vim.g.syllable_debug_app
end

vim.g.debug_gdb = "i386-elf-gdb"
vim.g.debug_gdb_init = {
  "set architecture i386",
  -- Libraries: the image's, with symbols (gdbserver names them by file name)
  "set sysroot " .. sbs .. "/out/sysroot",
  "set solib-search-path " .. sbs .. "/out/sysroot/system/libraries:" .. sbs .. "/out/libraries",
}
vim.g.debug_target = function() return debug_what() == "kernel" and "localhost:1234" or nil end
vim.g.debug_remote = function()
  local w = debug_what()
  return type(w) == "table" and "localhost:2345" or nil
end
vim.g.debug_remote_exec = function()
  local w = debug_what()
  return type(w) == "table" and w.guest or nil
end
vim.g.debug_program = function()
  local w = debug_what()
  if w == "kernel" then return sbs .. "/out/kernel.so" end
  if type(w) == "table" then return w["local"] end
  vim.notify("Debug: open a file of the program to debug (or :SyllableDebugApp), or a kernel file", vim.log.levels.WARN)
end
vim.g.debug_cmd = function()
  return debug_what() == "kernel" and "./sb image && ./sb run --gdb" or "./sb image --debug && ./sb run --gdbserver"
end

-- :SyllableDebugApp: pick the program Shift+F9 debugs from files outside one.
vim.api.nvim_create_user_command("SyllableDebugApp", function()
  local apps, seen = {}, {}
  for _, cm in ipairs(vim.fs.find("CMakeLists.txt", { path = root .. "/system", type = "file", limit = math.huge })) do
    local p = program_of(cm)
    if p and not seen[p.guest] then seen[p.guest] = true; apps[#apps + 1] = p end
  end
  table.sort(apps, function(a, b) return a.name:lower() < b.name:lower() end)
  vim.ui.select(apps, { prompt = "Debug which program?", format_item = function(p) return p.name .. "  " .. p.guest end },
    function(p) if p then vim.g.syllable_debug_app = p; vim.notify("Shift+F9 debugs " .. p.guest) end end)
end, { desc = "Pick the program Shift+F9 debugs" })
