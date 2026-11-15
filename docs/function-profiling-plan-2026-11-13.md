# Pyro function profiling plan

Nov 13, 2026 · Rick

No glibc changes are needed for the recommended approach: a kernel sampling profiler that records whole call stacks, plus a small userland tool to read and symbolize them.

## Short answer

Mostly no. The libc that ships today already exports every hook a profiler needs: `mcount`, `__monstartup`, `_mcleanup`, `profil`, `setitimer` (a real syscall, 220), `backtrace` and the `__cyg_profile_func_enter/exit` stubs. The gaps are in the kernel, plus one build flag.

- **Sampling profiler (recommended):** no glibc change at all. The work is in the kernel and a new userland tool.
- **gprof (`-pg`):** one small glibc build change, because the Syllable add-on sets `profile=no`, so `gcrt1.o` and `libc_p.a` are never installed and `gcc -pg` cannot link. It also needs the kernel timer fixes below.
- **`-finstrument-functions`:** no glibc change. You supply the two hook functions in your own library.

## What exists today

The pieces are mostly there; the kernel's profiling timer is what's broken. Checked against `Syllable/` (main) and the built `libc.so.2` in `Syllable-Build-System/out/libraries`.

| Piece | Status | Evidence |
| --- | --- | --- |
| glibc gmon (`mcount`, `monstartup`, `_mcleanup`, `profil`, `sprofil`) | Built and exported | `nm -D libc.so.2` |
| `gcrt1.o`, `libc_p.a` | Missing, so `gcc -pg` can't link | `profile=no` in `glibc-2.3.2/sysdeps/unix/sysv/syllable/configure`; neither file in build outputs |
| GCC `-pg` spec | Ready: picks `gcrt1.o`, `-lc_p` | `gcc/config/i386/syllable.h:58` |
| gprof binary | Built natively | `toolchain/scripts-late/12-native-gcc-4.1.2.sh:38` |
| `setitimer` / `getitimer` | Real syscalls 220/221 | libc disassembly, `syscall.s:814` |
| `ITIMER_PROF` in kernel | Broken: sends SIGTRAP not SIGPROF, compares wall-clock deadline to CPU time so it almost never fires, per-thread only | `psignals.c:672-677`, `insert_timer` at `psignals.c:595` |
| Timer list removal | Bug: removing the head node empties the whole list | `psignals.c:581`, `psignals.c:704` |
| Signal delivery to busy threads | Works: timer IRQ returns through `handle_signals` | `syscall.s` `TSIHand` and `smp_preempt` jump to `ret_from_sys_call1` |
| Timer tick | 1000 Hz PIT plus per-CPU APIC timer | `inc/pit_timer.h:27`, `smp.c:273` |
| Frame pointers | Kept everywhere: GCC 4.1.2 doesn't omit them by default on i386 and no build sets `-fomit-frame-pointer`; libc functions start with `push %ebp; mov %esp,%ebp` | grep of build files, libc bytes |
| `backtrace()` | ebp-chain walker, no DWARF | `sysdeps/.../syllable/backtrace.c` |
| Kernel stack walker | Exists for crash dumps | `trace_stack()` in `fault.c:131` |
| `-finstrument-functions` | Compiler supports it; libc hooks are empty stubs; no exclude-list flag (that arrived in GCC 4.3) | libc bytes `55 89 e5 5d c3` |

## Options

Sampling is the only option that profiles every thread and every shared library without rebuilding anything.

|  | A. Kernel sampler | B. gprof (`-pg`) | C. `-finstrument-functions` | D. QEMU gdbstub sampling |
| --- | --- | --- | --- | --- |
| What you get | Full call stacks for every thread, user and kernel, flat profile, call tree, flame graph | Flat profile and caller/callee counts for the main executable | Exact call counts and inclusive time per function and per stack | Rough stacks from the host |
| Recompile needed | No | Yes, with `-pg` | Yes, with the flag | No |
| Sees shared libs (`libsyllable`, `libc`) | Yes | No | Only if they're rebuilt with the flag | Yes |
| Sees window threads | Yes | No, the timer is per-thread | Yes | Yes |
| Overhead | Low, about 1% at 1 kHz | Moderate | High, 2 to 10x on small functions | Very high, pauses the VM |
| glibc work | None | Turn on profile build | None | None |
| Kernel work | Sampler in the timer IRQ, ring buffer, syscall or device | Fix `ITIMER_PROF` (four bugs) | None | None |
| Toolchain work | None | None | None | Host script |
| New userland | `profile` tool and symbolizer | None, gprof exists | Small hook library | Host script |

B is the cheapest to switch on, but in Syllable most GUI work runs in per-window threads and in `libsyllable.so`, and gprof sees neither. C fits timing one specific function exactly, and can sit next to A later.

## Recommendation: a kernel sampling profiler

Build option A. It answers both asks, "profile any function" and "profile any stack", from one data set: every sample is a full call stack, so any function can be looked at by its own time, its inclusive time, its callers or its callees.

How one sample flows:

1. **Tick.** On each timer interrupt (PIT on CPU 0, APIC timer on the others), if the running thread belongs to a profiled target, the handler records the CPU, thread, process, time and `eip`.
2. **Kernel frames.** If the thread was in kernel mode, the handler walks the kernel ebp chain right there, the same walk `trace_stack()` already does.
3. **User frames.** It does not touch user memory inside the IRQ, since a page could be missing. It sets a per-thread "sample pending" flag. On the way back to user mode, which already passes through `handle_signals` in `ret_from_sys_call1`, the thread walks its own user ebp chain with `memcpy_from_user`, capped at 64 frames and bounded by its stack area.
4. **Buffer.** The finished record goes into a per-CPU ring buffer held in a kernel area that the tool maps read-only.
5. **Read and symbolize.** A new `profile` tool drains the buffer, snapshots the target's loaded images, and turns addresses into `image+offset` and then function names from each ELF's symbol tables, demangling C++ names with `__cxa_demangle`.
6. **Report.** Flat profile (self and inclusive), call tree, per-thread split, "focus on function X" (callers and callees of X), and folded stacks for `flamegraph.pl` on the Mac.

Intended usage: `profile run <cmd>`, `profile attach <pid> [-t <thread>]`, `profile system -d 10`. The interface is a few new syscalls called with inline wrappers in the tool, so libc stays unchanged.

Two known limits. Hand-written assembly without a frame setup loses one frame, which matters mainly in libc's string routines. Code built with `-fomit-frame-pointer` later, such as when you move to GCC 9, would need that flag turned off or a DWARF unwinder.

## Work breakdown

Rough sizes are new or changed lines of code. Each step boots and is testable in QEMU on its own.

| Step | What | Where | Size |
| --- | --- | --- | --- |
| 1 | Fix the itimer bugs: SIGPROF instead of SIGTRAP, CPU-time deadlines for VIRTUAL and PROF, the head-removal bug. Worth doing whatever you pick | `psignals.c` | about 40 |
| 2 | Sampler core: target table, IRQ hook, kernel walk, pending flag, user walk on return to user, per-CPU ring buffer | new `kernel/profiler.c`, hooks in `timer.c`, `smp.c`, `psignals.c` | 400 to 600 |
| 2b | Optional off-CPU profiling: record the stack and wait time each time a thread blocks | `schedule.c`, same buffer and tool | about 150 |
| 3 | Syscalls: start, stop, map buffer, image snapshot | `syscall.s`, `syscall.h`, `syscalltable.h` | about 100 |
| 4 | `profile` tool: run, attach, system modes; ELF symbolizer; reports and folded output | new app under `system/apps/utils` | 800 to 1,200 |
| 5 | Test programs with known hot spots, single and multi-threaded, plus a GUI app run | `test/` | about 200 |
| Later | Optional: turn on the glibc profile build so gprof works too; a `-finstrument-functions` hook library for exact timing of one function | glibc add-on, new lib | small each |

## Profiling GUI apps

Option A covers GUI apps: it samples the main thread and every window's looper thread, and names frames inside `libsyllable.so`, so stacks read like `MyView::Paint → os::View::DrawString`, split by window thread.

- Drawing runs in the appserver process. Profile the app and appserver together (`profile system`, or attach to both) to see the time the app spends waiting on it; each sample carries its process and thread.
- Sampling only sees threads that are running. For a UI that feels slow while the CPU is idle (blocked on a message port, lock or disk), add step 2b: record the stack and wait time each time a thread blocks, about 150 lines in the scheduler, using the same buffer and tool.

## Open questions

- Do you also want kernel time in the stacks (for example, time spent in the appserver's syscalls), or user code only to start?
- Is `Syllable/` on `main` the tree to build this in? The older `Pyro/Native` tree has the same timer code and bugs.
- Do you want gprof working as well, or is the sampler enough?
