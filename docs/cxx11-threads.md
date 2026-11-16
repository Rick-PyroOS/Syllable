# C11 and C++11 threads on Syllable

What it takes to give programs built on Syllable the standard thread APIs:
C++11's `std::thread`, `std::mutex`, `std::condition_variable`,
`std::future` and `thread_local`, and C11's `<threads.h>`.  This covers the
parts in GCC 9.5.0's thread layer, in libpthread and glibc, and in the
kernel.

## Where things stand

GCC 9.5.0's libstdc++ and libgcc get their threads from
`libgcc/config/gthr-syllable.h`, which comes from the gcc-9.5.0 port
(`system/apps/utils/Builder/packages/Syllable/gcc-9.5.0/gcc-9.5.0.patch`).
It is built straight on kernel calls and provides only the "classic" gthreads
interface (`__GTHREADS`):

| gthreads call                         | Built on                                      |
|---------------------------------------|-----------------------------------------------|
| `__gthread_mutex_*`                   | a "benaphore": an atomic counter, plus a semaphore used only under contention |
| `__gthread_recursive_mutex_*`         | a kernel semaphore with `SEM_RECURSIVE`      |
| `__gthread_once`                      | atomic compare-and-swap, spinning with `snooze()` |
| `__gthread_key_*`, get/setspecific    | thread-local data (TLD) slots: `alloc_tld()`, `get_tld()` |

That is enough for exception handling, `static` locals and the emulated
thread-local storage that libgcc uses (emutls).  It is not enough for C++11:
libstdc++'s configure only defines `_GLIBCXX_HAS_GTHREADS` (which enables
`<thread>`, `<mutex>`, `<condition_variable>`, `<future>` and
`<shared_mutex>`) when the thread header also defines `__GTHREADS_CXX0X`.
`gthr-syllable.h` doesn't, so those headers are empty.  Doxygen 1.8.18 and
later, and most current C++ code, don't compile as a result.  That's why the
image has Doxygen 1.8.17 (cmake/ports/10-compilers.cmake).

The tree's own libpthread (`system/libs/libpthread`, `libpthread.so.2`) is
a separate POSIX layer on the same kernel calls.  It is complete enough for
qtools (Doxygen 1.8), but it has gaps; see "libpthread" below.

There is no `<threads.h>`: glibc only gained C11 threads in 2.28.

## What C++11 needs from gthreads

To define `__GTHREADS_CXX0X`, the thread header has to provide:

- **Threads:** `__gthread_t`, `__gthread_create(thread*, func, arg)`,
  `__gthread_join`, `__gthread_detach`, `__gthread_equal`, `__gthread_self`,
  `__gthread_yield`.
- **Timed locks:** `__gthread_time_t` (a `struct timespec`, absolute),
  `__gthread_mutex_timedlock`, `__gthread_recursive_mutex_timedlock`.
- **Condition variables:** `__gthread_cond_t`, `__GTHREAD_COND_INIT` (or
  `__GTHREAD_COND_INIT_FUNCTION`), `__gthread_cond_broadcast`, `_signal`,
  `_wait`, `_timedwait` (absolute timeout), `_wait_recursive`, `_destroy`,
  and `__GTHREAD_HAS_COND`.
- **Static initialisers:** `__GTHREAD_MUTEX_INIT` and
  `__GTHREAD_RECURSIVE_MUTEX_INIT`.  Without them `std::mutex` has no
  `constexpr` constructor, and a global mutex is only safe after its
  constructor has run.  Today's mutexes need an init function, because a
  benaphore needs a semaphore created at run time.

libstdc++ also uses, outside gthreads:

- `nanosleep()` for `this_thread::sleep_for`, and `sched_yield()`.
- `clock_gettime(CLOCK_MONOTONIC)` for `steady_clock`, which all of the
  `wait_for` and `try_lock_for` calls use.
- `sysconf(_SC_NPROCESSORS_ONLN)` for `thread::hardware_concurrency()`.
- `__cxa_thread_atexit_impl` (optional) for `thread_local` destructors.

## Two ways to do it

**A. `gthr-posix.h` over libpthread.** Configure GCC with
`--enable-threads=posix` and link `-lpthread`.  This needs no new header,
and C++ and POSIX code then share one thread layer.  But libpthread must be
fixed first (see below), every C++ program then depends on
`libpthread.so.2`, and the toolchain's sysroot needs libpthread and the
Syllable `pthread.h` *before* GCC is built, so the toolchain build order
changes.

**B. Extend `gthr-syllable.h` natively.** Add the calls listed above, on
kernel primitives.  Nothing new to link, and the toolchain build order stays
as it is.

**Recommendation: B, with the kernel work below shared with libpthread.**
Both routes run into the same missing kernel primitive (an atomic
release-and-wait), so do that once in the kernel.  Then use it from
`gthr-syllable.h` and from libpthread's condition variables, which fixes
both.

## Kernel work

### 1. An atomic "check-and-wait" for user space (required)

A condition variable has to release the mutex and go to sleep as one step.
Otherwise a signal sent in between is lost, and the waiter sleeps for ever.
The kernel already has this, but only inside the kernel:

- `unlock_and_suspend(wait_queue, sema)` and
  `spinunlock_and_suspend(wait_queue, lock, flags, timeout)` in
  `system/sys/kernel/kernel/sema.c`
- `sleep_on_sem()` and `wakeup_sem()`, which use a semaphore as a wait
  queue

None of these is a system call.  User space only has
`lock_semaphore_x()`, `unlock_semaphore_x()`, `suspend_thread()` and
`resume_thread()`.  That's why libpthread's `pthread_cond_wait()` unlocks the
mutex and *then* calls `suspend_thread()`: the race is real (cond.c,
`__pt_do_cond_wait`).

There are two options:

- **(a) A futex-style pair (preferred):**
  `sys_futex_wait(int *addr, int expected, bigtime_t timeout)` sleeps if
  `*addr == expected`, checking under the same lock that wake takes.
  `sys_futex_wake(int *addr, int count)` wakes up to `count` sleepers on
  `addr`.  The kernel keys sleepers on the physical address: a hash of wait
  queues in `sema.c`, made from the existing `WaitQueue_s`.  This lets
  mutexes and condition variables be plain integers in user memory, which
  gives the static initialisers (`{0}`) that C++11 wants.  The benaphore's
  per-mutex semaphore goes away.  Every modern thread library (glibc's NPTL,
  musl, Haiku's user mutexes) is built this way.
- **(b) Export what's there:** a `sys_unlock_and_sleep(wait_queue, sema,
  timeout)` built on `spinunlock_and_suspend`, plus
  `sys_wakeup_sem(wait_queue, all)`.  This is smaller, but every condition
  variable still needs a kernel semaphore created at run time, so there are
  no static initialisers.

Either way, add the call or calls to `syscalltable.h` and `syscall.h`, the
glibc 2.7 Syllable port's system call stubs
(`Syllable-Ports/archives/glibc-2.7.tar.bz2`, its `sysdeps/.../syllable`
tree), and `atheos/semaphore.h`.

### 2. Timeouts against a clock (required)

The kernel's timeouts are relative microseconds (`bigtime_t`).  C++11 passes
absolute `timespec`s, against `system_clock` or `steady_clock`.

- `get_raw_system_time()` (µs since boot) is monotonic: make it glibc's
  `CLOCK_MONOTONIC` in `clock_gettime()`, if the port doesn't do so already.
  Check `clock_gettime` and `clock_getres` in the glibc 2.7 port.
- Relative timeouts are fine for the system calls themselves: the gthreads
  layer turns absolute into relative just before it sleeps.  If the wait is
  cut short (a signal, `EINTR`) it works out the remainder again.
- A wake-up must report "timed out" (`ETIME`, mapped to `ETIMEDOUT`)
  separately from "woken", for `cv_status::timeout`.

### 3. Thread lifetime (check, probably fine)

- **Create:** `spawn_thread()` plus `resume_thread()`.  `std::thread`
  passes one `void*` argument, and `spawn_thread` already takes `pData`.
- **Join:** `wait_for_thread()` (sys_wait_for_thread in schedule.c).  Check
  that it can be called by any thread in the process (not only the parent),
  and that it returns at once for a thread that has already exited.
- **Detach:** check that a thread that exits with nobody waiting is reaped,
  and doesn't stay a zombie until the process ends.  Add a "detached" flag
  (`sys_detach_thread`) if it does.
- **Exit:** `exit_thread()` has to run the TLD destructors (`alloc_tld()`
  takes one) before the thread is gone.  libstdc++'s per-thread cleanup and
  `thread_local` destructors (through emutls) rely on that.  Check whether
  libc or the kernel calls them today.

### 4. `thread_local` (works now; native TLS is optional)

GCC 9.5.0 for Syllable uses emulated TLS (emutls): `thread_local`
variables live behind `__gthread_key_*`, that is in TLD slots.  It works
with no kernel change, at the cost of a call on each access.

Native ELF TLS (`__thread` through `%gs`, `PT_TLS` segments) would need:
the kernel to give each thread a TLS block and a `%gs` descriptor (the TLD
is already a per-thread, `%gs`-based area, `TLD_SIZE` = 4 pages, so this is
an extension of it); the ELF loader to lay out `PT_TLS` for the executable
and every library, including ones loaded later; and glibc's
`__tls_get_addr`.  It's a large job and not needed for C++11; leave it for
later.

### 5. Smaller items

- `sched_yield()`: map it to `snooze(0)`, or add a real yield call to the
  scheduler (schedule.c).
- `sysconf(_SC_NPROCESSORS_ONLN)`: from `get_system_info()`'s CPU count,
  for `hardware_concurrency()`.
- Per-thread signal masks: libpthread has `pthread_sigmask`.  Check that the
  kernel keeps the mask per thread, not per process.
- SMP: on i586 the `lock`-prefixed atomics are full barriers.  The futex
  calls must take the same lock as the wake side across CPUs (the existing
  `g_sSemListSpinLock` does this for semaphores).

## Changes outside the kernel

### gthr-syllable.h (GCC 9.5.0 port)

- Mutexes: with futexes, a three-state integer mutex (0 = free, 1 = locked,
  2 = locked with waiters), as in Drepper's "Futexes Are Tricky".  Then
  `__GTHREAD_MUTEX_INIT { 0 }`.  Without futexes, keep the benaphore but
  create its semaphore lazily (compare-and-swap on the `sem_id`), so `{ 0, -1 }`
  can be a static initialiser.
- Recursive mutexes: an owner thread ID and a count around the plain mutex,
  not `SEM_RECURSIVE`, so they have a static initialiser and a timed lock.
- Condition variables: a sequence counter.  Wait reads it, unlocks the
  mutex, and calls `futex_wait(&seq, old, timeout)`; signal and broadcast
  increment it and call `futex_wake`.
- Threads: `__gthread_t` = `thread_id`; create with a small trampoline over
  `spawn_thread`; join with `wait_for_thread`.
- Then define `__GTHREADS_CXX0X 1` and `__GTHREAD_HAS_COND 1`.
- Rebuild: the gcc-9.5.0 patch is used by the toolchain image
  (Syllable-Build-System/toolchain: the cross compiler, step 08) and by the
  native compiler (step 09): `toolchain/build.sh -a`, which `sb setup` now
  runs itself.

### libpthread (system/libs/libpthread)

- `pthread_cond_wait` and `pthread_cond_timedwait`: the same sequence counter
  and futex, which removes the lost-wakeup race and the timer thread that
  `pthread_cond_timedwait` starts for each timed wait (cond.c,
  `__pt_timer_thread_entry`).
- Add `pthread_mutex_timedlock`, `PTHREAD_MUTEX_INITIALIZER` that needs no
  init call, and `pthread_spin_*` (std::atomic doesn't need them, but ports
  do).
- The same `pthread.h` should then be usable from the toolchain's sysroot,
  so that ports can link `-lpthread` (today it is kept out of the staging
  sysroot; see cmake/ports/20-ssl.cmake's curl and the Doxygen port).

### C11 `<threads.h>`

glibc 2.7 has none.  Add a small `threads.h` and `libc11threads`: `thrd_*`
on `spawn_thread`/`wait_for_thread`, `mtx_*`, `cnd_*` and `call_once` on the
same futex code, and `tss_*` on TLD slots.  Install it in
`/system/development/headers` and link it into libpthread, or ship it as a
separate library.  It is a thin layer once the gthreads work is done, about
300 lines.

## Order of work and how to test it

1. **Kernel:** the futex pair (or the exported unlock-and-sleep) and its
   system call stubs; test it with a C program (two threads ping-ponging a
   counter a million times on SMP QEMU, `-smp 2`).
2. **glibc:** `CLOCK_MONOTONIC`, `sched_yield`, `_SC_NPROCESSORS_ONLN`.
3. **libpthread:** condition variables and timed lock on the new calls; run
   its existing users (Doxygen 1.8.17's dot runner, cups).
4. **gthr-syllable.h:** the C++11 part; rebuild the toolchain and native
   compilers (`toolchain/build.sh -a`).  Test with a C++ program that uses
   `std::thread`, `std::mutex`, `std::condition_variable::wait_for`,
   `std::async` and a `thread_local` with a destructor, built by
   `g++-9.5.0 -std=c++17` on Syllable and by the cross compiler.
5. **Ports:** move Doxygen to a current release (1.9 or later, which needs
   `std::thread`), and drop the "no pthreads" workarounds in curl and others.
6. **C11 `<threads.h>`.**

Steps 1 and 3 also fix a real bug in today's programs: the lost wake-up in
`pthread_cond_wait`.
