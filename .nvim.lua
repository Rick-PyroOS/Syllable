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

-- Build (Cmd+F9, Space rb) with Syllable-Build-System/os/pyro, which builds in
-- Docker (syllable-os-x86) from a copy of system/ in the pyro-work volume.
-- `pyro build` only copies system/ the first time, so sync first: it brings
-- your edits over and keeps the objs/ folders, so make rebuilds only what changed.
-- Compiler errors name the container's paths; build_path_map turns them into
-- this checkout's. Docker must be running.
local pyro = vim.fs.dirname(root) .. "/Syllable-Build-System/os/pyro"
vim.g.build_cmd = pyro .. " sync && " .. pyro .. " build"
vim.g.build_path_map = {
  ["/work/src/"] = root .. "/system/",                                         -- the build copy of system/
  ["/work/kinc/syllable/"] = root .. "/system/sys/include/syllable/atheos/",  -- kernel-side <syllable/...>
  ["/repo/"] = vim.fs.dirname(root) .. "/",                                    -- Rick-PyroOS, read-only
}
-- Recompile (Cmd+Shift+F9, Space rf): pyro builds components, not single files,
-- so this builds the component (os/components) the current file belongs to.
vim.g.build_file_cmd = function()
  local file = vim.api.nvim_buf_get_name(0)
  local rel = vim.fs.relpath(root .. "/system", file)
  local best, best_len = nil, 0
  local manifest = vim.fs.dirname(root) .. "/Syllable-Build-System/os/components"
  for _, l in ipairs(vim.fn.filereadable(manifest) == 1 and vim.fn.readfile(manifest) or {}) do
    local name, kind, path = l:gsub("#.*", ""):match("^(%S+)%s+(%S+)%s+(%S+)")
    if rel and kind == "make" and (rel == path or rel:sub(1, #path + 1) == path .. "/") and #path > best_len then
      best, best_len = name, #path
    end
  end
  if not best then
    vim.notify("This file isn't in a pyro component (Syllable-Build-System/os/components): building everything")
    return vim.g.build_cmd
  end
  return pyro .. " sync && " .. pyro .. " build " .. best
end
