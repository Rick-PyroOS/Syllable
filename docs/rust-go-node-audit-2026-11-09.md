# Rust, Go and Node.js on Syllable (Pyro): what it takes

Date: 2026-11-09. Read-only audit. Nothing was built, changed or committed.

Sources read: `Syllable/system/sys/kernel/kernel/` (`elf.c`, `threads.c`, `exec.c`, `schedule.c`, `psignals.c`, `init.c`, `syscall.s`), `include/syllable/{atheos,syllable}/tld.h`, `kernel/tunables.h`, the glibc 2.7 Syllable sysdeps (`i386/sysdep.h`), `Syllable-Ports/gcc-9.5.0/gcc-9.5.0.patch` (`syllable.h` specs), `Syllable-Build-System/toolchain/scripts/gcc-common.sh`, `Syllable/docs/{gcc-upgrade,glibc-upgrade,cxx11-threads}.md`, and the project's `docs/posix-audit-2026-10-30.md` (IDs like **P0-6** below refer to that audit). Upstream facts come from Rust's `platform-support.md` and Node's `BUILDING.md` for v20.x, v22.x, v24.x and main.

## Summary

None of the three is blocked by Syllable being 32-bit. Rust, Go and V8 all still support 32-bit x86, and Haiku (BeOS-like, like Syllable) already has a Rust target and a libuv port to copy from. What blocks them is the same handful of kernel and libc gaps, plus, for Node only, the compiler.

- **Rust is the closest.** rustc runs on the Linux build host and only the target's standard library has to be ported, so LLVM never has to run on Syllable at first. The work is a target spec, a `libc` crate module, and `std` cfg changes. A cross-compiled "hello world" with `panic=abort` could come after fixing mmap, entropy and a few small libc gaps. Native rustc and cargo on Syllable come later, and should be cross-built, not bootstrapped on the box.
- **Go is a medium port, and gc (not gccgo) is the way.** It needs a new `GOOS=syllable` for `GOARCH=386`, roughly the size of the OpenBSD or AIX port. Syllable's syscall ABI is Linux-shaped (`int $0x80`, args in ebx/ecx/edx/esi/edi, `-4095..-1` errors), and its kernel semaphores fit Go's `lock_sema` model directly. Hard requirements on the kernel: a working `sigaltstack`, a real `SA_SIGINFO` ucontext with `si_addr`, and a fixed `MAP_FIXED`. gccgo in GCC 9.5 is Go 1.12 and can't build modern Go code, so it is not worth porting.
- **Node is the furthest away.** Node 22 needs GCC ≥ 10.1, Node 24 needs GCC ≥ 12.2, and Node main needs GCC ≥ 13.2. GCC 9.5 only builds Node 18, which is end-of-life. So Node waits for the GCC 15 port (roadmap step 2) with working C++11 threads. It also needs real AF_UNIX sockets, a real `poll`, rwlocks and POSIX semaphores for libuv, and a V8 platform file. Linux x86 is still "Experimental" in Node's own tier table, so expect to fix ia32 bit-rot too.

Recommended order: **shared kernel/libc foundation → Rust std (cross) → Go → Node**. Rust gives the quickest proof that the foundation works, and the libuv work for Node is shared with Neovim.

## How Syllable looks to a language runtime

These are the facts each port has to design around.

| Area | What Syllable does | Where | Consequence |
|---|---|---|---|
| CPU target | `i586-pc-syllable`, GCC `--with-arch=i586`, x87 floats | `gcc-common.sh:16` | Rust has i586 targets (x87). Go's default `GO386=sse2` needs SSE2; use `GO386=softfloat` for a true Pentium, or decide that Pyro requires SSE2 (QEMU `-cpu max` has it). |
| Address space | User space is `0x80000000–0xffffffff` (2 GB). The kernel caps RAM at 1.5 GB. Main thread stack 4 MB. | `kernel/tunables.h:44`, `init.c:838`, `scheduler.h:52` | Fine for running programs. Too small to build LLVM, rustc or V8 natively; cross-build those. |
| Syscall ABI | `int $0x80`, Linux-style register args, `-4095..-1` = `-errno` | `init.c:283`, glibc `i386/sysdep.h` | Go can use raw syscalls like linux/386. Numbers are Syllable's own (`syscall.h`). |
| Loader | The **kernel** loads and relocates every image (`elf.c`). It reads **section headers**, not program headers. Only `R_386_{32,PC32,COPY,GLOB_DAT,JMP_SLOT,RELATIVE}`. Only `DT_INIT` (no `DT_INIT_ARRAY`), SysV `DT_HASH` only, no symbol versions, no TLS relocations, no IFUNC. | `elf.c:306-499`, `1233-1415` | Every toolchain must link through `i586-pc-syllable-gcc`/`ld` with `--hash-style=sysv`, `.ctors` (not `.init_array`), and no ELF TLS. LLVM emits `.init_array` by default, so Rust needs `-Z use-ctors-section`-style handling or a loader fix (below). |
| Executables | Linked as **shared objects** (`%{!r:-shared}` in `LINK_SPEC`), `-Bsymbolic --no-undefined`, `crt1.o crti.o crtbeginS.o` | `gcc-9.5.0.patch`, `syllable.h` | A normal static `ET_EXEC` at 0x08048000 (Go's internal linker, `rustc -no-pie`) can't load: it's below user space and not the expected shape. Rust and Go must produce PIC and link through gcc. |
| Per-thread data | `%gs` → a 16 KB TLD block per thread: tid at 0, pid 4, errno-addr 8, errno 12, **self pointer at 16**, libc area 512, user slots from 1024. The kernel rewrites the GDT descriptor on each switch. Segment limit is `TLD_SIZE`. | `tld.h`, `schedule.c:1974`, `smp.c:861` | No i386 ELF TLS (that needs the self pointer at `%gs:0` and negative offsets). GCC uses emutls (`--disable-tls`). Rust must use OS-key TLS. Go needs a fixed positive `%gs` slot for `g`. |
| Exceptions | GCC is built with **SJLJ** exceptions | `gcc-common.sh:17` | Rust's unwinder expects DWARF `_Unwind_RaiseException`. Use `panic=abort` first, then LLVM libunwind (needs `dl_iterate_phdr`, P2-5). |
| Threads | Kernel `spawn_thread` allocates stack + TLD. libpthread on kernel semaphores. No rwlocks, no `sem_*`, no `pthread_condattr_setclock`. `lock_semaphore_x` has a timeout. | `threads.c:815-950`, POSIX audit | Rust std's non-futex sync works on plain pthread mutex/cond. Go uses kernel semaphores directly. libuv needs rwlocks and semaphores. |
| Signals | `sigaltstack` is a stub. `SA_SIGINFO` fills only `si_signo`, ucontext is zeros. | `psignals.c:255-275`, `1266` | Go: hard blocker. Rust: disable the stack-overflow handler. V8: fine without it on ia32. |
| Memory | Anonymous mmap works. File `MAP_PRIVATE` → EINVAL, file offset forced to 0, `MAP_FIXED` bug (`&=`), `madvise` stub. No NX bit (no PAE). | `vfs.c:5155-5262` | rustc/LLVM read files with `MAP_PRIVATE`. Go reserves then maps with `MAP_FIXED`. V8 calls `madvise`. All need P0-6 and a no-op `madvise`. |
| 64-bit | None. 32-bit paging, no PAE, no x86_64 toolchain. | 64-bit support thread | Not needed for any of the three. |

## Shared foundation (do first; all three need most of it)

| # | Work | Rust | Go | Node | Audit ID |
|---|---|---|---|---|---|
| F1 | Fix mmap: file offset, `MAP_PRIVATE` file maps (COW), `MAP_FIXED` `\|=`, `PROT_NONE` reservations, `madvise`/`posix_madvise` returning 0 | needed (rustc, memmap2) | **required** | **required** | P0-6, P2-3 |
| F2 | Entropy: `/dev/urandom` + `getentropy`/`getrandom` | HashMap seeds | `crypto/rand`, map seeds | OpenSSL, V8 | P0-1 (check the "secure random numbers" work is merged and the driver is built) |
| F3 | `sigaltstack` + full `SA_SIGINFO` (`si_code`, `si_addr`, real `ucontext_t`/`mcontext` with registers, writable so the handler can redirect EIP) | optional | **required** | optional | P1-7 |
| F4 | `sched_yield` real; `nanosleep` sets `rem` | yes | yes | yes | P1-3 |
| F5 | `pthread_condattr_setclock(CLOCK_MONOTONIC)` or a documented REALTIME fallback | std Condvar | — | libuv | P1-1 |
| F6 | `dl_iterate_phdr` (and `dladdr`) in libdl, built on `get_image_info` | unwinding, backtraces | cgo only | V8 stack traces | P2-5 |
| F7 | Loader: run `DT_INIT_ARRAY`/`DT_FINI_ARRAY` (small change in `parse_dynamic_section` and the init path) | removes LLVM `.ctors` workaround | external-link only | GCC 15 defaults | new |
| F8 | Real `poll` syscall (no `FD_SETSIZE` cap) | `std` startup fd check | netpoller | libuv | P1-8 |
| F9 | AF_UNIX + `SCM_RIGHTS`; `socket()` rejects unknown families | `std::os::unix::net` | `net` unix sockets | **required** (child stdio, IPC) | P0-2 |
| F10 | `O_CLOEXEC`, `pipe2`, `dup3`, `accept4` | nicer | nicer (has fallbacks) | libuv | P1-6 |
| F11 | `pthread_rwlock_*`, `sem_*` | — | — | **required** (libuv, OpenSSL 3) | P0-7, P0-8 |
| F12 | `*at()` family | `remove_dir_all` (or add Syllable to std's fallback list) | `os.RemoveAll` has fallback | libuv `uv_fs_*` | P1-4 |

F1–F6 unblock Rust and Go. F8–F12 are mostly for Node (and Neovim).

## Rust

### What the port is

- **Target:** `i586-pc-syllable` (or `i686-pc-syllable` if Pyro decides to require SSE2; Haiku chose i686). Rust's `i686-unknown-haiku` and `i586-unknown-redox` are the closest templates.
- **Target spec** (`compiler/rustc_target/src/spec/targets/`): `os = "syllable"`, `families = ["unix"]`, `linker = "i586-pc-syllable-gcc"`, `linker_flavor = gcc`, `dynamic_linking = true`, `relocation_model = pic`, `position_independent_executables = false` (the gcc spec already forces `-shared`; passing `-pie` on top may confuse `ld`), `has_thread_local = false` (std falls back to pthread keys over TLD slots), `crt_static_respected = false`, `panic_strategy = abort` to start, `use_ctors_section = true` until F7. For the LLVM triple, use an OS-neutral ELF i386 triple so LLVM doesn't apply Linux assumptions; it only drives codegen, since gcc links.
- **`libc` crate:** a `src/unix/syllable/` module with types, struct layouts (`stat`, `sigaction`, `dirent`, `sockaddr*`, `termios`), constants and errno values. Generate them from the sysroot headers with the cross gcc and check them with `libc-test`. Note `wchar_t` is `long`, `off_t` is 64-bit, and errno is read through `__errno_location`.
- **std:** add `target_os = "syllable"` to the `unix` pal: thread spawn (`pthread_create`; no `pthread_getattr_np`, so no guard-page detection), the no-op stack-overflow handler, `os::syllable` with `exe_path` via `get_image_info`, `fs` (no `*at`, no hard links), `net` (no IPv6, AF_UNIX gated on F9), time (monotonic via `clock_gettime`), random (F2), `Condvar` without `setclock` until F5.
- **Unwinding:** SJLJ libgcc won't satisfy std's `unwind` crate. Stage 1 is `panic=abort`. Stage 2 links LLVM's libunwind statically (`llvm-libunwind = "in-tree"` in `bootstrap.toml`), which needs F6 to find `.eh_frame_hdr`. Rust panics and C++ exceptions then use different unwinders, which is fine as long as nothing unwinds across the boundary. Switching all of Syllable's GCC to DWARF EH is the other option, but it means rebuilding all C++.

### Build path

1. **Cross only.** In the toolchain Docker image, build Rust from source with `x.py build --target i586-pc-syllable library` and a `[target.i586-pc-syllable]` entry pointing `cc`, `ar` and `linker` at the cross toolchain. rustc itself stays a Linux binary.
2. **Testing.** Rust's `remote-test-server` runs on the target and talks to `x.py test` over TCP, which Syllable has. Run it inside `sb run` QEMU with a forwarded port. That keeps to the no-ssh rule.
3. **Host tools on Syllable.** Later, cross-build `rustc` and `cargo` as `--host i586-pc-syllable`. LLVM is cross-compiled with `i586-pc-syllable-g++`: GCC 9.5 meets LLVM's GCC 7.4 minimum, but LLVM needs C++11 threads (P1-1) or `LLVM_ENABLE_THREADS=OFF`, plus F1. rustc should fit in 2 GB for ordinary crates. Building rustc itself on the box won't.
4. **cargo** needs libgit2 and curl (bundled), plus OpenSSL. `openssl-1.1.1t` is in `Syllable-Ports/archives`. It's end-of-life but `openssl-sys` still supports it; vendored OpenSSL 3 needs F11. cargo falls back to copying when hard links fail.

## Go

### gc, not gccgo

| | gccgo (GCC 9.5) | gc |
|---|---|---|
| Language level | Go 1.12 (GCC 15: about 1.18) | Current |
| Can build modern modules (go ≥ 1.21 in `go.mod`) | No | Yes |
| Runtime port | libgo: needs `makecontext`/`swapcontext` (not in the Syllable glibc add-on), uses `__thread` heavily (emutls would be slow), Linux/Solaris/AIX-shaped `mksysinfo` | `runtime` + `syscall` in Go and assembly |
| Bootstrap | Needs the GCC Go front end ported and built | Cross-compile from Linux with one command |

Porting libgo is as much work as porting gc and ends with an obsolete Go, so gc is the only sensible path.

### What the port is (`GOOS=syllable`, `GOARCH=386`)

- **Syscalls:** raw `int $0x80` with Syllable's numbers, like linux/386. That keeps Go independent of the glibc 2.7 → 2.41 move. Calling libc like openbsd/386 is the alternative, but it costs more assembly.
- **`g` register:** Go on ELF 386 keeps `g` at a negative offset from `%gs`, which faults against Syllable's segment limit. Reserve a fixed positive slot in the TLD (for example inside the libc area at 512) in `tld.h` and teach `cmd/internal/obj/x86` to use it for `GOOS=syllable`. windows/386 already uses a fixed OS slot, so the plumbing exists.
- **Threads:** `newosproc` → `spawn_thread` + `resume_thread`, using the kernel-allocated stack as `g0`'s stack.
- **Locks and sleeping:** the semaphore model (`lock_sema.go`): `semacreate`/`semasleep(ns)`/`semawakeup` → `create_semaphore`/`lock_semaphore_x`/`unlock_semaphore`. No futex needed.
- **Signals:** handlers on `gsignal` via `sigaltstack` (F3), `sigctxt` read from `mcontext`, nil-pointer panics from `si_addr`, and async preemption by rewriting EIP. Without F3, every nil dereference is a crash and handlers overflow goroutine stacks. Until F3's ucontext is writable, async preemption can be turned off for this GOOS.
- **Memory:** `sysReserve`/`sysMap` = `mmap(PROT_NONE)` + `mmap(MAP_FIXED)` (F1). The 386 arena is small, so 2 GB is fine.
- **Netpoller:** start with a `poll()`-based poller like `netpoll_aix.go`, on F8 (or `select` with its cap). kqueue- or epoll-style comes later.
- **Linker:** only external linking via `i586-pc-syllable-gcc` with `-buildmode=pie` semantics, so output matches the loader. That brings cgo for free. Teaching Go's internal linker Syllable's shape can come later.
- **Generated files:** `ztypes`, `zerrors` and `zsysnum` for `syscall` and `x/sys/unix`, generated from the sysroot headers with `cgo -godefs` and the cross gcc.
- **Pure-Go pieces need no porting:** `crypto/tls` (no OpenSSL), the DNS resolver (reads `/etc/resolv.conf`; check where Syllable keeps it), `os/exec` (fork+exec; falls back from `pipe2`).

### Build path

1. `GOOS=syllable GOARCH=386 ./bootstrap.bash` on the Linux host produces a native toolchain tarball.
2. Test with a `go_syllable_386_exec` wrapper that runs test binaries inside `sb run` QEMU over a TCP port (not ssh).
3. Unpack the tarball on Syllable, then `go build` natively. Go then rebuilds itself on Syllable.

## Node.js

### Compiler first

| Node | Min GCC (from `BUILDING.md`) | Status |
|---|---|---|
| 18 | 8.3 | End-of-life. Builds with GCC 9.5 |
| 20, 22 | 10.1 | 22 is in maintenance LTS |
| 24 | 12.2 | Current LTS: the sensible target |
| main | 13.2 or Clang 19.1 | |

So Node depends on the GCC 15 port, with `__GTHREADS_CXX0X` (P1-1) and **libatomic enabled** (it's `--disable-libatomic` today; V8 and Node use 64-bit atomics on ia32). Native TLS (P1-2) is nice to have; emutls works for V8's `thread_local`.

### What the port is

- **libuv:** add a Syllable platform file modelled on upstream's `src/unix/haiku.c`, using libuv's `posix-poll.c` backend (as Haiku, QNX and Hurd do). Needs F8, F9 (`uv_spawn` stdio and `uv_pipe`), F10, F11, and `posix_openpt` only for node-pty. This is the same libuv Neovim uses, so the work counts twice.
- **V8 (ia32):** a `platform-syllable.cc` (shared-library list from `get_image_info`, thread stack start from `get_thread_info`'s `ti_stack` instead of `pthread_getattr_np`), plus the mmap/`mprotect`/`madvise` behaviour from F1. Start with `--jitless` to prove the port without executable memory, then turn the JIT on. Without PAE there is no NX, so W^X is cosmetic.
- **Node itself:** add `syllable` to gyp's `--dest-os` and the `common.gypi` conditions. Bundled OpenSSL 3 (needs F2 and F11) using the `linux-elf` config. `--with-intl=small-icu`. Bundled zlib, c-ares, nghttp2, llhttp, brotli. simdutf/simdjson must be built for i586/SSE2 as chosen.
- **Cross-build only.** Build-time tools (`torque`, `mksnapshot`, `node_mksnapshot`) run on the host, built as Linux `-m32` ia32 so the snapshot matches the target. The toolchain image needs `gcc-multilib`. `--without-node-snapshot` avoids running Node on the host at all. A native build on Syllable is out of reach: V8 translation units can each need 1–2 GB, and there's no Python 3 port for gyp.

## Staged plan

Effort figures are rough guesses for one person who knows the kernel, not measurements.

| Stage | Goal | Work | Done when | Rough effort |
|---|---|---|---|---|
| **0. Foundation A** | Runtimes can start | F1 mmap, F2 entropy (verify), F4 `sched_yield`/`nanosleep`, F5 condattr clock, F7 `DT_INIT_ARRAY` | A C test suite in QEMU covers each fix | 3–5 weeks |
| **1. Rust std, cross** | Rust programs run on Syllable | Target spec, `libc` module, std cfgs, `panic=abort`, `remote-test-server` in QEMU | `cargo build --target i586-pc-syllable` hello world, std tests mostly passing | 3–5 weeks |
| **2. Foundation B** | Real signals | F3 `sigaltstack` + full siginfo/ucontext, F6 `dl_iterate_phdr`/`dladdr` | Signal test: handler on alt stack reads `si_addr` and resumes at a new EIP | 2–4 weeks |
| **3. Rust unwinding + tools** | Panics unwind; rustc and cargo on the box | LLVM libunwind, cross-build LLVM + rustc + cargo for host Syllable, OpenSSL 1.1.1 | `cargo new && cargo run` on Syllable | 4–6 weeks |
| **4. Go** | Go toolchain on Syllable | `GOOS=syllable` runtime, syscall, `g` slot, poll netpoller, external link, exec wrapper | `go test std` mostly passing in QEMU; native `go build` works | 6–10 weeks |
| **5. Foundation C** | libuv-class POSIX | F8 `poll`, F9 AF_UNIX + `SCM_RIGHTS`, F10 CLOEXEC family, F11 rwlocks + semaphores, F12 `*at()` | libuv's own test suite passes in QEMU (helps Neovim too) | 6–10 weeks |
| **6. GCC 15** | C++20 toolchain | Existing roadmap step 2, plus C++11 gthreads and libatomic | Node 24's configure and a V8 ia32 build pass | (roadmap item) |
| **7. Node** | Node 24 on Syllable | libuv platform file, V8 `platform-syllable.cc`, gyp `dest-os`, `--jitless` first | `node -e` works; then the Node test suite in QEMU | 8–12 weeks |

Stages 1 and 2 can overlap, and stages 5 and 6 can run alongside stage 4.

## Open questions for Rick

1. **i586 or SSE2?** Requiring SSE2 (Pentium 4 and later; any QEMU `-cpu max`) gives Rust's i686 codegen and Go's default `GO386=sse2`, with correct IEEE floats. Staying on i586 means x87 floats and `GO386=softfloat`.
2. **glibc 2.7 now or after 2.41?** Rust's `libc` bindings follow whichever libc ships. Starting on 2.7 is fine but means redoing part of the module after the 2.41 move. Go with raw syscalls doesn't care.
3. **Upstream or fork?** Rust tier 3 targets and new Go ports are accepted upstream with a named maintainer. Keeping them as patches in `Syllable-Ports` is quicker at first.

## Checked versus inferred

- **Checked in the tree:** loader capabilities, executable link shape, TLD layout and `%gs` handling, syscall ABI, user address range, SJLJ/emutls/`.ctors` toolchain settings, `sigaltstack` stub.
- **Checked upstream:** Node's GCC minimums and its "Experimental" x86 Linux tier; Rust's i586/i686 Haiku, Redox and Hurd targets.
- **From knowledge, not re-verified today:** details of Go's runtime files (`lock_sema.go`, `netpoll_aix.go`, windows/386 TLS slot), libuv's `posix-poll.c` users, LLVM's GCC 7.4 host minimum, and the Rust std fallbacks named above. Check these when each stage starts.
