# Syllable glibc upgrade plan

Oct 9, 2026 · Rick

## Summary

Port **glibc 2.41** to Syllable. It is the newest release our GCC 9.5.0 and binutils 2.35 can build: 2.41 needs GCC 6.2 and binutils 2.25, while 2.42 jumps to GCC 12.1 and binutils 2.39.

Missing syscalls are real work but not the hardest part. Three structural gaps decide the plan:

1. **No ld.so.** The Syllable kernel links every image itself. It does no symbol versioning, no TLS relocations and no IFUNC, and modern glibc assumes all three.
2. **No TLS.** Syllable's `%gs` points at a per-thread TLD block whose offset 0 holds the thread id. The i386 TLS ABI needs offset 0 to be a self pointer.
3. **Threads live outside libc.** Syllable's `libpthread` is its own library built on kernel semaphores. Since 2.34, glibc has pthreads inside libc, and Linux NPTL needs futexes and `clone`.

The template is glibc's GNU/Hurd port. It is the only upstream port that is neither Linux nor futex-based, and its thread library (`htl`) sits on a small set of OS primitives. That maps onto Syllable's semaphores and `spawn_thread`.

The new library ships under a new soname, `libc.so.3`, beside today's `libc.so.2`. Rebuilding the base system against it happens in phases (see Compatibility).

## Where we are today

Today's port is glibc 2.7 plus a 181-file sysdeps tree, `sysdeps/unix/sysv/syllable`, in `Builder/packages/Syllable/glibc-2.7`. Everything below was read from that tree and from `system/sys` in the Syllable source.

| Area | What Syllable does now | Where |
| --- | --- | --- |
| Syscall entry | `int $0x80` with Syllable's own numbering. About 215 calls, numbered up to 227. | `i386/sysdep.h`, `sys/include/syllable/syllable/syscalltable.h` |
| Syscall wrappers | 150 entries in `syscalls.list`, plus C wrappers for the rest | port `syscalls.list` |
| Dynamic linking | The kernel loads and relocates every image (`elf.c`). It handles `R_386_32`, `PC32`, `COPY`, `GLOB_DAT`, `JMP_SLOT` and `RELATIVE`, and binds everything at load time. | `sys/kernel/kernel/elf.c` |
| Not supported by the loader | Symbol versioning, TLS relocations (`R_386_TLS_*`), `IRELATIVE` / IFUNC, `DT_GNU_HASH` | `elf.c` |
| dlopen | A separate `libdl` on the `load_library` and `get_symbol_address` syscalls. The port builds no `ld.so` or `rtld`; its TODO calls that part "hacked up". | `system/libs/libdl`, port `TODO` |
| Per-thread data | The kernel puts a 16 KB TLD block at the top of each thread's stack and points `%gs` at it. Offsets: thread id 0, process id 4, errno address 8, errno 12, self pointer 16, libc area 512, user area 1024. | `sys/include/syllable/syllable/tld.h`, `threads.c` |
| Threads | A separate `libpthread`, with mutex, cond and rwlock built on kernel semaphores and `spawn_thread` | `system/libs/libpthread` |
| Installed sonames | `libc.so.2`, `libm.so.2`, `librt.so.1`, `libcrypt.so.1.1`, plus the NSS libraries | sysroot `/system/resources/glibc/2.7/lib` |

The port's own TODO lists unfinished work that the new port has to solve properly: `__register_atfork` is an empty macro, `libc-lock.h` has no read/write lock, and TLD destructors run in the wrong order.

## Choosing the target version

The target is 2.41, because it is the newest glibc our cross toolchain can build. The table gives the minimum tools each release's `INSTALL` file states.

| glibc | Min GCC | Min binutils | Builds with GCC 9.5 + binutils 2.35 |
| --- | --- | --- | --- |
| [2.42](https://raw.githubusercontent.com/bminor/glibc/glibc-2.42/INSTALL) | 12.1 | 2.39 | No, it needs a toolchain upgrade first |
| [2.41](https://raw.githubusercontent.com/bminor/glibc/glibc-2.41/INSTALL) | 6.2 | 2.25 | Yes |
| [2.39](https://raw.githubusercontent.com/bminor/glibc/glibc-2.39/INSTALL) | 6.2 | 2.25 | Yes |
| [2.35](https://raw.githubusercontent.com/bminor/glibc/glibc-2.35/INSTALL) | 6.2 | 2.25 | Yes |

There's little reason to stop at an intermediate release. The sysdeps tree gets rewritten either way, and every release after 2.34 has pthreads, `libdl` and `librt` merged into libc, so the structural work is the same for all of them. Moving to 2.42 or later is a separate later step that comes with GCC 12 or newer.

The build host also needs Python 3.4 or newer, which the Docker base image (Ubuntu 20.04, Python 3.8) already has.

## Kernel work

The kernel needs one new mechanism (TLS), one new primitive (wait/wake on an address) and a batch of POSIX-2008 syscalls. I checked the table below against Syllable's syscall table. Which calls glibc 2.41 actually requires is my reading of its generic code; the Phase 1 audit confirms it.

| Need | Today | Proposed kernel change | Fallback without it |
| --- | --- | --- | --- |
| TLS (`__thread`, `%gs:0` self pointer) | `%gs` points at the TLD block, and offset 0 is the thread id | A `set_thread_area`-style syscall that points `%gs` at a TCB allocated by libc, with old TLD fields kept at their offsets past the TCB header | None. glibc 2.41 can't be built without TLS. |
| Wait/wake on an address | Kernel semaphores only | `addr_wait(addr, expected, timeout)` and `addr_wake(addr, count)`, like Hurd's gsync or Linux's futex | Build htl's sync objects on semaphores. That's slower and harder to make robust. |
| `*at` calls: `openat`, `fstatat`, `mkdirat`, `unlinkat`, `renameat`, `readlinkat`, `fchmodat`, `fchownat`, `symlinkat`, `faccessat`, `utimensat` | Missing. The `based_open` family partly covers them. | Add them in the VFS, reusing the `based_*` code | Emulate in libc through `/proc`-style paths, or fail with `ENOSYS` |
| Atomic close-on-exec: `pipe2`, `dup3`, `accept4`, `O_CLOEXEC` | Missing | Add them | Emulate with a non-atomic `fcntl`, which races with fork |
| `link` (hard links) | Missing, and the filesystems have none | None for now | `link()` returns `ENOSYS`/`EPERM` |
| `getrandom` | Missing | Add it, backed by `random.c` | Read `/dev/urandom` |
| `poll`, `clock_gettime` / `clock_nanosleep` with `CLOCK_MONOTONIC`, `sched_yield` | Emulated in the 2.7 port through `select`, `get_raw_system_time` and `snooze` | Native syscalls are nice to have | Keep the 2.7 emulations |
| `mremap`, `madvise` | Missing | Optional | malloc runs without them |
| Loader: `PT_TLS` + `R_386_TLS_*` relocations, symbol versions | Missing in `elf.c` | Needed only if the kernel keeps doing the linking (see Porting the sysdeps) | Ship a real `ld.so` |

Every new syscall goes into `syscalltable.h`, so `strace` and the syscall groups keep working.

## Porting the sysdeps

The new port is a fresh `sysdeps/syllable` tree that borrows its structure from Hurd's and carries over the Syllable-specific code from 2.7. It is not a forward-patch of the 2.7 diff.

**The one big decision is who links images.** Everything else follows from it.

|  | A. Kernel keeps linking (recommended) | B. Ship glibc's `ld.so` |
| --- | --- | --- |
| Kernel loader (`elf.c`) | Add `PT_TLS` setup, `R_386_TLS_*` relocations and symbol-version selection (bind to the default version) | Add `PT_INTERP` hand-off to `ld.so` |
| Existing apps and libraries | Unchanged: one loader for old and new | Two loaders side by side. New-libc images are linked in user space. |
| glibc changes | Stub out `rtld`. Write `dl_iterate_phdr`, `dlopen` and TLS module tracking over `get_image_info` and `load_library`. Build with `--disable-multi-arch` so there's no IFUNC. | Mostly upstream code, plus Syllable's image and area model inside `ld.so` |
| Risk | Symbol versions. A 2.41 ABI baseline removes compat symbols, so the loader can ignore versions (see Risks). | Larger change to how Syllable shares library pages between processes |

The steps, under option A:

1. Create `sysdeps/syllable` with `Implies` on `unix/bsd`, `posix`, `htl` and `i386`, and a `syscalls.list` generated from `syscalltable.h`, so numbers never drift by hand.
2. Port the 2.7 Syllable extensions unchanged in behaviour: areas, ports and messages, semaphores, `attrdir`/`indexdir`, `probe_fs`, `get_system_info`, `spawn_thread` and the image calls. These are about 40 C files plus their `syscalls.list` entries. The rest are POSIX wrappers that the generic `unix`/`posix` code now covers.
3. Threads: wire `htl` to `spawn_thread` and `exit_thread`, and its sync objects to `addr_wait`/`addr_wake` (or semaphores, until the kernel has them). That retires the separate `libpthread` and brings real `pthread_atfork`, rwlocks and TLS destructors.
4. TLS: `TLS_INIT_TP` calls the new kernel syscall. errno becomes a normal `__thread` variable, and the TLD errno slot stays mirrored for old code.
5. `dlfcn`: `dlopen`, `dlsym` and `dlclose` on top of `load_library`, `get_symbol_address` and `unload_library`, replacing the separate `libdl`.
6. Headers: regenerate the `bits/*.h` files (`stat`, `fcntl`, `signum`, `termios`, `socket` and others) from the kernel's own headers, and check every struct layout against the kernel.

## Toolchain and build-system changes

glibc 2.41 builds directly with the GCC 9.5.0 cross compiler, so it needs no bootstrap compiler. It gets its own sysroot, and the 2.7 build stays as it is.

- **Builder:** a new `Builder/packages/Syllable/glibc-2.41` with a recipe, `glibc-2.41.patch` and a `patches/` overlay, following the same conventions as the gcc and binutils ports. It's the source of truth, just as for those ports.
- **Docker:** a new script and stage after 08 builds 2.41 into `/opt/syllable/sysroot-2.41`. The current sysroot and the packages that depend on it keep building.
- **GCC rebuilt against the new sysroot:**
  - `--enable-tls` instead of emutls, once the kernel has TLS.
  - Thread model `posix` instead of `syllable`, which gives libstdc++ working `std::thread`, `std::mutex` and `std::condition_variable`.
  - `libgcc_s.so.3` keeps its soname.
  - Moving from SJLJ to DWARF unwinding becomes possible once `dl_iterate_phdr` exists. That's optional and comes later.
- **binutils 2.35:** no change expected, since i386 TLS relocations are already generic. If the kernel adds TLS as option A describes, `ENABLE_INITFINI_ARRAY` can stay off.
- **Testing on the box:** glibc's own test suite cross-compiles here and runs on Syllable. A FAT disk image from `make-fat-image.sh` carries the test binaries and a runner script into qemu, and results come back on the same image.

## Compatibility

The rule is one libc per process. A process links either `libc.so.2` everywhere or `libc.so.3` everywhere, never both. Two libcs in one process means two mallocs, two errnos and two stdio buffers, which corrupts memory as soon as a pointer crosses between them.

- **Old binaries:** they keep `libc.so.2` and the old TLD layout and run unchanged. The kernel keeps every TLD offset that old code reads, and the new TLS area sits past them.
- **Libraries that link libc:** `libsyllable`, `libmedia`, `libstdc++` and the rest each need a build against `libc.so.3` under a new soname or in a new directory, for example `libsyllable.so.8`. The old builds stay for old apps.
- **IPC:** the app server, the registrar and other services talk over kernel ports and messages, not libc structures. Old and new apps can share the desktop.
- **Guard:** the kernel loader refuses to load an image whose libc differs from the one the process already has, so a mix fails loudly instead of corrupting memory.
- **The GCC 9.5.0 package from this week** stays the compiler for `libc.so.2` apps. The new-libc toolchain installs alongside it.

## Phased plan

Each phase ends when its gate passes on a real Syllable install in qemu. Missing functions found along the way go onto the Phase 1 audit list and get fixed in Phase 4, or sooner if they block a gate.

| Phase | Work | Gate |
| --- | --- | --- |
| 1. Scaffold and audit | Unpack 2.41, create `sysdeps/syllable`, generate `syscalls.list`. Stub every missing call with `ENOSYS`; the audit becomes the kernel backlog. | A static hello world runs on Syllable |
| 2. Kernel TLS and loader | TLS syscall, `%gs` layout per libc, `PT_TLS` and TLS relocations in `elf.c`. Shared `libc.so.3` links; errno becomes a `__thread` variable. | A `__thread` test passes, static and shared |
| 3. Threads | `htl` on `spawn_thread`; `addr_wait` and `addr_wake` in the kernel. Retire `libpthread` and `libdl`; real atfork, rwlocks, TLS destructors. | pthread tests pass and `std::thread` works |
| 4. POSIX syscalls | `*at` family, `pipe2`, `dup3`, `accept4`, `getrandom`, native `poll`. Run glibc's test suite in qemu and triage every failure. | Every failure fixed or marked unsupported |
| 5. Toolchain and userland | GCC with real TLS and posix threads; libstdc++ rebuilt. `libsyllable.so.8` and Builder recipes against `libc.so.3`. | A GUI app on `libc.so.3` runs on the desktop |

## Risks and open questions

| Risk | Mitigation |
| --- | --- |
| The kernel can't pick between symbol versions | Give the port a 2.41 ABI baseline in `shlib-versions`, the way new upstream ports do. Then `libc.so.3` has no compat symbols at all, and the loader can ignore versions. |
| The TLS layout breaks old code that reads `%gs` offsets directly | Keep every TLD offset that old code reads. Put the TCB self pointer where the i386 TLS ABI wants it only for `libc.so.3` processes, chosen at `execve` from the image's libc. |
| htl on semaphores is slow or not robust | Add `addr_wait`/`addr_wake` early (Phase 2) |
| Upstream tests assume Linux (`/proc`, `clone`, hard links) | Mark them `UNSUPPORTED` in the port's test list instead of patching tests |
| A long tail of missing calls | Start each one as a libc stub returning `ENOSYS`, logged with `dbprintf`. The audit list then becomes the kernel backlog. |

Open questions:

1. Option A (the kernel keeps linking) or option B (`ld.so`)?
2. Is the soname `libc.so.3`, with a parallel `libsyllable.so.8`, the right way to version this?
3. Do kernel changes land in the Syllable tree in this repo, or on a separate kernel branch?
4. Stay on 2.41, or upgrade to GCC 12 first so the target can be 2.42 or later?

The live, commentable copy of this plan is the Claude doc "Syllable glibc upgrade plan": https://claude.ai/code/artifact/14ee4d89-57f0-4371-98e9-9c425822d254
