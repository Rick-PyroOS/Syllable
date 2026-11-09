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
