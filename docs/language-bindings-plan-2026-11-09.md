# Language bindings for libos / libsyllable

Plan only, no code changed. Read from `~/Code/Rick-PyroOS/Syllable` (`main` at `6e25bcd0`, and `cairo` at `83e0a251`), plus `docs/lambda-event-handlers.md`. The message that started this asked about "other languages like" with no list, so this plan assumes **Python, Rust, Go and JavaScript/Node.js**. Adding another language (Lua, Ruby, Zig, Swift) is a stage-4 job once the C layer exists.

## 1. Short answer

Don't bind the C++ directly. Build one hand-written **C ABI layer** (`libos-c`, working name) over libos, and have every language bind that C layer with its normal C FFI tool. The C layer is maybe 3 to 5 weeks of work for a useful core (app, looper, message, window, view, a dozen widgets, files). Each language after that is 1 to 3 weeks of binding work, **but** each language first needs its runtime running on Pyro, and that is the larger cost for everything except Python and a small JS engine.

Recommended order: C layer, then Python (cheapest, proves the design), then Rust, then Go (via gccgo), then JavaScript (QuickJS first, Node later).

## 2. What there is to bind

**Two libraries, one name.** On `main`, `libos.so` is just a symlink to `libsyllable.so.8` (`libsyllable/Makefile:73`, `CMakeLists.txt:47`). On `cairo`, `system/sys/appserver/libos` is a new library, `libos.so.1`, built on cairo and pango, with headers under `include/pyro/pyro/` (about 340 headers). The lambda-handlers plan already targets that libos. **Bindings should target cairo's libos only**, and treat libsyllable as the frozen compatibility library.

**The C++ GUI/app kit** (namespace `os::`): `util/` (32 headers: Application, Looper, Handler, Message, Messenger, String, Settings, Thread...), `gui/` (58 headers: Window, View and the widgets), `storage/` (16), `translation/`, `media/`.

**The kernel/system API** (`atheos/*.h`, `syllable/*.h`, 39 headers): threads, ports (`msgport.h`), semaphores, areas, images, device and filesystem calls. These are already plain C functions exported from the Syllable glibc port. Any FFI tool can consume them as they are. The work here is in each language's **standard library** (Rust's `libc` crate, Go's `syscall` package, Python's `os`/`select` modules), not in a binding layer.

## 3. Why not bind the C++ directly

What the headers show:

| Problem | Evidence |
|---|---|
| Virtual methods are the main extension point | `View` has 61 virtuals, `Looper`/`Window`/`Application` override `HandleMessage`, `DispatchMessage`, `OkToQuit`, `Paint`, `MouseDown`... Bindings have to subclass C++ from the foreign side. |
| Object layout is public | `Handler` keeps `m_cName`, `m_pcLooper`, `m_cFilterList` inline (`util/handler.h`). `Looper` uses `Private* m`, but many classes don't. |
| STL types in signatures | 63 uses of `std::string/vector/list/map` in `gui/` and `util/` headers, such as `Looper::handler_map` (`std::map<int,Handler*>`) and `MsgFilterList`. |
| Exceptions | 80 `throw`s in libsyllable `.cpp` files (storage, settings, looper, thread, catalog). An exception crossing into Rust, Go, Python or V8 frames is undefined behaviour or an abort. |
| Compiler-specific ABI | Itanium mangling as GCC 9.5 does it, libstdc++'s dual `std::string` ABI (`_GLIBCXX_USE_CXX11_ABI`), and `std::function` in the new lambda API. Only GCC 9.5 C++ code can call this safely. |

Tools that bind C++ directly each solve part of this: pybind11 (Python, handles virtual overrides with trampolines), SWIG "directors" (Python, Go, some JS), Rust's `cxx` (no subclassing of C++ classes from Rust). Each needs its own glue, and each must be built with the same GCC. A C layer solves the hard parts once.

## 4. The C layer (`libos-c`)

### 4.1 Shape

```c
/* pyro/c/window.h */
typedef struct os_window os_window;            /* opaque */
typedef struct os_view   os_view;

typedef struct {
    void (*handle_message)(void* user, os_window*, const os_message*);
    int  (*ok_to_quit)(void* user, os_window*);
    void (*destroyed)(void* user);             /* C++ object is gone */
} os_window_callbacks;

os_status  os_window_create(const os_rect* frame, const char* title,
                            uint32_t flags, const os_window_callbacks*,
                            void* user, os_window** out);
os_status  os_window_add_child(os_window*, os_view*);   /* takes ownership */
os_status  os_window_show(os_window*, int show);
os_status  os_window_lock(os_window*);
void       os_window_unlock(os_window*);
```

Rules:

1. **Opaque handles only.** No C++ struct layout crosses the boundary. Small value types (`os_point`, `os_rect`, `os_color`) are plain C structs passed **by pointer**, which avoids i386 struct-return quirks between compilers.
2. **Virtuals become callback tables.** For each bindable class there is one internal C++ subclass (`CWindow : os::Window`, `CView : os::View`) that overrides every virtual and forwards to the table plus a `void* user`. A null entry calls the base class. With the lambda API from `lambda-event-handlers.md`, most view events (`on_click`, `on_mouse_enter`, `on_timer_tick`) need no subclass at all: the C layer just registers a lambda that calls the C function pointer.
3. **No exceptions out.** Every entry point is `try { ... } catch (...)` and returns an `os_status` (negative errno style, matching `status_t`). `os_last_error_message()` gives thread-local detail text.
4. **Strings** are UTF-8 `const char*` in, and caller-owned buffers or `os_string*` handles out. No `std::string` crosses.
5. **`os_message`** gets typed accessors (`os_message_add_int32`, `os_message_find_string`, ...). It already flattens to bytes (`Message::Flatten`), so languages can also move whole messages as byte buffers.
6. **Constants** (message codes, `typecodes.h`, window flags, `CF_FOLLOW_*`, key codes) are generated from the headers by a script into `pyro/c/constants.h`, so they never drift.
7. **Versioning.** `libos-c.so.1` with a GNU symbol version script (`LIBOS_C_1.0`). Additions go in new version nodes; nothing is ever removed in `.so.1`. ABI checked with `abidiff` in CI.

### 4.2 Ownership and lifetime

libos has its own ownership rules, and the C layer has to state them per function:

- A **View** belongs to its parent once `AddChild` is called, and the window deletes its tree. Functions that take ownership say so in their name or doc (`os_window_add_child` "takes ownership").
- A **Window/Looper** deletes itself when it quits. The foreign object must not free it, and must not touch it after it has quit.
- **Messages** passed to `PostMessage` are copied, so the caller keeps its copy. (To confirm per call in `looper.cpp` before writing the header.)

To make this safe in garbage-collected languages, every handle the C layer hands out goes through a **handle table with generation counters**. When the C++ object is destroyed, the internal subclass's destructor marks the slot dead and calls `destroyed(user)`. Any later call with that handle returns `OS_ERR_DEAD_OBJECT` instead of using freed memory. The binding drops its reference to its own callback state in `destroyed`.

### 4.3 Threading

This is the hardest part. Every `Window` is a `Looper` with **its own thread**, and `HandleMessage`/`Paint`/`MouseDown` run on that thread with the window locked. Code on another thread must `Lock()` the window before touching its views. The C layer can't hide that, but it can make it explicit:

- Callbacks always say which thread they run on (the window's thread).
- `os_window_lock` / `unlock`, plus `os_looper_post(looper, msg)` for "run this on that window's thread".
- An optional **`os_app_run_on_main(fn, user)`** helper, so runtimes that want one UI thread can funnel work there.

How each runtime copes:

| Runtime | Callbacks from a Looper thread | Consequence |
|---|---|---|
| Python | Must take the GIL (`PyGILState_Ensure`). ctypes/cffi do this for you. | Works. Only one window runs Python code at a time, which is fine for UI. |
| Rust | Callbacks are plain `extern "C"` fns. | Closures stored for a window must be `Send + 'static`. A `WindowGuard` type from `lock()` gives safe access to views. Fits well. |
| Go | gc and gccgo both allow C threads to call into Go (cgo registers the thread). | Works, a little slower per call. Idiomatic Go wrapper can turn messages into a channel. |
| Node.js | JS runs on one thread only. Callbacks from other threads must go through `napi_threadsafe_function` and run later. | `Paint` and `OkToQuit` need an answer **now**, so the window thread would block waiting for JS, and deadlocks if JS is holding that window's lock. Needs a different model (section 6.4). |

## 5. Generators: what to use per language

All of them read the C headers. None needs to understand C++.

| Language | Raw layer | Idiomatic layer on top |
|---|---|---|
| Python | `cffi` (ABI mode, no compiler needed on Pyro) or `ctypes` | `pyro` package: `Window`, `View` classes with Python methods mapped to the callback table |
| Rust | `bindgen` → `pyro-sys` crate (run on the Mac, check the output in, so Pyro doesn't need libclang) | `pyro` crate: RAII handles, `Result<_, os::Error>`, `WindowGuard`, closures for events |
| Go | `cgo` (`import "C"` against `pyro/c/*.h`) | `pyro` package: `Window`, channels for messages |
| JavaScript | QuickJS: hand-written or generated C module. Node: N-API via `node-addon-api`, or the `koffi` FFI module | Promise/event-emitter API |

SWIG could generate Python, Go and JS from one interface file, but its JS back end targets V8/JSC internals rather than N-API's stable ABI (as far as I know; worth checking against current SWIG), and the output is less idiomatic. I'd only use it if the number of languages grows past four.

For the C layer itself, write the core (app, looper, message, window, view) by hand, because ownership and threading need thought. For the long tail of widgets (`Button::SetLabel`, `Slider::SetValue`...) a small libclang script can generate the plain non-virtual getters and setters from the headers. That's a stage-4 job.

## 6. Per-language prerequisites

The binding is the easy half. Each language must first run on Pyro (32-bit i386, Syllable glibc port, GCC 9.5). The toolchain audit for Rust, Go and Node is happening in another thread; this section only lists what the bindings depend on.

### 6.1 Python
- Only Python **2.5.1** is ported (`Pyro/Ported/Development/python`). Needs a CPython 3.x port. CPython is plain C99 plus POSIX, so the work is mostly `configure` fixes and whatever the POSIX audit (`docs/posix-audit-2026-10-30.md`) found missing. **libffi** must be ported too, for ctypes/cffi.
- **Cost: low to medium.** Best first target.

### 6.2 Rust
- No Syllable target in rustc. Needs a custom target spec (`i686-unknown-syllable`, based on `i686-unknown-linux-gnu`), a port of the `libc` crate (types, errno, syscalls), and `std::sys::unix` cfg branches. Cross-compile from the Mac with `-Zbuild-std`; a native rustc on Pyro is a much later goal.
- A `no_std` + `alloc` path gets GUI demos running before `std` is fully ported.
- **Cost: medium to high**, mostly the `std` port.

### 6.3 Go
- gc has no Syllable `GOOS`. **gccgo ships with GCC**, and GCC 9.5 is already ported, so the cheaper path is gccgo plus a **libgo** port (its `syscall` and `runtime` OS layers). libgo needs working pthreads, `mmap`, signals and `getcontext/makecontext` or a split-stack replacement.
- gccgo in GCC 9.5 is Go 1.12 era (no generics). That's fine for bindings.
- **Cost: medium to high**, mostly libgo.

### 6.4 JavaScript
- **Node.js** needs V8 (32-bit ia32, executable memory, signals, threads) plus libuv (needs `poll`/`epoll`-like support, which Pyro lacks). Recent Node also requires a newer GCC than 9.5 (I believe Node 20+ wants GCC 10+; check against the toolchain thread). **Cost: very high.**
- **QuickJS** (or QuickJS-ng) is one portable C library, embeds in a day, and can call the C layer directly. Because it's embedded, a JS context can live **on each window's thread**, which removes the Node threading problem from section 4.3.
- Recommendation: QuickJS now for scripting and small apps; Node only if the toolchain thread finds a realistic path. For Node, run all windows' callbacks through a single "UI" thread using `os_app_run_on_main`, and make `Paint` draw from state JS prepared earlier, not call into JS synchronously.

## 7. ABI under GCC 9.5

- The C layer **must** be built with the same GCC 9.5 and libstdc++ as libos. It is the only C++ code that touches libos symbols.
- Everything above the C layer sees the plain i386 System V (cdecl) C ABI, which bindgen, cgo, cffi and N-API all handle.
- Avoid `int64` fields inside structs shared across the boundary: alignment of 64-bit ints in structs differs between compilers on i386. Pass `bigtime_t` as a separate argument instead.
- Keep `libos-c` linked against a **shared** libstdc++ so it agrees with libos.
- If the lambda-handlers API lands with `std::function` in libos's ABI, the C layer is unaffected, because it's the only caller.

## 8. Staged plan

| Stage | Work | Done when | Rough size |
|---|---|---|---|
| **0. Decide** | Confirm the language list, confirm libos (`cairo`) is the target, pick the C layer's name and header path. | Answers written in this doc. | Hours |
| **1. C layer core** | `os_app`, `os_looper`, `os_message`, `os_messenger`, `os_window`, `os_view` (Paint, mouse, key), `Button`, `StringView`, `TextView`, `LayoutView`. Handle table, error model, constants generator, version script. A **C sample app** ("hello window" with a button and a counter) plus a unit test that posts messages across threads. | C sample runs on Pyro; `abidiff` baseline saved. | 3–5 weeks |
| **2. Python** | Port CPython 3.x and libffi. `pyro` package on cffi. Port the C sample to Python. | Python hello-window runs; a callback from a second window works with the GIL. | 2–4 weeks (mostly the port) |
| **3. Rust** | Target spec and `libc` port (cross from Mac). `pyro-sys` with bindgen, `pyro` safe crate. | Rust hello-window cross-compiled and running. | 4–8 weeks (mostly `std`) |
| **4. Wider C layer** | Rest of the widgets (libclang generator for simple accessors), `storage/` (File, Directory, NodeMonitor), `Settings`, `Clipboard`, translation. Lambda event API exposed as C callbacks. | Each new header has tests in C and Python. | Ongoing |
| **5. Go** | libgo port for gccgo 9.5, then `pyro` cgo package. | Go hello-window runs. | 4–8 weeks |
| **6. JavaScript** | QuickJS embedded with a `pyro` module, one context per window thread. Node later if the toolchain allows. | `pyro-js app.js` opens a window. | 1–2 weeks for QuickJS |
| **7. Stability** | ABI check in CI, docs generated from the C headers, a sample per language in the image. | CI fails on an accidental ABI break. | 1 week |

Stages 2, 3 and 5 can run in parallel once stage 1 is done, since each depends only on the C layer and its own runtime port.

## 9. Open questions

1. Which languages do you actually want? This plan assumed Python, Rust, Go and JS.
2. Should bindings cover libsyllable (`.so.8`) as well, for old apps? I'd say no.
3. Is it OK for the C layer to require the lambda API from `lambda-event-handlers.md`? It would make the view-event part much smaller.
4. Do the bindings need to run natively on Pyro (compilers on Pyro), or is cross-compiling from the Mac enough for now? It mostly changes the Rust and Go cost.

## 10. Dock plugins (added 2026-11-10)

Checked on 2026-11-10 against `main` and `cairo` in `~/Code/Rick-PyroOS/Syllable` (the Dock and plugin API are the same on both). The guesses in the first draft held up; this section adds what the code actually does.

### 10.0 How Dock plugins work today

- **Header:** `system/sys/include/syllable/appserver/dockplugin.h` (cairo also has a copy at `pyro/pyro/appserver/dockplugin.h`). `class os::DockPlugin` has pure virtuals `Initialize()`, `Delete()` and `GetIdentifier()`, two optional hooks `ScreenModeChanged()` and `DesktopActivated()`, and helpers `AddView(os::View*)`, `RemoveView(view, thread)`, `GetApp()` (the Dock window), `GetPath()`.
- **Entry point:** one `extern "C" DockPlugin* init_dock_plugin()`. There is **no API version** constant or check. Clock, CpuSpeed, Address and Camera declare it with `(os::Path, os::Looper*)` arguments, but the Dock calls it with none, so those arguments are garbage (unused, so harmless today).
- **Loading:** `DockWin::AddPlugin()` (`dock/Dock.cpp:852`) accepts only `.so` files in `/system/extensions/dock` (or `/boot/system/extensions/dock`), then calls `load_library(..., IM_ADDON_SPACE)`, `get_symbol_address("init_dock_plugin")`, `SetApp(this, id)` and `Initialize()`. The list of plugins comes from the Dock's settings and the `os/Dock/SetPlugins` event; there's no folder scan. A plugin whose `Initialize()` fails is leaked, not unloaded.
- **Threading:** plugins are not Loopers. `Initialize()`, `Delete()`, the hooks, and every plugin view's `Paint`/`MouseDown`/`TimerTick` run on the **Dock window's thread**, because `AddView` makes the view a child of the Dock's own view (`Dock.cpp:699`). Plugins that need background work bring their own thread (Clock's `os::Thread`, Eyes' own `Looper`), and `RemoveView(view, thread)` makes the Dock wait for that thread before unloading.
- **Existing plugins:** Address, battery, Camera, Clipper, clock, ColdFishRemote, CpuSpeed, Eyes, Meter, mixer, Printer, Switcher, WallpaperChanger. Meter is the smallest (`dock_plugins/Meter/meter.cpp:324`): a 10-line `MeterPlugin` plus a `Meter : os::View` driven by `GetApp()->AddTimer()`.
- **Dock2** (`~/Code/Rick-PyroOS/Dock2`, an old CVS tree) has a different, richer, ABI-incompatible API in `libdock.so` (namespace `dock`): `GetInfo()` with name/version/author, `DockView` with context menus and drag support, dock colours, size, position and font hooks. It's only partly migrated (5 plugins).
- **Bugs seen on the way:** `SetViewCount()` is declared `int` with no `return`, and `case DOCK_SET_POSITION` (`Dock.cpp:819`) has no `break`, so it falls through into `DOCK_REMOVE_PLUGIN`.

**What this changes in the plan:** the C adapter in D1 is even smaller than estimated, since `DockPlugin` has only five virtuals. D1 should also add an **API version** (a second exported symbol, `dock_plugin_api_version()`, that the Dock checks if present, so old plugins still load) and fix the two bugs. If you want Dock2's richer API (`GetInfo`, context menus, colours), design the C API around it now, so the bindings don't need a second version later.

### 10.1 What's different from apps

An app owns its process. A Dock plugin is a guest in the Dock's process, which adds three problems on top of section 3:

1. **One crash takes down the Dock.** A Python exception that isn't caught, a Rust panic crossing the boundary, or a Go runtime fault kills every other plugin too.
2. **Runtimes have to share one process.** Python can only have one interpreter per process (sub-interpreters help, but they're fragile with C extensions). Go allows **only one Go runtime per process**, so two Go plugins built as separate `.so` files can't both load. Node can't be embedded twice either.
3. **Plugins run on the Dock's thread.** `Paint`, `MouseDown` and timer ticks run on the Dock window's Looper thread. A slow plugin, or a garbage collection pause, freezes the whole Dock.

### 10.2 Three ways to do it

| | How it works | Good for | Weak spots |
|---|---|---|---|
| **A. Native C ABI** | A small static library, `libdockplugin-c.a`, supplies the Dock's C++ entry point and a `DockPlugin` subclass that forwards to a C callback table (same idea as section 4). The language builds a `.so` that links it. | C, Rust (a `cdylib` has no runtime, so it's as safe as C++ if panics are caught at the boundary), Zig | Go (one runtime per process); anything with a heavy runtime |
| **B. Loader plugins** | One native plugin per language, such as `python.so` or `quickjs.so`. It embeds the runtime once and loads every script in `~/Settings/Dock/plugins/python/*.py` as a separate Dock item. | Python and QuickJS: plugins become single script files, easy to write and share | One bad script can still hang the loader; Python needs a timeout watchdog |
| **C. Out-of-process** | The plugin is its own program. The Dock shows a placeholder view, and the two talk with `os::Message`s: the plugin sends an icon or bitmap (in a shared area from `create_area()` for anything that updates often), a tooltip and a menu; the Dock sends clicks, size changes and theme changes back. | Go, Node, anything. A crash only kills that plugin, and the Dock can restart it | More memory per plugin; drawing is "send a picture", not live drawing; a few ms latency on clicks |

### 10.3 Recommendation

- **Do A first**, as part of stage 1. It's small (a few hundred lines) once the C layer exists, and it gives Rust and C plugins straight away.
- **Then B for Python and QuickJS.** For most Dock items (a meter, a clock, a battery or VPN indicator) a 30-line script is the right size. The loader runs each script's updates on a worker thread and only posts finished bitmaps to the Dock thread, so a slow script can't freeze the bar.
- **Then C**, as the only route for Go and Node, and as an option for any plugin you don't trust. It fits the init server design: an out-of-process plugin can be a `SESSION` service that init restarts if it crashes, and the Dock indicator and notification history plugins from `init-server-design-2026-11-08.md` could be built this way.
- **Version the plugin ABI** separately from `libos-c` (`DOCK_PLUGIN_C_API 1`), and have the Dock refuse plugins whose version it doesn't know, so an old plugin fails cleanly rather than crashing.

### 10.4 Added stages

| Stage | Work | Done when | Rough size |
|---|---|---|---|
| **D1** | Add the API-version symbol and fix the two Dock bugs; write `libdockplugin-c.a` plus `pyro/c/dock.h`; port one existing plugin (Meter or the clock) to C through it. | The C version of the plugin loads and works next to the C++ ones. | 1 week, after stage 1 |
| **D2** | Rust `pyro-dock` crate with a `#[dock_plugin]` macro; a Rust sample plugin. | Rust plugin loads in the Dock. | 3–5 days, after stage 3 |
| **D3** | JavaScript loader plugin on QuickJS, with a worker thread and a watchdog (see 10.5). | A `.js` file dropped in the folder shows up in the Dock. | 1.5–2 weeks, after D1 |
| **D3b** | Python loader plugin, reusing the D3 code with CPython (see 10.5). | A `.py` file dropped in the folder shows up in the Dock. | 1 week, after D3 and stage 2 |
| **D4** | Out-of-process protocol, a client library in C (and so every language), Dock placeholder view, restart through init. | A Go plugin runs, and `kill -9` on it leaves the Dock running and the plugin restarts. | 2–3 weeks |

### 10.5 Example: a CPU meter Dock plugin in JavaScript and Python

A 24×24 bar that shows CPU load, updates twice a second, shows the load in its tooltip, and opens a monitor app when clicked. **None of this exists yet.** It shows what route B would give you, first with **QuickJS** (section 6.4, no Node port needed), then with **Python**. Both loaders work the same way.

#### The plugin in JavaScript

One file, `~/Settings/Dock/plugins/js/cpu-meter.js`:

```js
import { DockItem, launch } from "pyro:dock";
import { cpuLoad } from "pyro:sys";

export default class CpuMeter extends DockItem {
  static id = "JsMeter";
  static size = [24, 24];
  static interval = 500;          // ms between update() calls

  load = 0;

  // Runs on the loader's worker thread, never on the Dock's thread.
  update() {
    this.load = cpuLoad();        // 0.0 .. 1.0
    this.tooltip = `CPU ${Math.round(this.load * 100)}%`;
  }

  // Also on the worker thread. `g` draws into an offscreen bitmap,
  // which the loader copies into the Dock in one step.
  paint(g) {
    const [w, h] = this.size;
    g.fillStyle = "rgb(40,40,40)";
    g.fillRect(0, 0, w, h);
    g.fillStyle = "rgb(80,200,120)";
    g.fillRect(0, h - h * this.load, w, h * this.load);
  }

  clicked(button) {
    launch("/Applications/Monitor");
  }

  // Optional menu, shown on right-click.
  menu() {
    return [
      { label: "Update every second", checked: this.interval === 1000,
        action: () => { this.interval = this.interval === 1000 ? 500 : 1000; } },
      { label: "Open Monitor", action: () => launch("/Applications/Monitor") },
    ];
  }
}
```

`g` is a small canvas-like API (`fillStyle`, `fillRect`, `strokeRect`, `drawImage`, `fillText`) over the C layer's view drawing calls, so the code reads like web JavaScript. Drop the file in the folder and the item appears in the Dock; delete it and the item goes away.

#### The same plugin in Python

One file, `~/Settings/Dock/plugins/python/cpu_meter.py`, run by a `Python.so` loader built exactly like the JavaScript one below (one interpreter on a worker thread, drawing into an offscreen bitmap). It needs the CPython 3 and libffi ports from stage 2.

```python
from pyro import dock, sys

class CpuMeter(dock.Item):
    id = "PyMeter"
    size = (24, 24)
    interval = 0.5          # seconds between update() calls

    def __init__(self):
        self.load = 0.0

    # Runs on the loader's worker thread, never on the Dock's thread.
    def update(self):
        self.load = sys.cpu_load()          # 0.0 .. 1.0
        self.tooltip = f"CPU {self.load:.0%}"

    # Also on the worker thread: `g` draws into an offscreen bitmap.
    def paint(self, g):
        w, h = self.size
        g.fill_rect((0, 0, w, h), (40, 40, 40))
        g.fill_rect((0, h - h * self.load, w, h), (80, 200, 120))

    def clicked(self, button):
        dock.launch("/Applications/Monitor")

    def menu(self):
        return [
            dock.MenuItem("Update every second", checked=self.interval == 1.0,
                          action=lambda: setattr(self, "interval", 0.5 if self.interval == 1.0 else 1.0)),
            dock.MenuItem("Open Monitor", action=lambda: dock.launch("/Applications/Monitor")),
        ]
```

Differences from the JavaScript loader: the watchdog can't interrupt Python as cleanly as QuickJS's interrupt handler, so it uses `PyThreadState_SetAsyncExc` to raise a `TimeoutError` in a call that runs over a second, which doesn't stop code stuck inside a C extension. Every script shares one interpreter, so a script that changes global module state can affect the others.

#### What the loaders do (JavaScript shown)

`JavaScript.so` is an ordinary C++ Dock plugin in `/system/extensions/dock/`, listed once in the Dock settings. It's built on `libdockplugin-c.a` (stage D1) and QuickJS:

1. On `Initialize()` it starts one **worker thread** with one QuickJS runtime, registers the `pyro:dock` and `pyro:sys` modules, and loads every `*.js` file in `~/Settings/Dock/plugins/js/`. Each default-exported `DockItem` class becomes one Dock item, with one placeholder `os::View` added through `AddView()`.
2. Every `interval` ms it calls `update()` then `paint()` for that item **on the worker thread**, into an offscreen `os::Bitmap`, then posts the finished bitmap to the Dock window. The placeholder view's `Paint()` only copies that bitmap, so the Dock's thread never runs JavaScript.
3. Clicks and menu choices from the Dock thread are posted back to the worker, which calls `clicked()` or the menu `action`.
4. **Watchdog:** QuickJS's interrupt handler stops any call that runs longer than one second. An exception, or a stopped call, puts a small red mark on that item and the error in its tooltip. The Dock and the other items keep running.
5. It watches the folder with `os::NodeMonitor`, so new, changed or removed scripts are loaded, reloaded or removed without restarting the Dock.
6. On `Delete()` it removes the views, stops the worker, and passes the thread to `RemoveView(view, thread)` so the Dock waits for it before unloading.

#### What the loaders don't do

- **Not Node.** There is no `require`, npm or Node API. Pure-JS npm packages with no Node dependencies can be copied in as ES modules.
- **No live drawing on the Dock thread.** Everything is drawn ahead and copied in, which is fine for meters, clocks and indicators. A plugin that needs to react to every mouse move would need the native route.

#### Stage

D3 in section 10.4 covers JavaScript: QuickJS port (plain C99, should be a day or two), the `JavaScript.so` loader, the `pyro:dock` / `pyro:sys` modules and the canvas-like drawing API, then the watchdog and folder watching. **About 1.5 to 2 weeks after D1.** The cpu-meter script above is the test: it should run, survive an infinite loop added to `update()`, and reload when edited. The Python loader (D3b) reuses the same C++ code with CPython in place of QuickJS: **about 1 week after D3 and the stage 2 Python port.**
