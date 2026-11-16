# Syllable (Pyro) POSIX compatibility audit — 2026-10-30

Read-only audit of the tree in `/Users/rcaudill/Code/Rick-PyroOS`. Nothing was built, run, changed or committed.

## Summary

Syllable is a POSIX-*shaped* system: a BeOS-style kernel (areas, ports, kernel semaphores, thread-local-data slots) with **about 230 syscalls**, plus **glibc 2.7** on top through a `syllable` add-on, plus the tree's own **libpthread** and **libdl**. Classic Unix file, process and terminal calls work. Anything newer than about POSIX.1-2001 is mostly stubbed or missing. The headers advertise more than the system does.

The blocking gaps, in order of how much they hold up the stated goals:

1. **No usable entropy.** `/dev/random` is a Mersenne Twister seeded from boot time, and on the current tree its driver isn't built at all (`drivers/CMakeLists.txt` has no `misc`). There's no `/dev/urandom`, `getrandom` or `getentropy`. OpenSSL/OpenSSH can't seed; `std::random_device` has nothing to read.
2. **No AF_UNIX, no IPv6, no SCM_RIGHTS.** The kernel speaks only AF_INET TCP/UDP/raw. `socket()` doesn't even check the family: `socket(AF_UNIX, SOCK_STREAM, 0)` quietly makes a TCP socket, and `bind`/`connect` then fail with EAFNOSUPPORT. `socketpair()` is a libc emulation over loopback TCP. `sendmsg`/`recvmsg` ignore `msg_control`. This breaks sshd's privsep/reexec, and many daemons, Python, and anything that passes fds.
3. **Threads are thin.** In libpthread, **every `pthread_rwlock_*` returns ENOSYS**. There are no POSIX semaphores (`sem_*` at all), barriers, spinlocks, `pthread_atfork`, `pthread_mutex_timedlock`, `pthread_condattr_setclock` or robust mutexes. Cancellation only acts at `pthread_testcancel()`. `pthread_cond_timedwait` spawns a kernel thread per call, and there is no native TLS (`--without-__thread`, gcc `--disable-tls`: `__thread` falls back to libgcc emutls). `sched_yield` is an ENOSYS stub. GCC's thread model (`gthr-syllable.h`) doesn't define `__GTHREADS_CXX0X`, so `<thread>`, `<mutex>` and `<condition_variable>` are empty (already covered in `Syllable/docs/cxx11-threads.md`).
4. **The `*at()` family is missing.** `openat`, `fstatat`, `mkdirat`, `unlinkat`, `renameat`, `symlinkat`, `readlinkat`, `faccessat`, `fchownat`, `utimensat` and `futimens` are all ENOSYS stubs. So are **`link()`**, `lchown`, and `posix_openpt`. `dup3`, `pipe2`, `accept4`, `O_CLOEXEC`, `SOCK_CLOEXEC` and `SOCK_NONBLOCK` don't exist (glibc 2.7 predates them, and the kernel has no flags for them).
5. **Security model isn't enforced.** The VFS does no mode-bit checks on open, exec or search. `access()` checks existence only and ignores R_OK/W_OK/X_OK. Only chmod/chown check ownership. UIDs, GIDs and setuid calls exist and are tracked.
6. **mmap is mostly broken for files.** `MAP_PRIVATE` file mappings return EINVAL. The file offset is hard-wired to 0. The `MAP_FIXED` path ANDs the protection with `AREA_EXACT_ADDRESS` (it should OR), and the O_RDONLY check can never fire. Anonymous mappings work.
7. **Signals are basic.** Each thread keeps a 64-bit pending mask, so real-time signals aren't queued. `sigaltstack` is a kernel ENOSYS stub. `SA_SIGINFO` fills only `si_signo` (si_code, si_pid, si_addr and the ucontext are all 0). `sigqueue`, `sigtimedwait` and `sigwaitinfo` are stubs.
8. **No IPC beyond pipes, FIFOs and TCP.** SysV msg, sem and shm, POSIX `mq_*` and `shm_open` are all stubs. There's no `sem_open`.
9. **Timers.** `timer_create` and the rest of `timer_*` are stubs. `setitimer` (REAL/VIRTUAL/PROF) and `alarm` are real. `CLOCK_MONOTONIC` works (microsecond system time); CPU-time clocks don't.

## Method

- **Kernel syscall table:** `Syllable/system/sys/include/syllable/syllable/syscall.h` (the `__NR_*` list; identical to `atheos/syscall.h`), with descriptions in `.../syllable/syscalltable.h`. Implementations are under `Syllable/system/sys/kernel/kernel/` (`psignals.c`, `schedule.c`, `fork.c`, `exec.c`, `areas.c`, `pgroup.c`, `vfs/vfs.c`, `vfs/flock.c`, `vfs/pty.c`, `vfs/fifo.c`, `net/socket.c`).
- **The libc that ships:** glibc **2.7** with the `syllable` add-on, from `Syllable/system/apps/utils/Builder/packages/Syllable/glibc-2.7/` (patch + `patches/syllable/sysdeps/unix/sysv/syllable/`, shortened below to **`GP/`**). It's built by `Syllable-Build-System/toolchain/scripts/06-glibc.sh` (`--without-__thread`, `--enable-add-ons=syllable`) and installed as `/system/resources/glibc/2.7` (`Syllable/cmake/SyllablePorts.cmake:262`). The trees `Syllable/system/libs/glibc` (2.1.2) and `glibc-2.3.2` are **not** what ships and were used only for comparison.
- **Built artefacts:** `Syllable-Build-System/out/sysroot/system/resources/glibc/2.7/` (shortened to **`G/`**). I read `G/include/gnu/stubs-32.h`, which lists 140 `__stub_*` functions (glibc's own list of functions that are ENOSYS), and used `nm -D` on `G/lib/libc-2.7.so`, `librt-2.7.so`, `libutil-2.7.so` and `out/sysroot/system/libraries/libpthread.so.2` / `libdl.so.3`.
- **How glibc builds missing syscalls:** `GP/syscalls.list` names calls such as `sched_yield`, `nanosleep`, `flock` and `fdatasync`. Where the kernel has no `__NR_x`, `make-syscalls.sh` emits nothing, and glibc falls back to either a generic emulation (`sysdeps/posix/*.c`, `misc/*.c`) or a generic ENOSYS stub, which then appears in `stubs-32.h`. I classified each case by checking both.
- **Threads:** `Syllable/system/libs/libpthread/*.c` with the header `Syllable/system/sys/include/syllable/pthread.h`.
- **Earlier work cross-checked:** `_parked/sshd-wip/README.md` (sshd findings) and `Syllable/docs/cxx11-threads.md`.

**Status key**

| Status | Meaning |
|---|---|
| **K** | Real kernel syscall |
| **L** | Implemented in libc on top of other calls (emulated) |
| **P** | Present but partial or broken (see notes) |
| **S** | Symbol exists but returns ENOSYS (glibc stub list, or explicit ENOSYS in the source) |
| **M** | Missing: no symbol or no header |

## Per-area tables

### Process

| Interface | Status | Where | Notes |
|---|---|---|---|
| fork | K | `__NR_Fork`; `kernel/fork.c`; `GP/i386/fork.c` | Copies areas; only the calling thread survives. No real `pthread_atfork` (see Threads). |
| vfork | L | `GP/vfork.c` → `posix/vfork.c` | Plain fork. |
| execve | K | `__NR_execve`; `kernel/exec.c` | Close-on-exec honoured (`exec.c:332`). |
| fexecve | S | `G/include/gnu/stubs-32.h` | |
| posix_spawn* | L | `libc-2.7.so` | Generic glibc fork+exec implementation. |
| wait4 / waitpid / wait3 | K / P | `kernel/schedule.c:1274` | `rusage` from children is never filled (`schedule.c:1371`, TODO). WUNTRACED works; WCONTINUED isn't handled. |
| waitid | L | glibc `sysdeps/posix/waitid.c` | Emulated via waitpid; siginfo is synthesised. |
| setsid / setpgid / getpgid / getsid / getpgrp | K | `__NR_setsid` and related; `kernel/pgroup.c` | |
| getrlimit | P | `GP/getrlimit.c` | Constant values: NOFILE=256, CORE=0, most others INFINITY; STACK comes from thread info. |
| setrlimit | S | `GP/setrlimit.c` | Always ENOSYS. **Breaks `ulimit`, sshd and bash's `ulimit -c`.** |
| getrusage | P/Broken | `GP/getrusage.c` | RUSAGE_SELF only, and only for the *calling thread*. The arithmetic is wrong (`/100000`, and usec = `t % tv_sec`). RUSAGE_CHILDREN → ENOSYS. |
| times | S | stubs-32.h | Breaks `bash`'s `times` builtin, `make`, and many benchmarks. |
| getpriority / setpriority / nice | L | `GP/getpriority.c`, `setpriority.c`, `nice.c` | Mapped to thread priority. |
| getloadavg | S | stubs-32.h | |

### Signals

| Interface | Status | Where | Notes |
|---|---|---|---|
| sigaction | K/P | `__NR_sigaction`; `kernel/psignals.c` | `SA_RESTART`, `SA_NOCLDSTOP`, `SA_NOCLDWAIT`, `SA_ONESHOT`/`SA_RESETHAND` and `SA_NOMASK`/`SA_NODEFER` are honoured. |
| SA_SIGINFO / siginfo_t | P | `psignals.c:255-275` | Only `si_signo` is set. `si_code`, `si_pid`, `si_uid`, `si_addr` and `si_value` are 0, and the ucontext is all zeros (no `uc_mcontext`). Crash handlers, JITs and GC write barriers that read `si_addr` or ucontext won't work. |
| sigprocmask / sigpending / sigsuspend | K | `__NR_sigprocmask` and related | Per-thread masks. |
| kill / killpg | K | `__NR_kill`, `__NR_killpg` | |
| sigaltstack | **S** (kernel) | `psignals.c:1266` prints "stub called" and returns ENOSYS | Stack-overflow handlers (Rust, Go, Python faulthandler, libsigsegv, GHC) fail. |
| sigqueue | S | stubs-32.h | |
| Real-time signals SIGRTMIN..SIGRTMAX | P | `posix/signal.h:68` (32..64) | Numbers exist, but pending is a bitmask (`tr_sSigPend`), so RT signals aren't queued and carry no value. |
| sigwait | L | glibc `sysdeps/posix/sigwait.c` | Emulated with sigsuspend plus a temporary handler; racy, not thread-directed. |
| sigtimedwait / sigwaitinfo | S | stubs-32.h | |
| pthread_sigmask | L | `libpthread/pthread.c:423` | Just calls `sigprocmask`. That's equivalent here because masks are per thread. |
| pthread_kill | L | libpthread | Kernel `kill` aimed at a thread id. |
| alarm / pause | K / L | `__NR_alarm`; glibc `sysdeps/posix/pause.c` | |

### Files

| Interface | Status | Where | Notes |
|---|---|---|---|
| open / close / read / write / lseek | K | `__NR_open` and related | |
| O_DIRECTORY / O_NOFOLLOW | K | `posix/fcntl.h:45-47` (`O_NOFOLLOW` = `O_NOTRAVERSE`) | `O_NOFOLLOW` opens the link itself rather than failing with ELOOP: not POSIX semantics. |
| O_CLOEXEC | **M** | not in `posix/fcntl.h` | Only `FD_CLOEXEC` via `F_SETFD` (racy across fork in threaded programs). |
| openat, fstatat, mkdirat, unlinkat, renameat, linkat, symlinkat, readlinkat, faccessat, fchownat, fchmodat, mknodat (`__xmknodat`), futimesat | **S** | stubs-32.h | Every `*at()` call. `fdopendir` exists. |
| utimensat / futimens / futimes / lutimes | S | stubs-32.h | |
| utime / utimes | K/L | `__NR_utime`; `vfs.c:3228` | Second resolution only. |
| pread / pwrite (+64) | L→K | `GP/pread.c`, `pwrite.c` → `__NR_read_pos` / `__NR_write_pos` | |
| readv / writev | K | `__NR_readv`, `__NR_writev` (+`_pos` variants) | |
| ftruncate / truncate | K | `__NR_ftruncate`, `__NR_truncate` | |
| fsync | K | `__NR_fsync`; `vfs.c:890` | |
| fdatasync | L | glibc `misc/fdatasync.c` | Calls fsync. |
| posix_fallocate | L | glibc `sysdeps/posix/posix_fallocate.c` | Emulated by writing zeros. |
| posix_fadvise | S | stubs-32.h | Most programs ignore its failure. |
| mkfifo / mknod | K | `__NR_mknod`; `vfs/fifo.c`; `__xmknod` in libc | |
| symlink / readlink | K | `__NR_symlink`, `__NR_readlink` | |
| **link** | **S** | stubs-32.h | No hard links at all. Breaks `ln`, git (`clone --local`), `make install` and lockfile schemes. |
| rename | K | `vfs.c:2100` | |
| dup / dup2 | K | `vfs.c:4231`, `4256` | |
| dup3 / pipe2 | M | not in glibc 2.7 | |
| pipe | K | `vfs.c:1568` | |
| fcntl F_GETFD/SETFD/GETFL/SETFL/DUPFD | K | `vfs.c:4694` | `F_SETFL` only changes `O_APPEND`/`O_NONBLOCK`. No `F_DUPFD_CLOEXEC`. |
| fcntl F_GETLK/F_SETLK/F_SETLKW | K/P | `vfs/flock.c` | Byte-range locks with deadlock detection. **The owner is the thread** (`flock.c:538`, `get_thread_id`), not the process, so two threads of one process conflict with each other, against POSIX. |
| flock | L | glibc `sysdeps/posix/flock.c` | Emulated on fcntl locks: inherits the thread-owner quirk and isn't BSD per-open-file semantics. |
| lockf | L | libc | Via fcntl. |
| access | **P/Broken** | `vfs.c:5142` | Only checks that the path exists; `nMode` is never used. |
| faccessat | S | stubs-32.h | |
| chmod / fchmod / chown / fchown | K | `vfs.c:3273`, `3458` | Owner/root checks present (`vfs.c:3302`, `3344`, `3401`). |
| lchown / lchmod | S | stubs-32.h | |
| umask | K | `__NR_umask` | |
| realpath | L | glibc generic | |
| statfs | S | stubs-32.h | `fstatfs`/`statvfs` are implemented in `GP/`. |

### Directories

| Interface | Status | Where | Notes |
|---|---|---|---|
| opendir / readdir / closedir / rewinddir | L→K | `__NR_getdents`, `__NR_rewinddir`; `GP/getdents.c` | |
| fdopendir | L | libc-2.7.so | Not in the stub list. |
| readdir_r | L | libc | `readdir64_r` **is** a stub (stubs-32.h), so `_FILE_OFFSET_BITS=64` code gets ENOSYS. |
| scandir / alphasort | L | libc | |

### Memory

| Interface | Status | Where | Notes |
|---|---|---|---|
| mmap MAP_ANONYMOUS | K | `vfs.c:5170+` | Always private regardless of MAP_SHARED, so no shared anonymous memory across fork. |
| mmap of a file, MAP_SHARED | **P/Broken** | `vfs.c:5210-5262` | **Offset is forced to 0** ("Hardwired for debugging"). The O_RDONLY+PROT_WRITE check tests `f_nMode & O_RDONLY` with `O_RDONLY == 0`, so it never fires. Only regular files and symlinks can be mapped (not /dev/zero). |
| mmap of a file, MAP_PRIVATE | **Broken** | `vfs.c:5203` | Returns EINVAL ("Eeek"). This is how `ld.so`, Python, git, sqlite and most databases map files. |
| MAP_FIXED | **Broken** | `vfs.c:5166` | `nProtFlags &= AREA_EXACT_ADDRESS` should be `|=`: this throws away the protection bits and doesn't actually request the exact address. |
| munmap / mprotect / msync | K | `__NR_munmap` and related; `areas.c:3048`, `3157` | |
| mlock / munlock / mlockall | S | stubs-32.h | Even though `_POSIX_MEMLOCK` is advertised. |
| madvise / posix_madvise / mincore | S | stubs-32.h | |
| shm_open / shm_unlink | S | stubs-32.h (librt) | Even though `_POSIX_SHARED_MEMORY_OBJECTS` is advertised. |
| brk / sbrk | K | `__NR_sbrk` (`brk` is a stub) | |
| posix_memalign | L | libc | `aligned_alloc` is M (C11, glibc 2.16). |

### Threads (libpthread.so.2, `Syllable/system/libs/libpthread`)

| Interface | Status | Where | Notes |
|---|---|---|---|
| pthread_create / join / detach / exit / self / equal | L→K | `pthread.c`; `spawn_thread` and related | |
| pthread_key_* / getspecific / setspecific | L | TLD slots (`alloc_tld`) | |
| pthread_once | L | | |
| Mutex NORMAL / ERRORCHECK / RECURSIVE | L | `mutex.c:67-80` → kernel semaphore flags | **`PTHREAD_MUTEX_INITIALIZER` is RECURSIVE** (`pthread.h:123`), so a deadlock-on-relock is silently allowed. |
| Mutex PROCESS_SHARED | P | `mutex.c:89` | Mapped to `SEM_GLOBAL` ("close enough"). |
| Robust mutexes (`pthread_mutex_consistent`, `*_setrobust`) | M | | |
| Priority inheritance/protect (`PTHREAD_PRIO_INHERIT`, prioceiling) | S | `mutex.c:44`, `121`, `177`, `182`, `218`, `223` return ENOSYS | Constants declared, not implemented. |
| pthread_mutex_timedlock | M | | |
| Condition variables | L/P | `cond.c` | `pthread_cond_timedwait` **spawns a kernel thread per call** as its timer (`cond.c:202`). The timeout uses a 32-bit microsecond difference, wrong past ~35 min. Always CLOCK_REALTIME. |
| pthread_condattr_setclock / getclock | M | | Needed by libstdc++ `steady_clock` waits. |
| pthread_cond_clockwait, pthread_mutex_clocklock | M | (glibc 2.30 API) | |
| **rwlocks (all `pthread_rwlock_*`, `rwlockattr_*`)** | **S** | `rwlock.c:23-65` all `return ENOSYS` | `std::shared_mutex`, Python, Neovim/libuv `uv_rwlock_*` and glib `GRWLock` fail or assert. |
| Barriers (`pthread_barrier_*`) | M | | libuv `uv_barrier`, OpenMP. |
| Spinlocks (`pthread_spin_*`) | M | | |
| pthread_cancel | P | `pthread.c:108` → `pthread_kill(SIGCANCEL)` | Deferred cancellation only takes effect in `pthread_testcancel()`; blocking calls aren't cancellation points. Asynchronous cancellation calls `pthread_exit` from the signal handler. |
| pthread_atfork | **M** | `glibc-2.3.2/.../TODO` notes the empty `__register_atfork` | malloc isn't fork-safe in threaded programs. |
| pthread_sigmask / pthread_kill | L | see Signals | |
| pthread_setname_np / getname_np | M | | `rename_thread` syscall exists (`__NR_rename_thread`): easy to add. |
| pthread_getcpuclockid | M | | |
| pthread_setconcurrency | S | `pthread.c` returns ENOSYS | Harmless. |
| `__thread` / `thread_local` TLS | **P** | glibc `--without-__thread`; gcc `--disable-tls` (`gcc-9.5.0.recipe:51`) | No native TLS; gcc emits emutls calls (libgcc, on `__gthread_key`), which work but are slow and invisible to gdb. No `__cxa_thread_atexit_impl`. |
| sched_yield, sched_get/setparam, get/setscheduler, priority_min/max, rr_get_interval, get/setaffinity | **S** | stubs-32.h | `sched_yield` being ENOSYS hurts spin-then-yield loops (libstdc++, libuv, Python's GIL drop). |
| sched_getcpu, CPU_* macros | M/P | `GP/sched_cpualloc.c` | |

### Semaphores

| Interface | Status | Where | Notes |
|---|---|---|---|
| sem_init / sem_destroy / sem_wait / sem_trywait / sem_post / sem_getvalue / sem_timedwait | **M** | no `sem_*` in libc, libpthread or librt; no POSIX `<semaphore.h>` (only `syllable/semaphore.h`, the kernel API) | Python `multiprocessing` and `_thread` locks, libstdc++ `<semaphore>`, and libuv `uv_sem_*` all need these. |
| sem_open / sem_close / sem_unlink | M | | |
| Kernel counting semaphores | K | `__NR_create_semaphore`, `lock_semaphore_x` (with timeout) and related | A good base to build POSIX semaphores on. |

### Clocks and timers

| Interface | Status | Where | Notes |
|---|---|---|---|
| clock_gettime CLOCK_REALTIME | L | `GP/clock_gettime.c` (librt) | Via gettimeofday (microseconds). |
| clock_gettime CLOCK_MONOTONIC | L | same | Uses `get_system_time()` (microseconds since boot). Works, but `_POSIX_MONOTONIC_CLOCK` is defined as **0**, meaning "check at runtime". |
| CLOCK_PROCESS_CPUTIME_ID / CLOCK_THREAD_CPUTIME_ID | M | `clock_gettime.c` default → EINVAL | |
| clock_getres | L | `GP/clock_getres.c` | Claims 1 ms. |
| clock_settime | L | librt | |
| clock_nanosleep | L/P | glibc `sysdeps/unix/clock_nanosleep.c` | REALTIME/MONOTONIC via nanosleep. Other clocks → ENOTSUP. |
| nanosleep | L→K | `GP/nanosleep.c` → `__NR_snooze` | **Always sets `rem` to 0**, even when interrupted by a signal, so EINTR-resume loops lose time. |
| setitimer / getitimer (REAL/VIRTUAL/PROF) | K | `__NR_setitimer`; `psignals.c:662-800` | |
| timer_create / settime / gettime / delete / getoverrun | **S** | stubs-32.h (librt) | Even though `_POSIX_TIMERS` is advertised. |
| gettimeofday / settimeofday / time | L→K | `GP/gettimeofday.c` and related | |

### IPC

| Interface | Status | Where | Notes |
|---|---|---|---|
| SysV msgget/msgsnd/msgrcv/msgctl | S | stubs-32.h | |
| SysV semget/semop/semtimedop/semctl | S | stubs-32.h | |
| SysV shmget/shmat/shmdt/shmctl | S | stubs-32.h | Breaks X11 MIT-SHM, PostgreSQL < 9.3 and some IPC-based ports. |
| POSIX mq_* | S | stubs-32.h (librt) | |
| Pipes / FIFOs | K | `vfs.c:1568`, `vfs/fifo.c` | |
| Native message ports | K | `__NR_create_port`, `send_msg` and related | The Syllable-native IPC. |

### Sockets and networking

| Interface | Status | Where | Notes |
|---|---|---|---|
| socket AF_INET STREAM/DGRAM/RAW | K | `net/socket.c:66-130` | TCP, UDP, raw. |
| **socket AF_UNIX / AF_LOCAL** | **Broken** | `net/socket.c:66` never checks `nFamily` | Creates a TCP socket; `bind`/`connect` then fail with EAFNOSUPPORT (`socket.c:759`, `838`). Headers *do* define `AF_UNIX` (`atheos/socket.h:59`), so configure scripts think it exists. |
| **AF_INET6** | **M** | no IPv6 code in `kernel/net` | `sockaddr_in6` and `IPPROTO_IPV6` are in the headers. getaddrinfo asserts on IPv6 (`_parked/sshd-wip/README.md`, item 2). |
| socketpair | **P (fake)** | `GP/socketpair.c` | SOCK_STREAM only, built as listen/connect/accept over INADDR_ANY TCP (the parked patch moves it to loopback). Can't carry fds, is reachable by other local users, and isn't atomic. |
| sendmsg / recvmsg | K/P | `net/socket.c:1107-1140` | `msg_control` is ignored: **no SCM_RIGHTS, no SCM_CREDENTIALS**. |
| accept4 / SOCK_CLOEXEC / SOCK_NONBLOCK | M | glibc 2.7 and kernel have neither | |
| bind/listen/accept/connect/get/setsockopt/getpeername/getsockname/shutdown | K | `net/socket.c` | |
| send / recv / sendto / recvfrom | L→K | `GP/send.c` and related → sendmsg/recvmsg | |
| select | K | `vfs.c:4856` | |
| poll | L | `GP/poll.c` → `sysdeps/unix/bsd/poll.c` (built on select) | There's no `__NR_poll`. Limited to `FD_SETSIZE`; `POLLPRI`/`POLLHUP` are approximate. |
| pselect / ppoll | L/P | glibc `misc/pselect.c`, `io/ppoll.c` | Implemented as sigprocmask + select/poll, **not atomic** (the classic race these calls exist to avoid). |
| epoll / kqueue / eventfd / signalfd / inotify | M | | Only Syllable's `watch_node` for file watching. |
| getaddrinfo / getnameinfo | L | libc with `libnss_dns` | IPv4 only in practice. |
| if_nametoindex / if_indextoname / if_nameindex | S | stubs-32.h | |
| getifaddrs | L | libc | Generic fallback; behaviour not verified. |
| sendfile | S | stubs-32.h | |

### Terminals

| Interface | Status | Where | Notes |
|---|---|---|---|
| tcgetattr / tcsetattr / cfset*speed | L→K | `GP/tcgetattr.c`, `tcsetattr.c` → `TCGETA`/`TCSETA*` (`vfs/pty.c:1689-1720`) | |
| tcgetpgrp / tcsetpgrp / tcdrain / tcflow / tcflush | L | `GP/tc*.c` | TIOCSPGRP/TIOCGPGRP in `pty.c:1761-1810`. |
| TIOCGWINSZ / TIOCSWINSZ / TIOCNOTTY / TIOCPKT / FIONREAD / FIONBIO | K | `vfs/pty.c` | |
| TIOCSCTTY | M | no handler in `vfs/pty.c` | The controlling tty is assigned implicitly on open. |
| getpt (BSD-style `/dev/pty/mst/ptyN`) | L | `GP/getpt.c` | |
| posix_openpt | **S** | stubs-32.h; `GP/getpt.c` ENOSYS branch | Modern software (tmux, Python’s `pty`, libuv-based programs) expects it; gnulib and autoconf probe for it. |
| grantpt / unlockpt / ptsname / ptsname_r | L | `GP/unlockpt.c` (no-op), `GP/ptsname.c` | |
| openpty / forkpty / login_tty | L | `libutil-2.7.so` | Built on getpt. |
| /dev/ptmx | M | | |

### Users and credentials

| Interface | Status | Where | Notes |
|---|---|---|---|
| getuid/geteuid/getgid/getegid/getgroups | K | `__NR_getuid` and related | |
| setuid/setgid/setreuid/setregid/setresuid/setresgid/setfsuid/setfsgid/setgroups | K | `kernel/pgroup.c:425-600` | Saved IDs implemented. |
| seteuid / setegid | L | `GP/seteuid.c`, `setegid.c` | |
| getpwnam_r / getpwuid_r / getgrnam_r / getgrouplist / initgroups | L | libc with `libnss_files` | |
| crypt | L | `libcrypt-2.7.so` | DES, MD5 and SHA-256/512 (`$5$`/`$6$`, added in glibc 2.7). |
| **Permission enforcement** | **M** | no rwx checks in `vfs.c` lookup/open/exec | Any user can read and write any file. Only chmod/chown (`vfs.c:3302`, `3344`, `3401`, `3415`) and attribute calls check ownership. `access()` ignores its mode. |
| setlogin / sethostname / setdomainname / acct / revoke / vhangup | S | stubs-32.h | |

### Locale, iconv, wide characters

| Interface | Status | Where | Notes |
|---|---|---|---|
| setlocale / newlocale / uselocale | L/P | libc-2.7 | **No compiled locales**: `G/lib/locale` doesn't exist (recipe: "Locales are not installed"). Only C/POSIX works until `localedef` runs. `share/i18n` sources and `localedef` are present. |
| iconv / iconv_open | L | `G/lib/gconv` (244 modules) | |
| wchar / wctype / mbrtowc and related | L | libc | |

### Dynamic loading

| Interface | Status | Where | Notes |
|---|---|---|---|
| dlopen / dlsym / dlclose / dlerror | L→K | `Syllable/system/libs/libdl/dlfcn.c` → `load_library` / `get_symbol_address` | `RTLD_LAZY` is treated as NOW; GLOBAL/LOCAL are approximate. **`RTLD_NEXT` is defined as `0x00`** (`dlfcn.h:27`), the same value as `NULL`/`RTLD_DEFAULT`. |
| dladdr / dlinfo / dlvsym / dl_iterate_phdr | M | | `dl_iterate_phdr` is needed by modern libgcc unwinding and by sanitizers. |

### Randomness and system information

| Interface | Status | Where | Notes |
|---|---|---|---|
| /dev/random | **P/Broken** | `drivers/misc/random/random.c`; `kernel/random.c` (MT19937 seeded from `system_time()`) | **Not cryptographic.** The driver **isn't built** in the current tree (`drivers/CMakeLists.txt` lacks `add_subdirectory(misc)`), so the image has no `/dev/random` at all. |
| /dev/urandom | M | | |
| getrandom / getentropy / arc4random | M | glibc 2.7 predates them | |
| uname | L | `GP/uname.c` via `get_system_info` | |
| sysconf | L | glibc `sysdeps/posix/sysconf.c` with `GP/getsysstats.c` | `_SC_NPROCESSORS_*` is real; `_SC_OPEN_MAX` = 256. |
| pathconf / fpathconf | L | glibc generic | Static values. |
| limits.h | — | `posix/limits.h` | `OPEN_MAX` 256, `PATH_MAX` 4096, `NAME_MAX` 255, `PIPE_BUF` 4096, `ARG_MAX` 131072, `NGROUPS_MAX` 32. |

### Feature macros: what the headers claim vs reality

From `G/include/bits/posix_opt.h` (the same file as `GP/bits/posix_opt.h`) and `G/include/unistd.h`:

| Macro | Advertised | Reality |
|---|---|---|
| `_POSIX_VERSION` | 200112L | Roughly 1996-era coverage plus parts of 2001. |
| `_XOPEN_VERSION` | 600/500/4 depending on feature macros | |
| `_POSIX_JOB_CONTROL`, `_POSIX_SAVED_IDS` | 1 | True. |
| `_POSIX_PRIORITY_SCHEDULING` | 200112L | **False**: every `sched_*` is a stub. |
| `_POSIX_SYNCHRONIZED_IO`, `_POSIX_FSYNC` | 200112L | fsync is true; fdatasync = fsync. |
| `_POSIX_MEMLOCK`, `_POSIX_MEMLOCK_RANGE` | 200112L | **False**: mlock* are stubs. |
| `_POSIX_MEMORY_PROTECTION` | 200112L | True (mprotect). |
| `_POSIX_TIMERS` | 200112L | **False**: `timer_*` are stubs. |
| `_POSIX_SHARED_MEMORY_OBJECTS` | 200112L | **False**: `shm_open` is a stub. |
| `_POSIX_MONOTONIC_CLOCK` | 0 (runtime check) | CLOCK_MONOTONIC actually works; could be 200112L. |
| `_POSIX_CHOWN_RESTRICTED` | 1 | Partly true (ownership checks in chown). |
| `_POSIX_THREADS`, `_POSIX_THREAD_SAFE_FUNCTIONS`, `_POSIX_SEMAPHORES`, `_POSIX_READER_WRITER_LOCKS`, `_POSIX_TIMEOUTS`, `_POSIX_BARRIERS`, `_POSIX_SPIN_LOCKS`, `_POSIX_CLOCK_SELECTION` | **not defined** | Correctly absent, but the missing `_POSIX_THREADS` makes some configure scripts (and libstdc++'s `_GLIBCXX_USE_*` probes) turn threading off entirely. |
| `AF_UNIX`, `AF_INET6`, `SCM_RIGHTS`, `sockaddr_in6` | defined | **False**: not implemented in the kernel. |

## Partial or broken behaviour (consolidated)

1. **`socket(AF_UNIX, ...)` makes a TCP socket** (`net/socket.c:66`; family never validated). It should return EAFNOSUPPORT until AF_UNIX exists.
2. **`socketpair` is emulated over TCP** (`GP/socketpair.c`); SOCK_STREAM only, can't pass fds.
3. **`access()` ignores its mode** (`vfs.c:5142`), and the VFS has no DAC checks on open, exec or search.
4. **mmap:** file offset forced to 0, MAP_PRIVATE file maps return EINVAL, the MAP_FIXED `&=`/`|=` bug, an O_RDONLY check that can never fire, and MAP_SHARED anonymous memory that's actually private (`vfs.c:5155-5262`).
5. **fcntl locks are owned per thread**, not per process (`vfs/flock.c:538`, `568`).
6. **SA_SIGINFO** delivers a mostly zero siginfo and ucontext (`psignals.c:255-275`).
7. **sigaltstack** is a kernel stub (`psignals.c:1266`).
8. **nanosleep** zeroes `rem` even when interrupted (`GP/nanosleep.c`).
9. **getrusage** reports the wrong arithmetic and covers only the calling thread (`GP/getrusage.c`); **wait4** never fills rusage (`schedule.c:1371`).
10. **setrlimit** always ENOSYS; **getrlimit** returns constants (`GP/setrlimit.c`, `getrlimit.c`).
11. **pthread rwlocks all ENOSYS** (`libpthread/rwlock.c`); the default mutex initializer is recursive (`pthread.h:123`); cond_timedwait spawns a thread per wait (`libpthread/cond.c:202`).
12. **pselect/ppoll aren't atomic** with the signal mask (glibc `misc/pselect.c`, `io/ppoll.c`).
13. **poll** is built on select (no `__NR_poll`), so `FD_SETSIZE` caps it.
14. **`RTLD_NEXT == 0`** in Syllable's `dlfcn.h`.
15. **`/dev/random`** isn't cryptographic, and isn't even built into the current image.
16. **posix_opt.h overclaims** timers, shared memory, priority scheduling and memlock.

## Prioritised upgrade list

### P0: unblocks sshd/OpenSSH, HTTPS, and basic correctness

| # | Work | Unblocks |
|---|---|---|
| P0-1 | **Real entropy device**: kernel `/dev/random` and `/dev/urandom` built from a CSPRNG (ChaCha20) seeded from RDRAND/RDSEED when present, plus TSC/interrupt jitter, with a seed file saved across boots. Build the driver (add `misc` to `drivers/CMakeLists.txt`). Add `getentropy()` and `getrandom()` to libc (a syscall, or a read of `/dev/urandom`). | OpenSSL 1.1/3 seeding, ssh-keygen, sshd, ssh, curl HTTPS, Python `os.urandom`/`secrets`, libstdc++ `std::random_device`, git (hash seeds). |
| P0-2 | **AF_UNIX sockets in the kernel** (stream + dgram, filesystem paths, `socketpair` as a real syscall) with **SCM_RIGHTS** fd passing in sendmsg/recvmsg. Make `socket()` reject unknown families. | sshd privsep and `sshd`→`sshd-session` reexec (OpenSSH 9.8+/10), ssh-agent, Python `multiprocessing`/`asyncio` subprocess pipes, Neovim's RPC socket (`$NVIM` / `--listen`), libuv `uv_pipe_*`, dbus, tmux, X11/Wayland-style servers, PostgreSQL local sockets. |
| P0-3 | **posix_openpt / `/dev/ptmx`** (Unix98 ptys) plus `TIOCSCTTY`, keeping `/dev/pty/mst` for compatibility. | sshd pty allocation (`openpty` in modern OpenSSH), tmux/screen, Python `pty`, Neovim `:terminal`, expect. |
| P0-4 | **Enforce permissions**: DAC checks (owner/group/other rwx, root override) in the VFS lookup/open/exec/create/unlink paths; make `access()`/`faccessat()` honour their mode, using the real IDs. | Making sshd's privilege separation, `StrictModes` and per-user `~/.ssh` meaningful; bash's `test -r/-w/-x`; `make`; any multi-user use. Without this, sshd is a remote root shell for every account. |
| P0-5 | **setrlimit/getrlimit for real** (at least NOFILE, CORE, STACK, AS) tracked per process in the kernel; fix getrusage and wait4's rusage. | sshd (its rlimit sandbox treats a failed `setrlimit` as fatal), bash `ulimit`, Python `resource`, make, test suites. |
| P0-6 | **Fix mmap**: honour the file offset, support MAP_PRIVATE file mappings (copy-on-write areas), fix MAP_FIXED, fix the O_RDONLY check, and make MAP_SHARED anonymous memory really shared across fork. | Python (mmap module), git (packfiles), sqlite, gcc/ld (large object files), any modern runtime. |
| P0-7 | **POSIX semaphores** (`sem_init`/`wait`/`trywait`/`timedwait`/`post`/`getvalue`/`destroy`, then `sem_open`) on kernel semaphores, with a `<semaphore.h>`. | Python `_thread`/`threading` locks (CPython uses POSIX semaphores when `_POSIX_SEMAPHORES` is set), `multiprocessing`, libuv `uv_sem_*` (Neovim), libstdc++ `<semaphore>`. |
| P0-8 | **pthread rwlocks** (real implementation on mutex+condvar or kernel semaphores), **pthread_atfork**, and a non-recursive default mutex. | glib/GTK `GRWLock`, libuv `uv_rwlock_*` (Neovim), Python, libstdc++ `std::shared_mutex`, OpenSSL 3 (its `CRYPTO_THREAD_lock_new` uses rwlocks, so lock creation fails). |

### P1: unblocks gcc 15 / libstdc++ C++11 threads and chrono, and modern ports

| # | Work | Unblocks |
|---|---|---|
| P1-1 | **C++11 gthreads** (`__GTHREADS_CXX0X` in `gthr-syllable.h`, or switch GCC to `--enable-threads=posix` on top of a fixed libpthread). Needs: `pthread_mutex_timedlock`, `pthread_condattr_setclock(CLOCK_MONOTONIC)`, `pthread_cond_clockwait`/`pthread_mutex_clocklock` (or let libstdc++ use the REALTIME fallback), `sched_yield`, `nanosleep` with a correct `rem`. Replace the thread-per-call timer in `cond_timedwait` with a kernel timed wait (`lock_semaphore_x` already takes a timeout). | gcc 15 libstdc++ `<thread>`, `<mutex>`, `<condition_variable>`, `<future>`, `<shared_mutex>`, `<stop_token>`, `std::jthread`; Doxygen ≥ 1.8.18 (`docs/cxx11-threads.md`); LLVM/clang; CMake ≥ 3.x; Neovim's C++ deps. |
| P1-2 | **Native TLS (`__thread`)**: a GS/FS-based TLS block per thread in the kernel and loader, the ELF `PT_TLS` program header, the `___tls_get_addr` ABI; then build glibc `--with-__thread` and gcc `--enable-tls`, plus `__cxa_thread_atexit_impl`. | gcc 15 libstdc++ (`thread_local`, emergency EH pool, `std::random_device` state), Python and Rust (fast native `thread_local`), modern glibc (2.41 requires TLS; see `docs/glibc-upgrade.md`). |
| P1-3 | **`sched_yield` and `sched_*` real** (map to `set_thread_priority` and a scheduler yield; `sched_getaffinity` returns the online CPUs). Fix `posix_opt.h` to match. | libstdc++ `this_thread::yield` (`_GLIBCXX_USE_SCHED_YIELD`), spinlocks in libuv/Python/OpenSSL, `nproc`-style probes. |
| P1-4 | **The `*at()` family** in the kernel (an fd-relative lookup base already exists: `__NR_based_open`, `based_mkdir`, `based_rmdir`, `based_unlink`, `based_symlink`), plus `fstatat`, `utimensat`/`futimens` (nanosecond times), `renameat`, `linkat`, `readlinkat`, `fchmodat`, `fchownat`, `faccessat`, `mknodat`. | coreutils ≥ 8 / gnulib (they emulate these slowly, via `/proc/self/fd` that doesn't exist here, or fail), git, Python `os` (`dir_fd=`), Neovim's libuv `uv_fs_*`, `find`/`rm -r`. Pairs with the glibc 2.41 move. |
| P1-5 | **Hard links (`link`)** in the VFS and AFS. | `ln`, git (hard-linked object copies, `git clone --local`), `make install`, dpkg/rpm-style tools, maildir, Python `os.link`. |
| P1-6 | **O_CLOEXEC, F_DUPFD_CLOEXEC, dup3, pipe2, accept4, SOCK_CLOEXEC/SOCK_NONBLOCK** (kernel flags plus glibc wrappers; free with glibc 2.41). | Race-free fork+exec in threaded programs: libuv/Neovim, Python 3 (requires `O_CLOEXEC`/`pipe2` paths or falls back racily), OpenSSH, systemd-style daemons. |
| P1-7 | **sigaltstack and a full SA_SIGINFO** (si_code, si_pid, si_uid, si_addr, si_value, a real `ucontext_t` with `uc_mcontext`), plus `sigtimedwait`/`sigwaitinfo`/`sigqueue` with queued RT signals. | Python `faulthandler`, Rust/Go runtimes, libsigsegv (used by GNU m4, clisp), the gdb/gdbserver signal path, Boehm GC, JITs; libuv signal handling (Neovim). |
| P1-8 | **Real poll() syscall plus atomic pselect/ppoll** (kernel-side mask swap), then an event mechanism (kqueue- or epoll-style) and eventfd. | libuv (Neovim) and Python `selectors`/`asyncio` without the `FD_SETSIZE` cap and races; node, nginx. |
| P1-9 | **CLOCK_PROCESS_CPUTIME_ID / CLOCK_THREAD_CPUTIME_ID, `times()`, `timer_create`** (POSIX timers on kernel timers delivering signals or threads). | libstdc++ `<chrono>` CPU clocks, Python `time.process_time`/`thread_time`, `bash` `times`/`time`, make, profilers. |

### P2: completeness and wider ports

| # | Work | Unblocks |
|---|---|---|
| P2-1 | **IPv6** in the kernel network stack; fix the getaddrinfo IPv6 assertion. | sshd without `AddressFamily inet`, curl/git over IPv6, Python `socket.has_ipv6`. |
| P2-2 | **POSIX shared memory** (`shm_open` on a tmpfs `/dev/shm`) and **POSIX message queues**; optionally SysV IPC. | Python `multiprocessing.shared_memory`, PostgreSQL, X11 MIT-SHM, Chromium-style multiprocess apps. |
| P2-3 | **mlock/madvise/posix_madvise/posix_fadvise/mincore** as real calls or harmless no-ops returning 0 (and correct `_POSIX_MEMLOCK`). | gpg/OpenSSH (`mlock` of key material), databases, JITs. |
| P2-4 | **Locales**: run `localedef` at image build for at least `C.UTF-8` and `en_US.UTF-8`. | Python's UTF-8 mode probing, Neovim/bash multibyte input, `wcwidth` correctness, `sort`/`grep` with UTF-8. |
| P2-5 | **libdl completeness**: `dladdr`, `dl_iterate_phdr`, `RTLD_DEFAULT` distinct from `RTLD_NEXT`, `dlvsym`. | Modern libgcc unwinder (gcc 15), Python extension loading diagnostics, backtraces, sanitizers. |
| P2-6 | **pthread extras**: barriers, spinlocks, `pthread_setname_np` (wrap `rename_thread`), robust and priority-inheritance mutexes, `pthread_getcpuclockid`, real cancellation points. | OpenMP/libgomp, libuv barriers, debugging (thread names in gdb), glibc 2.41's NPTL expectations. |
| P2-7 | **Process-scoped fcntl locks** (owner = process, released on any close by that process), real BSD `flock` on the open file. | sqlite, git, Python `fcntl.flock`, mail tools. |
| P2-8 | **Correct the headers**: drop `_POSIX_TIMERS`, `_POSIX_SHARED_MEMORY_OBJECTS`, `_POSIX_PRIORITY_SCHEDULING`, `_POSIX_MEMLOCK*` until they're real; set `_POSIX_MONOTONIC_CLOCK` 200112L; define `_POSIX_THREADS` / `_POSIX_THREAD_SAFE_FUNCTIONS` (they are true); hide `AF_UNIX`/`AF_INET6` or implement them. | Configure scripts making correct choices instead of linking stubs that fail at run time. |
| P2-9 | **glibc 2.41 migration** (`docs/glibc-upgrade.md`). Most P1 libc items (dup3, pipe2, accept4, getrandom, getentropy, aligned_alloc, `*at`, `posix_spawn` file actions, C11 `<threads.h>`) come with it once the kernel supplies the syscalls above. | Current ports in general (Python 3.12+, Neovim 0.10+, bash 5.2, coreutils 9, OpenSSH 10, gcc 15 target libraries). |

### What each goal needs, at a glance

- **sshd/OpenSSH 10:** P0-1, P0-2, P0-3, P0-4, P0-5 (and P2-1, or keep `AddressFamily inet`). P1-6 is strongly advised.
- **gcc 15 / libstdc++ threads and chrono:** P1-1, P1-2, P1-3, P0-8 (shared_mutex), P0-7 (`<semaphore>`), P1-9 (CPU clocks), P2-5 (`dl_iterate_phdr` for the unwinder).
- **Python 3.x:** P0-1, P0-2, P0-6, P0-7, P0-8, P1-2 (3.12+), P1-4, P1-6, P1-7, P2-4.
- **Neovim (libuv):** P0-2, P0-3, P0-6, P0-7, P0-8, P1-4, P1-6, P1-8, P1-7.
- **bash 5.x:** P0-4, P0-5, P1-9 (`times`), P0-3 (for job control under sshd); otherwise works on what exists.
