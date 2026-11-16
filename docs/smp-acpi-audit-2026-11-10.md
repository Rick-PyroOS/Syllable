# Kernel SMP and ACPI audit (2026-11-10)

Read-only audit of the Syllable (Pyro) kernel's SMP and ACPI code, looking for memory bugs and correctness problems. The source was `Rick-PyroOS/Syllable` at `ca83ddf` (2026-11-09), from the public GitHub repo, because Rick's Mac was offline.

**Files covered**

- SMP: `system/sys/kernel/kernel/smp.c`, `apic.c`, `smp_entry.asm`, `inc/smp.h`, `inc/apic.h`, `inc/acpi.h`
- Locking: `include/syllable/kernel/spinlock.h`, `include/syllable/syllable/atomic.h`
- ACPI glue: `kernel/drivers/bus/acpi/osl.c`, `acpi.c`, `ec.c`, `thermal.c`, `scan.c`, `wakeup.c`

The bundled ACPICA interpreter (version 20080321) was not audited line by line. See section 4.

Severity: **Critical** means it breaks boot or corrupts memory on common hardware. **High** means memory corruption, deadlock or a use-after-free on a realistic path. **Medium** means wrong behaviour, a leak, or a narrow race. **Low** means cleanup or hardening.

---

## 1. Summary of the most important findings

| # | Sev | Where | Problem |
|---|-----|-------|---------|
| S1 | Critical | `smp_entry.asm` / `SmpTrampolineCfg_s` | The trampoline header is 3 bytes with current NASM, but C assumes 4. Every AP jumps to a garbage address. |
| S2 | Critical | `apic.c:175` | The MADT APIC ID (0–255) indexes `g_asProcessorDescs[16]` with no bounds check. |
| S3 | High | `apic.c:432` | The MPC bounds check is `>` where it should be `>=`. APIC ID 16 writes one past the array. |
| S4 | High | `apic.c:162-196` | The MADT walk has no `length == 0` or overrun check. A zero-length entry loops forever. |
| S5 | High | `apic.c:404-489` | The MPC walk has no `default:` case, so an unknown entry type loops forever. |
| S6 | High | `apic.c:232` | RSDP revision 2 or higher is *rejected*, so ACPI SMP detection is skipped on every machine made after about 2000. |
| L1 | High | `spinlock.h` | Recursion is detected per CPU, not per thread. With interrupts on, a preempted holder lets another thread on the same CPU into the lock. |
| A1 | High | `osl.c:642-656` | `acpi_os_acquire_lock` doesn't disable interrupts, and release does `put_cpu_flags(0)`, which **turns interrupts off**. |
| A2 | High | `ec.c:297,320` | `acpi_ec_transaction` calls `spinlock()` where it should call `spinunlock()`. The EC lock is never released, and the code sleeps while holding it. |
| A3 | High | `osl.c:593` | AML deferred work runs on a new kernel thread with a **4 KB stack**. |
| A4 | High | `osl.c:600` + `thermal.c:872` | `acpi_os_wait_events_complete` is a no-op, so thermal and EC teardown free memory that queued work still uses. |
| A5 | High | `osl.c:90-97` | `vsprintf` into a shared static 512-byte buffer, with no bound and no lock. |
| A6 | Medium-High | `osl.c:153` | `acpi_os_unmap_memory` is a no-op, so every AML SystemMemory window move leaks a kernel area. |

---

## 2. SMP and APIC (`smp.c`, `apic.c`, `smp_entry.asm`)

### S1. Critical: the AP trampoline layout doesn't match the C struct

`SmpTrampolineCfg_s` (`inc/smp.h`) expects a 4-byte `tc_nJmpInstr`, then `tc_nKernelEntry` at offset 4, `tc_nKernelStack` at 8, and so on. The assembly starts with:

```nasm
	jmp	start
	db	0
kernel_entry:	dd 0x101693
```

NASM has optimised by default (`-Ox`) since 2.09, so `jmp start` becomes a 2-byte short jump. The build (`kernel/kernel/CMakeLists.txt:29`, and the old `Makefile:68`) passes no `-O` flag. I assembled the file with NASM 2.16.01:

```
default : eb 15 00 | 93 16 10 00 ...   -> kernel_entry at offset 3
-O0     : e9 15 00 00 | 93 16 10 00    -> kernel_entry at offset 4 (what C expects)
```

`install_trampoline()` writes the entry point at offset 4 and the stack at offset 8. The trampoline reads `[kernel_entry]` at offset 3 and `[kernel_stack]` at offset 7. So each AP loads `ESP` and the far-jump target from byte-shifted values, jumps into the weeds with a zero-limit IDT, and triple-faults. Under QEMU a triple fault resets the whole VM. On real hardware the AP shows up as "CPU #n is not responding". The same off-by-one also hits the warm-reset vector at `0x467`, which is set to `sizeof(SmpTrampolineCfg_s)` = 24, while `start` is at 0x17 = 23.

**Fix:** use `jmp near start` (or `jmp word start`) so the header is always 4 bytes. Better still, add `times 4-($-$$) db 0` before `kernel_entry`, and add a C `static_assert`/runtime check that `g_anTrampoline[4..7] == 0x00101693`. **Verify:** boot QEMU with `-smp 2` before and after.

### S2. Critical: the MADT APIC ID isn't bounds-checked

```c
g_asProcessorDescs[psP->amp_nApicId].pi_bIsPresent = true;   // apic.c:175
```

`amp_nApicId` is a `uint8`, and `MAX_CPU_COUNT` is 16. APIC IDs are often sparse or large: AMD parts, Intel parts with HT off (0,2,4,…), multi-socket Xeons, and any QEMU/KVM `-smp` topology with gaps. An ID of 16 or more writes past `g_asProcessorDescs` into whatever `.bss` follows it. This happens during early boot, before anything can catch it.

The same assumption runs through the rest of the SMP code. `get_processor_id()` returns the *APIC ID*, and that ID is used directly as an index in `get_processor()`, `calibrate_delay()`, `ap_entry_proc()`, `set_bit(nCPU, &g_nCpuMask)` (a 32-bit mask), `SetTR((8 + nCPU) << 3)` (a GDT TSS slot), and `init_apic_ldr()` (`1UL << nCPU` in an 8-bit logical-ID field, so it breaks at 8 or more CPUs). In `init_apic()` (`apic.c:760-764`), `g_nBootCPU = get_processor_id()` has the same problem.

**Fix:** keep a proper APIC-ID → logical-index table, filled in while parsing MADT/MPC, and skip entries once `MAX_CPU_COUNT` is reached. Index every per-CPU structure by logical index. Make `get_processor_id()` return the logical index, via the table or a per-CPU GS/FS slot. `init_apic_ldr` needs flat-mode limits: at most 8 CPUs, otherwise use cluster mode or physical destination mode.

### S3. High: the MPC bounds check is off by one

`apic.c:432`: `if( m->mpc_nAPICID > MAX_CPU_COUNT )` should be `>=`. APIC ID 16 writes `g_asProcessorDescs[16]`.

### S4. High: the MADT entry walk isn't validated

`apic.c:162-196`:

- If `ame_nLength == 0`, the loop never advances, so boot hangs.
- Nothing checks that `psEntry + sizeof(entry) <= nMadtEnd` before reading a `AcpiMadtProcessor_s`, so a truncated table is read out of bounds.
- `ath_nLength` is trusted for the mapping size. A corrupt huge length makes `create_area` fail, which is handled, but a moderately bad one maps and walks garbage.

**Fix:** `if (len < 2 || (uint8*)psEntry + len > end) break;`, and check `len >= sizeof(*psP)` per type.

### S5. High: the MPC entry walk hangs on an unknown entry type

`smp_read_mpc()` has no `default:` case. An unknown or vendor entry type (the MP spec 1.4 extended entries live in a separate table, but corrupt BIOS tables exist) never advances `mpt`/`count`, so it loops forever. `mpc_nSize` isn't sanity-checked either. **Fix:** a `default:` case that bails out of the walk.

### S6. High: modern RSDPs are rejected and the RSDT walk is fragile

`smp_scan_acpi_config()` does:

```c
if ( psRsdp->ar_nRevision >= 2 ) continue;
```

Every ACPI 2.0+ firmware (all PCs since about 2000, and every QEMU machine type) has revision 2. The ACPI path is therefore never used, and detection falls back to MP tables. Many UEFI and newer BIOSes no longer ship MP tables, and there the machine boots single-CPU. A revision-2 RSDP still carries a valid 32-bit `RsdtAddress`, so the RSDT path would work unchanged. Using the XSDT is the better fix.

Related problems in the same path:

- **Checksum read past the mapping.** The RSDT is mapped as *one* page (`apic.c:82`), but the header read and `mpf_checksum(psRsdt, ath_nLength)` can cross into the next page when the RSDT sits near a page end or is longer than about 4 KB. That is an unmapped read, which page-faults.
- **Only 8 tables.** `AcpiRsdt_s.ar_nEntry[8]` and `nTableEnt > 8 → 8` mean the MADT is missed whenever it isn't among the first 8 RSDT entries. Real firmware commonly has 10–30 tables.
- **Wrong scan regions.** `init_apic()` scans `0x0–0x400`, which is the real-mode IVT, plus `0xE0000–0x100000`. The spec says to scan the first 1 KB of the **EBDA** (segment at `0x40E`) plus `0xE0000–0xFFFFF`. ACPICA's `acpi_find_root_pointer()`, already in the tree, does this correctly. The kernel could reuse it, or take the RSDP from the multiboot info.
- **Scan keeps going after a hit.** The scan loop doesn't stop when it finds a valid RSDP, so a later stale copy can overwrite `nCPUCount`.
- **Leaked areas on error.** The early returns at `apic.c:125` and `apic.c:147` leak `hRsdtArea` (and `hEntry`). `return 1` on `create_area` failure (`apic.c:86`) is reported as "1 CPU found".
- **Overrides ignored.** `ACPI_MADT_LAPIC_OVR` is printed but not applied, and x2APIC entries (type 9) are ignored.
- `g_bAPICPresent = true` is set as soon as a MADT is seen, even if no CPU entries are enabled.

### S7. Medium: `smp_init_default_config()` reads the APIC address, not the APIC

```c
g_nBootCPU = GET_APIC_LOGICAL_ID( g_nApicBase + APIC_ID );
```

This passes the *address* `0xFEE00020` to a macro that does `(x >> 24) & 0xff`, so `g_nBootCPU = 0xFE`, which is far out of bounds everywhere. The comment says the MMU is off, but `init_smp()` runs after `enable_mmu()` (`init.c:813`). This only triggers on MP "default configuration" machines (old 486/P5 boards), but it is plainly wrong. The fix is to read the register after it is mapped, or to use CPUID leaf 1 EBX[31:24].

### S8. Medium: AP bring-up races and late-responding APs

- **Thread table written without a lock.** `ap_entry_proc()` sets `pi_bIsRunning = true` *before* `create_idle_thread()`. The BSP then moves straight on to the next AP, so two APs can be inside `create_idle_thread()` at once. That function calls `MArray_Insert(&g_sThreadTable, …)` **without `sched_lock()`** (`fork.c:440`), while every other caller holds it (`threads.c:930,1048`). The fix is to take `sched_lock()` there, or to signal "alive" only after the idle thread exists.
- **`g_nLogicalCPU += 1`** in `ap_entry_proc()` is a non-atomic read-modify-write on a shared counter. The BSP also rewrites `g_anLogicToRealID[]` at the end (`smp.c:542-548`), so the AP's write is redundant and racy. Remove it.
- **APs that answer after the 5 s timeout** are a problem. The BSP gives up and reuses the *same* trampoline page for the next CPU (`install_trampoline()` re-`memcpy`s it, with a new stack). A late AP can then read a torn config or share the next AP's stack. It also gets into `g_nCpuMask` but not into `g_nActiveCPUCount`. **Fix:** after a timeout, send INIT to park the AP, or use a fresh trampoline and stack per AP.
- **AP stacks leak, and are unchecked.** `install_trampoline()` does `kmalloc(4096, MEMF_KERNEL) + 4092` with no NULL check (NULL becomes `ESP = 0xFFC`). That stack also isn't freed when the AP fails, or after it switches to its idle thread.
- **Unchecked allocation.** `alloc_gdt_desc()` in `boot_ap_processors()` isn't checked.
- **Trampoline memory is never freed.** The `alloc_real(4096 + 4095)` buffer is kept forever (8 KB of low memory). That's minor.
- **Halt path is unreachable.** In `ap_entry_proc()`, the `halt:` label falls straight after `create_idle_thread()`. It works only because `DoSchedule()` never returns. Add an explicit `for(;;) hlt` before the label, and log the error only on the failure path.

### S9. Medium: TLB shootdown (`flush_tlb_global`)

- **Deadlock that degrades to memory corruption.** The caller spins with IF=0 until every CPU clears its bit. If another CPU is spinning with IF=0 on a spinlock this CPU holds, neither can make progress. After about 50M iterations the flush times out and **returns anyway**, leaving stale TLB entries on the other CPU. That is a silent use-after-free of freed or remapped pages. The timeout should panic, not continue. Callers should never hold a spinlock across `flush_tlb_global()`.
- **Overlapping flushes clobber each other.** `g_nTLBInvalidateMask = g_nCpuMask` is a plain store. A second CPU starting a flush overwrites the first CPU's in-progress mask. That's harmless today because every flush is a full flush, but per-CPU generation counters are the robust design.
- **Bad pointers in the timeout path.** The timeout diagnostics use `nOther = !get_processor_id()`, which assumes exactly 2 CPUs with IDs 0 and 1. They also dereference `g_asProcessorDescs[get_processor_id()].pi_psCurrentThread` without a NULL check.
- **MTRR updates aren't synchronised.** `descriptors.c:282/326` set `g_nMTRRInvalidateMask` after dropping `sched_lock`, and each CPU then reprograms its MTRRs independently. Intel SDM 11.11.8 requires a rendezvous with caches disabled while MTRRs are changed on MP systems. Without it, CPUs can briefly disagree on memory types.

### S10. Low: other SMP items

- **Negative CPU IDs.** `get_cpu_extended_info()` and `update_cpu_speed()` (exported via `ksyms.c`) check only `>= MAX_CPU_COUNT`. A negative `nPhysicalCPUId` reads or writes out of bounds. `strncpy` into `anVendorID`/`zName` may also leave them unterminated.
- **Unlocked CPU count.** `shutdown_processor()` does `g_nActiveCPUCount--` non-atomically, and `g_anLogicToRealID` isn't compacted. `get_system_info()` (`kernel.c:144`) can then dereference a stale CPU's `pi_psIdleThread`.
- **Scheduling with interrupts off.** `shutdown_ap_processors()` calls `Schedule()` in a loop with interrupts disabled to migrate to the boot CPU.
- **Shorthand passed twice.** `apic_send_ipi()` ORs `nTarget` into both ICR2's destination field and the ICR low word. That only works because the one caller passes a shorthand (`APIC_DEST_ALLBUT`). A real CPU number would corrupt the ICR vector or mode bits. Split the parameter into "shorthand" and "destination".
- **Timer stops if bus speed is 0.** In `idle_loop()`, if `pi_nBusSpeed` is 0 (calibration failed), `nAPICDiv` becomes 0 and the APIC timer silently never fires. Scheduling then stalls on that CPU.
- **CPU index mix.** `calibrate_delay()` writes `pi_nCoreSpeed` to `[nProcessor]` but `pi_nDelayCount` to `[get_processor_id()]`. These are the same today, but it is a trap once logical and physical IDs differ.

### L1. High: spinlock semantics (`spinlock.h`)

- **Recursion keyed on CPU, not thread.** `spinlock()` treats "lock held and `sl_nProc == this CPU`" as a recursive acquire. That is only safe if the holder has interrupts disabled. `DEBUG_SPINLOCKS` asserts this, but release builds don't. Thread A takes the lock with IF=1, the timer preempts it, and thread B on the same CPU takes the same lock "recursively" and enters the critical section at the same time. The ACPI driver does exactly this (A1, A2, and `scan.c`/`wakeup.c` with `acpi_device_lock`). **Fix:** enforce IF=0 (assert or panic) in release builds too, or move these users to semaphores or mutexes.
- **No compiler barrier on unlock.** `spinunlock()` releases with `atomic_set()`, a plain volatile store. Non-volatile stores in the critical section can legally be moved after it by GCC. x86 TSO keeps hardware order, but the compiler doesn't have to. Add `barrier()` before the releasing store, or use `__atomic_store_n(..., __ATOMIC_RELEASE)`.

---

## 3. ACPI driver glue (`drivers/bus/acpi`)

### A1. High: ACPICA spinlocks leave interrupts disabled and don't protect against the SCI

```c
acpi_cpu_flags acpi_os_acquire_lock( acpi_spinlock lockp ) {
	unsigned long flags;
	flags = spinlock( lockp );        // returns 0, does NOT cli()
	return( flags );
}
void acpi_os_release_lock(acpi_spinlock lockp, acpi_cpu_flags flags) {
	spinunlock_restore(lockp, flags); // put_cpu_flags(0) => IF cleared!
}
```

- After any ACPICA lock round-trip (GPE handling, `acpi_enable_gpe`, hardware register access), **interrupts are left off** on the calling CPU until something else re-enables them.
- Because acquire doesn't disable interrupts, the SCI handler on the same CPU can take the same lock "recursively" (see L1) while thread context holds it. That corrupts ACPICA's GPE lists and hardware register state.

**Fix:** `return spinlock_disable(lockp);` and `spinunlock_enable(lockp, flags);`. Also check the `kmalloc` in `acpi_os_create_lock()`, which isn't checked today.

### A2. High: the EC lock is never released, and the code sleeps holding it

`ec.c:acpi_ec_transaction()`:

```c
	spinlock(&ec->lock);
	...
		if (ACPI_FAILURE(status)) {
			spinlock(&ec->lock);   // should be spinunlock
			return -ENODEV;
		}
	...
end:
	...
	spinlock(&ec->lock);           // should be spinunlock
```

The lock is taken twice and never released. On one CPU it "works" only because of the CPU-recursive spinlock: the nest count just grows forever. On SMP, the first EC access from another CPU (or from a thread that migrated) **spins forever**. Battery, thermal `_TMP` and `_Qxx` methods all go through the EC on most laptops. On top of that, `acpi_ec_wait()` in interrupt mode calls `sleep_on_sem()` **while holding the spinlock**.

**Fix:** use a semaphore or mutex (as Linux does: `mutex_lock(&ec->lock)`), and pair it with unlocks.

Other EC problems:

- **Uninitialised memory.** `acpi_ec_add()` and `acpi_ec_get_real_ecdt()` use `kmalloc(..., MEMF_KERNEL)` without `MEMF_CLEAR`. `query_pending`, `leaving_burst`, `wait` (in poll mode), `command_addr`/`data_addr` and `global_lock` (if `_GLK` evaluation fails) are left as garbage. Linux uses `kzalloc` here.
- **Dangling pointer.** `acpi_ec_add()` does `kfree(ec_ecdt)` but doesn't set `ec_ecdt = NULL`. A second EC device then double-frees it. Queued `acpi_ec_gpe_query` work still holding `ec_ecdt` becomes a use-after-free (see A4).
- **Leaked semaphores.** The `ec->wait` and `ec_ecdt->wait` semaphores are never deleted.

### A3. High: deferred AML runs on a 4 KB kernel stack

`acpi_os_execute()` → `spawn_kernel_thread("acpi_task", …, 0, 4096, dpc)`. This runs EC `_Qxx` query methods, thermal checks and notify handlers through the ACPICA interpreter, which is recursive and stack-hungry; Linux has hit overflows with 8 KB. Kernel stacks here are plain `kmalloc` with no guard page, so an overflow silently corrupts the neighbouring heap object.

Two more problems in the same function:

- The `spawn_kernel_thread` result isn't checked. On failure, `wakeup_thread(-errno)` runs and `dpc` leaks.
- A fresh thread is created per event, so events run in no particular order and with no limit on how many.

**Fix:** use one or two long-lived ACPI worker threads with a queue (like Linux's `kacpid`/`kacpi_notify`) and at least 16–32 KB of stack.

### A4. High: `acpi_os_wait_events_complete()` is a no-op, so teardown frees memory still in use

`thermal.c:acpi_thermal_remove()` explicitly calls `acpi_os_wait_events_complete(NULL)` ("synchronize deferred task") and then `kfree(tz)`. With the no-op, a thermal check thread that is already queued or running uses freed `tz`. The same applies to EC query work after `kfree(ec)`/`kfree(ec_ecdt)`. With the worker-queue fix in A3, implement this as "drain the queue".

Related: `acpi_thermal_run()` is a kernel timer callback, which likely runs in interrupt context. It calls `acpi_os_execute()`, which does `kmalloc(MEMF_KERNEL)` (can block) and `spawn_kernel_thread()` from that context. Defer it with a flag or semaphore wake-up instead.

### A5. High: unbounded `vsprintf` into a shared static buffer

```c
static char buffer[512];
vsprintf(buffer, fmt, args);
```

ACPICA messages include namespace paths of up to 256 bytes, plus exception text and module names. `__ENABLE_DEBUG__` is defined in the CMake target, so they do reach this function. An over-long message overflows `.bss`. The buffer is also shared across CPUs and threads with no lock, so concurrent messages corrupt each other. **Fix:** `vsnprintf` into a stack buffer, or pass the format straight to the kernel's printk variant.

### A6. Medium-High: mappings are never released

- **`acpi_os_unmap_memory()` is empty.** Every `acpi_os_map_memory()` creates a new kernel area that is never deleted. ACPICA unmaps and remaps whenever an AML SystemMemory OperationRegion access moves outside its 4 KB window (`exregion.c:141-159`), and also during table checks and the RSDP/EBDA scan. Under periodic thermal or EC polling, this slowly uses up kernel address space and area handles. **Fix:** keep a small `{virt → area_id}` table and `delete_area()` on unmap.
- **Area leaked on remap failure.** `acpi_os_map_memory()` doesn't `delete_area()` when `remap_area()` fails.
- **Physical addresses used as virtual.** `acpi_os_read_memory()`/`acpi_os_write_memory()` treat the *physical* address as a virtual pointer. That only works for identity-mapped RAM. FADT/GAS registers in SystemMemory space (the reset register, MMIO GPE blocks, and hardware-reduced platforms) will page-fault or touch the wrong memory. **Fix:** map, access, unmap, or use a cached mapping.

### A7. Medium: semaphore and timer OSL bugs

- **Wrong timeout result.** `acpi_os_wait_semaphore()` puts the *kernel errno* from `lock_semaphore()` into `acpi_status`. `ret` is never set, so `AE_TIME` is never returned for timed waits. ACPICA (and AML `Acquire(mutex, timeout)`) expects exactly `AE_TIME` on timeout. Instead it gets a garbage status, and the method aborts rather than seeing "timed out".
- **Creation not checked.** `acpi_os_create_semaphore()` ignores `max_units` and doesn't check the `create_semaphore()` result.
- **Fake timer.** `acpi_os_get_timer()` returns an incrementing counter, not time in 100 ns units. AML `Timer` loops (common in EC and `_PS0` delays) end up with wrong timeouts. It should be derived from `get_system_time() * 10`.
- **`_OSI` always false.** `acpi_os_validate_interface()` returns `AE_SUPPORT` for everything, so `_OSI("Windows 20xx")` is false. Many BIOSes then take legacy or broken paths, including the EC and backlight code. Linux answers yes to the Windows strings. This is a behaviour choice, but worth knowing about.
- **SCI handled flag.** `acpi_irq()` always returns 0, whether or not ACPICA handled the interrupt. Check what Syllable's shared-IRQ code expects from the return value.

### A8. Medium: device ID strings can be unterminated

ACPICA 20080321 copies `_HID`/`_UID`/`_CID` with `ACPI_STRNCPY(dst, src, 9)` into `char value[9]` (`uteval.c:389`). A 9-character or longer ID (string `_UID`s like this exist) is left **unterminated**. `scan.c:676-680` then does `strcpy(device->pnp.unique_id, uid)` into `acpi_unique_id[9]`. That reads past the source and overflows into the following `acpi_device` fields. **Fix:** terminate explicitly (`value[8] = 0`) in `acpi_ut_copy_id_string`, and use bounded copies in `scan.c`. Newer ACPICA allocates IDs dynamically.

### A9. Low/Medium: other glue issues

- **Stack buffer overflow.** `wakeup.c:acpi_wakeup_device_init()` appends `"XXXX "` per wakeup device into a 255-byte stack buffer with `strcat`. More than 51 `_PRW` devices overflows the stack. Use `snprintf` with a running offset.
- **Iteration across unlock.** `wakeup.c` drops `acpi_device_lock` in the middle of `list_for_each_safe`. `next` can be freed meanwhile.
- **Lock without IRQ protection.** `scan.c`/`wakeup.c` take `acpi_device_lock` with `spinlock()` and interrupts on (see L1).
- **SCI number source.** `acpi_os_install_interrupt_handler()` ignores its `gsi` argument and uses the FADT SCI. That's equivalent today, but it's fragile.

---

## 4. ACPICA itself

The interpreter is **ACPICA 20080321** (`include/acpi/acconfig.h:66`). Since then, upstream has fixed a long list of table-validation, interpreter, and reference-counting bugs, including the unterminated-ID issue in A8, and has added XSDT-first parsing, `_OSI` handling, and much better OperationRegion support. I didn't audit the 100+ ACPICA files line by line. Upgrading ACPICA (the OSL in `osl.c` is the only port layer to rewrite) would remove a whole class of malformed-AML and malformed-table risks at once, and is the recommended route over patching the 2008 code piecemeal.

---

## 5. Suggested order of work

1. **S1** (trampoline): a one-line change. Then boot QEMU `-smp 2` and `-smp 4` to see whether APs come up at all.
2. **S2/S3/S4/S5**: bounds and length checks in the MADT/MPC parsers. These are small and protect early boot.
3. **S6**: accept RSDP revision 2+, use the XSDT, map the full RSDT, and drop the 8-entry cap. Better, reuse ACPICA's root-pointer finder.
4. **A1 + A2**: fix the ACPICA lock wrappers and the EC lock. These are small diffs with a big effect on SMP and laptops.
5. **L1**: make the spinlock IF=0 rule enforced in release builds, so the remaining misuse shows up.
6. **A3/A4/A5/A6**: an ACPI worker queue with a real stack and drain, a bounded printf, and real unmapping.
7. Logical-CPU indexing (S2's wider fix) and the TLB-shootdown timeout → panic (S9).
8. Plan an ACPICA upgrade.

None of this was compiled or run. The S1 layout mismatch was confirmed by assembling `smp_entry.asm` with NASM 2.16.01 and checking the byte offsets. Everything else comes from reading the code.
