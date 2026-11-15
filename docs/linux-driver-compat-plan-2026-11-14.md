# Linux driver compatibility layer: audit and plan (2026-11-14)

This is an audit of how Pyro's kernel drivers are built today and a design for a layer that makes porting Linux drivers mostly mechanical. The layer includes a small declarative driver description (the "DSL") that generates the boilerplate. This is a plan only. Nothing here has been implemented.

Source: `Rick-PyroOS/Syllable` at `755304a1` (2026-11-12), read on Rick's Mac. Paths are under `Syllable/system/sys/` unless noted. This plan reuses the findings in `docs/usb-audit-2026-11-10.md`, `docs/smp-acpi-audit-2026-11-10.md`, `docs/network-stack-audit.md`, `docs/arm-i686-x86_64-audit-2026-11-10.md` and `docs/posix-audit-2026-10-30.md`. The DSL follows the style of `docs/ports-dsl.md` (`syllable_port`).

---

## 1. Short answer

- **Pyro already ports Linux drivers, by hand.** 13 of the 16 network drivers and 8 of the 9 OSS sound drivers are Linux drivers from 2001 to 2007. The EHCI driver and the whole USB stack are Linux 2.4/2.6 derived. They share one 492-line shim, `include/syllable/kernel/linux_compat.h`. Each port was then hand-edited: calls were rewritten to native APIs (`kmalloc`, `alloc_pkt_buffer`, `spinlock_cli`, `psBus->read_pci_config`), and about 100 lines of device-node, ioctl and probe glue were copy-pasted into every driver. Because of that, none of them can take an upstream fix.
- **The fix is a proper LinuxKPI-style layer in three tiers:** a small native driver kit (`pdk`, like DDEKit), Linux API headers on top of it (`linuxkpi`, like FreeBSD's), and per-class bridges that connect Linux `net_device`, block, USB and sound drivers to Pyro's existing interfaces. Upstream sources stay unmodified in a vendor directory.
- **The DSL is a `.ldrv` manifest per driver** (about 15 lines) plus a generator. It produces the `device_init`/`device_uninit` entry points, the PCI ID match table (read straight from the driver's `MODULE_DEVICE_TABLE`), the `/dev` node and ioctl glue, the `CONFIG_*` header, stubs for Linux symbols the layer doesn't cover, and an ID manifest so the kernel can load drivers by device ID.
- **Four kernel gaps come first,** because every Linux driver hits them: a real DMA API (today `virt_to_phys` is a cast), the spinlock semantics bug (SMP/ACPI audit L1), driver-to-driver linking (`elf.c:1571` FIXME), and MSI/MSI-X (`IRQ_COUNT` is 16 and `pci_enable_msi` is a stub).
- **Best pilots:** Intel `e1000`/`e1000e` (the QEMU, VirtualBox and VMware default NIC, which Pyro lacks per network audit E6), then `virtio-net` and `virtio-blk` (which bring in `virtio_pci`, useful for the SPICE/virtio-gpu plan), then a re-port of `r8169` from current Linux to compare against today's hand port.
- **Don't route graphics or HDA audio through the layer.** DRM/KMS is too big for the payoff (FreeBSD's `drm-kmod` is the cautionary example), and Pyro's video drivers are appserver userland plugins anyway. The native HDA driver is fine. Use the layer for NICs, storage (virtio-blk, NVMe), USB host/class drivers, and maybe Wi-Fi much later.
- **Licensing fits:** the Pyro kernel is GPLv2, so GPLv2-only Linux drivers can be linked against it, unlike in Haiku (MIT), which is why Haiku went with FreeBSD drivers. The compat layer itself must be GPLv2-compatible. (This is not legal advice.)

---

## 2. How kernel drivers work today

### 2.1 Loading and probing

- A driver is an ELF shared object linked with `-kernel` against `libkernel.so` and installed under `/system/drivers/dev/<class path>` (for example, `net/eth`). The CMake call is `syllable_driver(r8169 SOURCES r8169.c INSTALL system/drivers/dev/net/eth)` (`cmake/Syllable.cmake:117`).
- **Drivers are loaded by directory access, not by device ID.** `kernel/vfs/dev.c:515` `scan_driver_dir()` maps `/system/drivers/dev/...` onto `/dev/...`. When something lists or opens `/dev/net/eth`, every driver in that directory is loaded and its `device_init(int nDeviceID)` runs. `init_devices()` (`kernel/devices.c:547`) only walks `/dev/bus` and `/dev/hcd` at boot.
- **Every driver probes every PCI device itself.** `device_init` gets the PCI bus manager with `get_busmanager(PCI_BUS_NAME, PCI_BUS_VERSION)`, loops over `get_pci_info(&sInfo, i)`, compares against its own table, calls `claim_device()`, then `create_device_node()`. If nothing matched, it calls `disable_device()` so it isn't loaded again. `r8169.c:2068-2134` is the template that nearly all drivers copy.
- **Drivers can't import symbols from other drivers.** The kernel ELF loader only resolves `DT_NEEDED` against `libkernel.so` (`kernel/elf.c:1562-1571`, "FIXME: We should support kernel add-ons to link agains dll's"). Shared code is reached through bus managers instead: a driver registers a named table of function pointers with `register_busmanager()`, and others fetch it with `get_busmanager(name, version)`. PCI, USB, ATA, SCSI and ACPI all work this way.

### 2.2 The core kernel interfaces a driver uses

| Area | Pyro API | Header | Notes for Linux compat |
|---|---|---|---|
| Device registry | `register_device`, `claim_device`, `release_device`, `set_device_data` | `kernel/device.h` | Similar to Linux's `struct device` plus driver binding, but flat (no parent/child tree, no `probe`/`remove` callbacks). |
| /dev nodes | `create_device_node(id, handle, path, &DeviceOperations_s, cookie)` | `kernel/device.h` | `DeviceOperations_s` is open/close/ioctl/read/write/readv/writev/select/mmap. Every class (net, disk, audio) goes through it. |
| PCI | `PCI_bus_s` hooks: `get_pci_info`, `read/write_pci_config`, `enable_pci_master`, `get_pci_capability`, `get_bar_info` | `kernel/pci.h` | 64-bit BAR decode exists (`bus/pci/pci_registers.c:161-231`). There's no MSI, no PCIe extended config (ECAM/MCFG), no AER and no hotplug. |
| IRQ | `request_irq(irq, top, bottom, flags, name, data)`, `release_irq`, `IRQRET_HANDLED` | `kernel/irq.h` | `IRQ_COUNT` is 16 (legacy ISA lines only, `irq.h:26`). There's a bottom-half hook, but no threaded IRQs. |
| Memory | `kmalloc(size, MEMF_KERNEL…)`, `create_area`, `remap_area`, `get_area_physical_address` | `kernel/areas.h`, `kernel/malloc.h` | Kernel space is 0x00100000 to 0x7fffffff with physical RAM mapped 1:1 (`tunables.h:40-44`), which is why `virt_to_phys(p)` is a cast. There's no DMA zone flag (only `MEMF_REAL` for below 1 MB). |
| Locks and sleeping | `SpinLock_s` + `spinlock_cli`/`spinunlock_restore`, semaphores (`create_semaphore`, `lock_semaphore`), `snooze`, `sleep_on_queue` | `kernel/spinlock.h`, `kernel/semaphore.h` | Spinlock recursion is keyed by CPU, not by thread (SMP/ACPI audit L1), so `spin_lock()` without IRQs off isn't safe. That is exactly what Linux `spin_lock` means. |
| Timers and threads | `create_timer`/`start_timer`, `spawn_kernel_thread` | `kernel/timer.h`, `kernel/threads.h` | Enough to build workqueues, `timer_list`, `delayed_work` and completions. |
| Net | `struct net_device` (a 2.4-era subset), `PacketBuf_s`, `alloc_pkt_buffer`, `enqueue_packet`, ioctls `SIOC_ETH_START`/`STOP` | `kernel/net_device.h`, `kernel/net.h` | The interface between a driver and the stack is a `/dev/net/eth/*` node. The stack opens it, passes a `NetQueue_s*` with `SIOC_ETH_START`, and writes frames to the node. |
| Block | `DeviceOperations_s` read/write on a node, `decode_disk_partitions()`, `bcache` | `kernel/device.h` | Disks publish `/dev/disk/<bus>/<n>/raw` and partition nodes. It's synchronous with no request queue. |
| USB | `USB_bus_s`, `USB_driver_s{AddDevice,RemoveDevice}`, `USB_packet_s` (a 2.4 URB) | `kernel/usb.h` | Close to Linux 2.4 `usb_driver`/`urb`, but with many memory-safety bugs (USB audit). |
| Audio | `GenericStream_s` ring helper (`drivers/audio/hda/audio.h`), plus old OSS `soundcard.h` ioctls for `drivers/sound/*` | `drivers/audio/hda/audio.h`, `atheos/soundcard.h` | There are two generations: `audio/` (hda, i8xx, vt82xx, used by the media server) and `sound/` (OSS ports from Linux). |
| Video | Kernel side is only a PCI claim stub (`drivers/graphics/*`, 200 to 600 lines each). The real driver is a C++ appserver plugin in `appserver/appserver/drivers/video/*`. | | Linux fbdev/DRM don't map onto this. See section 7. |

### 2.3 Inventory

| Class | Drivers | Linux-derived? | Shim used |
|---|---|---|---|
| Net | 3c509, 3c59x, dl2k, dp83815, eepro100, ne2000, nvidia (forcedeth 0.49), pcnet32, rtl8139, rtl8169, sis900, skge, tg3 (10.9 kloc), tulip, via, tun | 13 of 16 (not ne2000, tun or 3c509's core) | `linux_compat.h` in 11; local `#define readl inl` hacks in `dl2k.h:47` and `via.c:133-164` |
| Sound (OSS) | emu10k1, es1370, es1371, i810, maestro, sbpro, trident, via686a, vt82xx, ymfpci | 9 of 10 | `linux_compat.h` in 8, plus `i810/linuxcomp.h`; 5 private copies of `ac97_codec.c` |
| Audio (new) | hda, i8xx, vt82xx | Native (hda) | None |
| USB | bus/usb (core, hub, libusb), hcd/usb_uhci, usb_ohci, usb_ehci (Linux 2.6 EHCI), input/usb_hid, disk/usb, misc/usb_printer | All descended from Linux 2.4 | Hand-merged; `kernel/usb.h` is a renamed 2.4 API |
| Storage | bus/ata + hcd/ata_pci, disk/ata (legacy), disk/bios, disk/ram, bus/scsi | Partly (comments credit linux-ide) | None |
| Video | 15 appserver plugins plus kernel claim stubs | Some register logic from XFree86 | None |
| Missing that matter | e1000/e1000e, virtio-net/blk/gpu, AHCI, NVMe, xHCI, any Wi-Fi, HDA on modern codecs (it works but is minimal) | | |

### 2.4 What's wrong with `linux_compat.h` today

It's useful as a list of what drivers needed, but it shouldn't become the base of the new layer:

- **Correctness bugs.**
  - `pci_set_power_state` (`linux_compat.h:332-355`) reads `pm + PCI_PM_CTRL` before `pm` is assigned, so it writes to a random config offset.
  - `spin_lock_irq` (`:173`) doesn't disable interrupts.
  - `disable_irq` is `disable_irq_nosync` (`:414`), so it doesn't wait for a running handler.
  - `pci_find_device` returns a pointer to one shared `static` buffer (`:230`).
  - `request_region` always succeeds (`:159`).
  - `ioremap` (`:135`) creates an area that nothing ever unmaps, and it masks the address with the *I/O* mask.
- **Won't build with a modern compiler.** `pci_set_power_state` and `is_valid_ether_addr` (`:332`, `:442`) have no return type (implicit `int`), which GCC 14 rejects by default.
- **Hard-wired to 32-bit and identity mapping.** `dma_addr_t` is `uint32` (`:41`), `virt_to_phys` is a cast (`:109`), and `pci_alloc_consistent` is `kmalloc`. All of these break on x86_64, on ARM, and on any >4 GB machine (arm/x86_64 audit §7.6). They also only work while kernel memory stays 1:1 mapped.
- **Leaks the native API.** The PCI config macros need a variable called `psBus` in scope. The `netif_*` macros assume the 2.4 `tbusy`/`start` fields. So it's a half-translation, not an API.
- **No MSI.** `pci_enable_msi` (`:309`) returns failure, which most drivers handle, but modern NVMe/xHCI/e1000e want MSI or MSI-X for performance.

Recommendation: freeze `linux_compat.h` for the existing hand ports, fix the four correctness bugs above in place (small, separate change), and build the new layer next to it rather than on it.

---

## 3. Prior art

| Project | What it is | What to copy | What to avoid |
|---|---|---|---|
| **FreeBSD LinuxKPI** (`sys/compat/linuxkpi`) | Linux API headers (`linux/pci.h`, `linux/netdevice.h`, `linux/workqueue.h`, …) implemented on FreeBSD's newbus, busdma, taskqueue and ifnet. It runs drm-kmod (amdgpu, i915), iwlwifi, rtw88/89 and the Mellanox drivers with mostly unmodified upstream sources. | A real `include/linux/` header tree; pinning to one Linux version (`LINUXKPI_VERSION`) so drivers can `#if` on it; bridging Linux `pci_driver` into the native bus as a normal driver; mapping the DMA API onto the native busdma, not onto casts. | It grew to tens of thousands of lines because of DRM and 802.11. Start narrow. |
| **Haiku's FreeBSD compat** (`src/libs/compat/freebsd_network`, `freebsd_iflib`, `freebsd_wlan`) | Haiku (a BeOS sibling of Pyro, so the closest relative) runs nearly unmodified FreeBSD NIC and Wi-Fi drivers. Each driver has a tiny `glue.c` with `HAIKU_FBSD_DRIVER_GLUE(name, devname, pci)` and `HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES | …)`. | **The glue macro is the DSL idea in its simplest form.** Copy the declarative requirements flags ("this driver needs taskqueues / a fast taskqueue / MII") so the compat runtime starts only what each driver uses. Also copy the per-driver "check this interrupt is ours" hook for shared legacy IRQs. | Haiku chose FreeBSD for licence reasons that don't apply to GPLv2 Pyro. |
| **DDE / DDEKit** (TU Dresden for L4; also used by Minix 3 and Genode) | Two tiers: DDEKit is a small OS-neutral C API (memory, PCI, IRQ, threads, timers, locks), and DDE-Linux emulates Linux on top of it. | **The two-tier split.** Pyro's `pdk` (tier 0) is DDEKit. It keeps the Linux emulation free of Pyro internals and gives native drivers a clean API too. | DDE ran drivers in userspace on a microkernel. Pyro's drivers are in-kernel, so skip the IPC parts. |
| **Genode `dde_linux` (lx_emul / lx_kit)** | Genode's current approach compiles larger chunks of *original* Linux code and generates no-op "dummies" for every unresolved symbol with a tool (`create_dummies`). Only what's actually used is written by hand. | **Generated stubs from linker errors.** The generator in section 5 does the same thing: build, collect undefined symbols, emit traced stubs, and report coverage. | Genode keeps a separate emulation per driver. Share one layer instead. |
| **LKL** (Linux Kernel Library) and **NetBSD rump kernels** | Build the whole Linux (or NetBSD) kernel as a library with an arch port, then call into it. | Good reference if Pyro ever wants Linux Wi-Fi or a full Linux USB stack: it's the "port the subsystem, not the driver" option. | Too heavy as the first step: it duplicates the scheduler and memory manager and has two copies of every lock. |
| **NDISwrapper** | Binary Windows NIC drivers on Linux. | Nothing. | Binary compat is fragile. Source compat only. |
| **Pyro's own `syllable_port`** (`docs/ports-dsl.md`) | A CMake function DSL with a declarative field list. | Same feel for the driver manifest: keyword fields, generated build steps, a dependency graph. | |

---

## 4. Proposed architecture

```
   Linux driver sources (vendor/linux/v6.x/drivers/..., unmodified)
          │  #include <linux/...>
   ┌──────▼──────────────────────────────────────────────────────┐
   │ Tier 1: linuxkpi  — include/linux/*.h + liblinuxkpi.a       │
   │  types, errno, printk, kmalloc/slab, list/idr/xarray,       │
   │  spinlock/mutex/completion/wait_queue, timer_list, jiffies, │
   │  workqueue/tasklet, struct device/driver, pci_driver,       │
   │  dma-mapping, io (readl/ioread32/iomap), irq, firmware      │
   ├─────────────────────────────────────────────────────────────┤
   │ Tier 2: class bridges                                       │
   │  lkpi_net (net_device/sk_buff/NAPI ⇄ PacketBuf/NetQueue)    │
   │  lkpi_blk (blk-mq/gendisk ⇄ DeviceOperations + partitions)  │
   │  lkpi_usb (usb_driver/urb ⇄ USB_driver_s/USB_packet_s)      │
   │  lkpi_snd (minimal ALSA PCM ⇄ GenericStream_s) — later      │
   ├─────────────────────────────────────────────────────────────┤
   │ Tier 0: pdk  — Pyro driver kit (native C, OS-facing)        │
   │  pdk_dma_alloc/map, pdk_mmio_map, pdk_irq_*, pdk_pci_*,     │
   │  pdk_thread/timer/sleep, pdk_lock, pdk_log, pdk_node_*      │
   └──────▲──────────────────────────────────────────────────────┘
          │ calls only public kernel APIs
   Pyro kernel (devices.c, PCI/USB bus managers, areas, irq, net, vfs/dev)
```

### 4.1 Tier 0: `pdk` (the Pyro driver kit)

A small, stable C API that is the *only* place compat code touches kernel internals. It's useful for new native drivers too, because it puts each fix in one place. About 1,500 lines.

- **DMA:** `pdk_dma_alloc_coherent(dev, size, &phys, flags)` allocates physically contiguous memory with a real physical address (`get_area_physical_address`, not a cast), optionally below 4 GB (`PDK_DMA32`) or below 16 MB. Also `pdk_dma_map_single/unmap` (a no-op on x86 today, a bounce buffer later for >4 GB), and `pdk_dma_sync_*`. This is where the `MEMF_DMA32` zone from the x86_64 audit lands.
- **MMIO and port I/O:** `pdk_mmio_map(phys, len, uncached)` returns a handle that `pdk_mmio_unmap` frees. It's page-aligned and uses the right area flags. Also `pdk_io_request` with real region tracking.
- **PCI:** a `pdk_pci_dev` handle that wraps `PCI_Info_s` plus the bus manager, with `config_read/write{8,16,32}`, `bar(i)` (64-bit aware via `get_bar_info`), `enable`, `set_master`, `find_capability`, `pm_set_state`, and `irq_vectors(min, max, flags)` (INTx today, MSI/MSI-X once section 6.4 is done).
- **IRQ:** `pdk_irq_request(dev, vec, handler, thread_fn, flags)`. It supports a threaded handler on a per-device kernel thread, because Linux drivers increasingly use `request_threaded_irq`. Also `pdk_irq_sync()`, which waits for a running handler (the missing half of `disable_irq`).
- **Threads, time, locks:** `pdk_spin` (always disables IRQs on the local CPU, so the L1 bug can't bite), `pdk_mutex`, `pdk_cond`, `pdk_sleep_us`, `pdk_time_ns`, `pdk_timer` (one-shot/periodic), and `pdk_worker` (a kernel thread plus queue).
- **Nodes:** `pdk_node_create(dev, "net/eth/%s-%d", ops, cookie)` with numbering, and teardown that is safe to call while the node is open (this fixes the unplug use-after-free pattern in USB audit K4/M1 for new drivers).
- **Logging:** `pdk_log(dev, level, fmt, …)` maps to `kerndbg` with the driver and device name.

### 4.2 Tier 1: `linuxkpi`

A header tree at `system/sys/kernel/compat/linuxkpi/include/linux/…` (plus `asm/`, `uapi/`) and `liblinuxkpi.a`, built with the existing `syllable_static_library(… KERNEL …)`.

**Pin one Linux version.** Everything here targets one LTS (6.12 or whatever LTS is current when work starts), and `LINUX_VERSION_CODE` is set to it. Drivers are imported from that tree only. Moving to the next LTS is a planned job, the way FreeBSD bumps `LINUXKPI_VERSION`.

**How it links,** given the `elf.c` limitation:

1. **Phase 1, static:** link `liblinuxkpi.a` and `libpdk.a` into each driver. That's simple, and each driver gets private copies of globals (its own worker threads and so on), which is acceptable for a handful of drivers. The cost is code size (estimated 60 to 150 KB per driver).
2. **Phase 2, shared:** fix `kernel/elf.c` so kernel images can `DT_NEEDED` other kernel images in `/system/drivers/lib/` (see section 6.3). Then `liblinuxkpi.so` loads once. A bus manager (`get_busmanager("linuxkpi", 1)`) is the fallback if the loader change is unwelcome, but it would mean wrapping every function in a hooks table.

**Coverage, by priority:**

| Group | Headers | Implementation |
|---|---|---|
| Basics | `types.h`, `kernel.h`, `compiler.h`, `bug.h`, `errno.h`, `printk.h`, `bitops.h`, `bitfield.h`, `byteorder`, `unaligned.h`, `minmax.h`, `string.h`, `overflow.h`, `log2.h`, `sizes.h`, `module.h` (`MODULE_*` become no-ops or manifest metadata, `module_param` becomes a boot option) | Mostly header-only |
| Memory | `slab.h` (`kmalloc`/`kzalloc`/`kcalloc`/`kvmalloc`/`kmem_cache`), `gfp.h`, `mm.h` (`alloc_pages`, `page_address`), `vmalloc.h`, `io.h` (`ioremap*`, `readl`/`writel`, `ioread32`, `pci_iomap`), `dma-mapping.h`, `dmapool.h`, `scatterlist.h` | Wraps `pdk_dma_*`/`pdk_mmio_*`. `struct page` is a thin descriptor of a physical page. |
| Data structures | `list.h` (Pyro's `kernel/list.h` is already the Linux one), `hlist`, `rbtree.h`, `idr.h`/`xarray.h` (simple versions), `kref.h`, `refcount.h`, `atomic.h`, `bitmap.h`, `crc32.h`, `etherdevice.h` helpers | Copy GPLv2 implementations from Linux `lib/` |
| Sync | `spinlock.h` (`spin_lock*`, `raw_spinlock`), `mutex.h`, `rwsem.h`, `semaphore.h`, `completion.h`, `wait.h` (`wait_event*_timeout`, `wake_up`), `rcupdate.h` (a simple "synchronize = wait for every CPU to schedule" RCU), `seqlock.h`, `lockdep.h` (no-ops) | On `pdk_spin`/`pdk_mutex`/`pdk_cond` |
| Time | `jiffies.h` (HZ=1000 on `get_system_time`), `delay.h` (`udelay`/`mdelay`/`msleep`/`usleep_range`), `timer.h` (`timer_list`, `mod_timer`, `del_timer_sync`), `hrtimer.h` (basic), `ktime.h` | On `pdk_timer`/`pdk_time_ns` |
| Deferred work | `workqueue.h` (`schedule_work`, `delayed_work`, `alloc_workqueue`, `flush_work`, `cancel_*_sync`), `interrupt.h` (`request_irq`, `request_threaded_irq`, `tasklet_*`, `disable_irq`/`synchronize_irq`), `kthread.h` | On `pdk_worker`/`pdk_irq` |
| Device model | `device.h` (`struct device`, `dev_get_drvdata`, `dev_err`/`dev_info`, `devm_*` with a per-device release list), `pci.h` (`struct pci_dev`, `pci_driver`, `pci_register_driver`, `pci_enable_device`, `pci_request_regions`, `pci_resource_*`, `pci_alloc_irq_vectors`, `pcim_*`), `firmware.h` (`request_firmware` loads from `/system/firmware/`), `of.h`/`property.h`/`acpi.h` (empty or minimal), `pm.h` (stubbed until Pyro's suspend is real), `sysfs`/`kobject` (stubs) | The `pci_driver` bridge: see below |

**The `pci_driver` bridge** is the heart of tier 1. A Linux driver calls `pci_register_driver(&e1000_driver)` from its `module_init`. The generated `device_init()` (section 5) calls that `module_init`. `pci_register_driver` then walks `get_pci_info()`, matches against `id_table` with full Linux semantics (vendor, device, subvendor, subdevice, class/mask, `driver_data`), calls `claim_device()`, builds a `struct pci_dev` wrapping a `pdk_pci_dev`, and calls `probe(pdev, id)`. Teardown calls `remove()` and releases `devm_*` resources in reverse order. If nothing matched, it calls `disable_device()`, exactly as today's drivers do by hand.

### 4.3 Tier 2: class bridges

**`lkpi_net` (first).** It maps Linux `struct net_device` (allocated by `alloc_etherdev`), `net_device_ops`, `sk_buff`, NAPI and `ethtool_ops` onto Pyro's node-based NIC interface:

- `register_netdev()` creates `/dev/net/eth/<drv>-<n>` with a shared `DeviceOperations_s`. `SIOC_ETH_START` stores the `NetQueue_s*` and calls `ndo_open`. `SIOC_ETH_STOP` calls `ndo_stop`. `SIOCGIFHWADDR` reads `dev_addr`. `write()` wraps the frame in an `sk_buff` and calls `ndo_start_xmit`. That replaces the roughly 100 copy-pasted lines in each of the 13 current drivers (for example, `r8169.c:1997-2068`).
- `sk_buff` is a real Linux-shaped struct (head/data/tail/end, `len`, `protocol`, `ip_summed`, frags). Receive (`netif_receive_skb`, `napi_gro_receive`, `netif_rx`) copies the data, or hands it over without copying when it's linear, into a `PacketBuf_s`, then calls `enqueue_packet()`. The zero-copy version would need `PacketBuf_s` to adopt an external buffer. That's a small change to `net/packets.c` and can come later.
- NAPI runs `poll()` on a per-device `pdk_worker` with the weight respected. `netif_carrier_on/off` maps to `IFF_RUNNING`. Offloads (checksum, TSO) are reported as off until the stack can use them (network audit P2).
- `ethtool_ops` and `mii`/`phylib` are stubbed at first. Generic `phylib` is about 3 kloc and is needed by many embedded NICs, but not by e1000e, virtio or r8169.

**`lkpi_blk` (second).** It maps `blk_mq_alloc_disk`/`add_disk` plus a `blk_mq_ops->queue_rq` onto a `/dev/disk/<drv>/<n>/raw` node with `read`/`write`/`readv`/`writev`. It splits I/O into requests at logical-block boundaries (avoiding the SCSI 0xffff chunking bug, USB audit M2), waits on a completion, then runs `decode_disk_partitions()` to publish the partition nodes. Pilots: virtio-blk, then NVMe (PCI, simple queue model, INTx fallback exists). AHCI in Linux sits on libata plus the SCSI midlayer, which is a lot of code. A native AHCI driver on Pyro's `bus/ata` is probably cheaper, so treat AHCI-through-compat as an open question.

**`lkpi_usb` (later).** There are two options, and Rick has to pick (section 8):

- *(a) Class drivers only:* map Linux `usb_driver`, `usb_device`, `usb_interface` and `urb` onto the existing `USB_driver_s`/`USB_packet_s`. It's cheap because the native API is already a renamed Linux 2.4 API, but the result inherits the bus manager bugs from the USB audit.
- *(b) Port the subsystem:* run Linux `drivers/usb/core` + `host/xhci*` + `class/` + `storage/` under linuxkpi as a new `usb` bus manager that replaces the 2003 stack. This is more work (about 40 kloc imported, with perhaps 3 to 5 kloc of new compat), but it brings xHCI (which Pyro lacks entirely, so USB 3 ports and many new laptops have no working USB), UAS, and decades of descriptor hardening. Because the USB audit found Critical issues in almost every layer, (b) is probably the better long-term path. It needs tiers 0 and 1 to be solid first.

**`lkpi_snd` (optional, last).** A minimal ALSA PCM subset (`snd_card`, `snd_pcm_ops`, period interrupts) mapped onto `GenericStream_s`. It's only worth doing for a specific card that the native `hda` can't drive. Full ALSA (controls, mixer, `snd-hda-codec-*`) is very large. Extend the native HDA instead.

---

## 5. The DSL: `.ldrv` driver manifests

### 5.1 Goals

- One small declarative file per ported driver. No hand-written glue C in the common case.
- Upstream sources are untouched. Any necessary change is a `.patch` in the driver directory, listed in the manifest, and the generator warns when a patch no longer applies to a new LTS.
- Information that's already in the Linux source (PCI IDs, `MODULE_*` metadata, Kconfig dependencies) is extracted, not retyped.
- It fits the existing build: the manifest is read by one CMake function, `pyro_linux_driver()`, in the style of `syllable_port()`, which calls a Python generator at configure time.

### 5.2 Syntax

It's line-based, `keyword value…`, with `#` comments and one `driver` block per file. That's small enough to parse in about 150 lines of Python and readable without docs. (TOML was considered. The keyword form matches `syllable_port` and reads better for lists of files.)

```text
# system/sys/kernel/drivers/net/e1000e/e1000e.ldrv
driver   e1000e
linux    6.12  drivers/net/ethernet/intel/e1000e     # LTS pin + upstream dir
sources  netdev.c ethtool.c param.c ich8lan.c 82571.c mac.c manage.c nvm.c phy.c
class    net.ethernet                                # picks the lkpi_net bridge
bus      pci
match    from-source e1000_pci_tbl                   # parse the MODULE_DEVICE_TABLE
node     net/eth/e1000e-{n}
config   CONFIG_PM=n CONFIG_PTP_1588_CLOCK=n CONFIG_E1000E_HWTS=n
needs    workqueue timer napi irq-msi?               # runtime services; ? = optional
firmware none
stub     ptp_* pm_qos_* pcie_aspm_*                  # generate traced no-op stubs
patch    patches/0001-no-ptp.patch
```

A virtio pair shows dependencies between compat modules:

```text
driver   virtio_net
linux    6.12  drivers/net
sources  virtio_net.c
uses     virtio_pci                                   # another .ldrv: built first, linked in
class    net.ethernet
bus      virtio                                       # virtio bus provided by virtio_pci
match    from-source id_table
node     net/eth/virtio-{n}
needs    workqueue napi
```

### 5.3 Field reference

| Field | Required | Meaning |
|---|---|---|
| `driver <name>` | yes | Module and output name. It's installed under the class's `/system/drivers/dev/...` directory. |
| `linux <ver> <dir>` | yes | Which pinned upstream tree and directory under `vendor/linux/<ver>/`. The generator refuses a version that doesn't match `LINUX_VERSION_CODE` of the linuxkpi build. |
| `sources …` | yes | `.c` files relative to `<dir>`. Globs are allowed. |
| `class <class.sub>` | yes | `net.ethernet`, `net.wlan` (future), `block.disk`, `usb.host`, `usb.class`, `sound.pcm`, `misc`. This picks the tier-2 bridge and the default node path. |
| `bus <pci\|usb\|virtio\|platform>` | yes | How devices are matched and enumerated. |
| `match from-source [symbol]` | one of | Extract IDs from the `MODULE_DEVICE_TABLE` (or the named table) by running the preprocessor and a small C parser over the table initializer. |
| `match pci <ven>:<dev>[:<subven>:<subdev>] …` | one of | Explicit IDs, to restrict or extend. |
| `node <path>` | no | `/dev` path template, with `{n}` as the instance number. Defaults by class: `net/eth/<driver>-{n}`, `disk/<driver>/{n}/raw`, … |
| `config KEY=y\|n\|value …` | no | Written to a generated `pyro_config.h` that is force-included. Unset `CONFIG_*` are `n`. |
| `needs …` | no | Runtime services to start, like Haiku's `HAIKU_DRIVER_REQUIREMENTS`: `workqueue`, `timer`, `napi`, `threaded-irq`, `irq-msi`, `firmware`, `dma32`. A trailing `?` means the driver falls back without it. |
| `uses <ldrv> …` | no | Other compat modules (for example `virtio_pci`, `phylib`, `usbcore`), built first and linked in (statically in phase 1, `DT_NEEDED` in phase 2). |
| `firmware <file> …` / `none` | no | Blobs to install into `/system/firmware/`, with SHA256 like `syllable_port`. |
| `stub <glob> …` | no | Linux symbols to satisfy with generated stubs (section 5.5). |
| `patch <file> …` | no | Applied in order to a copy of the sources in the build tree, never to `vendor/`. |
| `define`, `include`, `cflags` | no | Escape hatches, passed to `syllable_driver`. |
| `glue <file.c>` | no | Hand-written extra glue, for the rare driver that needs it. |

### 5.4 What the generator emits

`tools/ldrvgen.py <file.ldrv>` writes the following into `${CMAKE_CURRENT_BINARY_DIR}/ldrv/<name>/`:

1. **`pyro_config.h`:** the `CONFIG_*` values, `KBUILD_MODNAME`, and `LINUX_VERSION_CODE`.
2. **`glue.c`:** about 60 lines, the same for every driver of a class. For `bus pci`, `class net.ethernet`:

```c
/* generated by ldrvgen from e1000e.ldrv — do not edit */
#include <linuxkpi/glue.h>
extern int  __lkpi_module_init(void);   /* aliased to the driver's module_init */
extern void __lkpi_module_exit(void);

static const struct lkpi_driver_desc desc = {
    .name   = "e1000e",
    .klass  = LKPI_CLASS_NET_ETHERNET,
    .node   = "net/eth/e1000e-%d",
    .needs  = LKPI_NEED_WORKQUEUE | LKPI_NEED_TIMER | LKPI_NEED_NAPI,
    .wants  = LKPI_NEED_IRQ_MSI,
};

status_t device_init(int nDeviceID)
{
    return lkpi_driver_start(&desc, nDeviceID, __lkpi_module_init);
}

status_t device_uninit(int nDeviceID)
{
    return lkpi_driver_stop(&desc, nDeviceID, __lkpi_module_exit);
}
/* device_suspend / device_resume forward to dev_pm_ops when CONFIG_PM=y */
```

   `module_init(fn)` in `linux/module.h` expands to an alias `__lkpi_module_init = fn`, so the driver source isn't changed.

3. **`ids.manifest`:** the matched IDs in a simple text form (`pci 8086:10d3 * * e1000e`). The build concatenates all of them into `/system/drivers/dev/drivers.ids` (see section 6.5, driver autoload).
4. **`stubs.c`:** generated stubs (section 5.5).
5. **`CMakeLists` fragment:** a `syllable_driver()` call with the sources (patched copy), `-std=gnu11`, `-include pyro_config.h`, the linuxkpi include path first, Linux's usual `-fno-strict-aliasing -fno-common -fno-delete-null-pointer-checks`, and links to `liblinuxkpi.a` + `libpdk.a` + `uses` modules.
6. **`report.md`:** which Linux symbols the driver uses, which linuxkpi covers, which are stubbed, and which are missing. This is the porting checklist.

### 5.5 Stub generation (borrowed from Genode)

On the first build, the link fails on Linux symbols that linuxkpi doesn't provide yet. `ldrvgen --discover` runs `nm -u` on the objects, subtracts what linuxkpi and `libkernel.so` export, and for each remaining symbol:

- If it matches a `stub` glob in the manifest, it emits a stub that logs once (`pdk_log(..., "lkpi stub: %s called")`) and returns `0`, `NULL` or `-EOPNOTSUPP`, chosen from the prototype found in the pinned Linux headers.
- Otherwise it's listed as **missing** in `report.md` and the build fails, with a pointer to the header where linuxkpi should implement it.

This makes "how far is driver X from working?" a one-command answer, and it shows over time which missing pieces to promote into linuxkpi proper because several drivers need them.

### 5.6 CMake usage

```cmake
# system/sys/kernel/drivers/net/e1000e/CMakeLists.txt
pyro_linux_driver(e1000e MANIFEST e1000e.ldrv)
```

`pyro_linux_driver()` lives in a new `cmake/PyroLinuxDriver.cmake` next to `Syllable.cmake`. It runs `ldrvgen.py` at configure time with `CMAKE_CONFIGURE_DEPENDS` on the manifest, so edits re-run it, and then calls the existing `syllable_driver()`.

### 5.7 Can the DSL help native drivers too?

Yes, for the same boilerplate. A `class net.ethernet` + `bus pci` manifest **without** a `linux` line could generate the probe loop, `claim_device`, node creation and the `SIOC_ETH_*` ioctl dispatch for a native driver that implements a small `pdk_netdrv_ops`. That would remove the copy-pasted glue from the existing 13 hand-ported NICs too, if Rick wants to touch them. It's optional and not needed for the Linux path.

---

## 6. Kernel prerequisites (do these first)

These are small, native changes. Each one also helps existing drivers.

| # | Change | Where | Why Linux drivers need it | Size |
|---|---|---|---|---|
| 6.1 | **Fix spinlock semantics** (SMP/ACPI audit L1): require IRQs off for recursion, add a release barrier | `include/syllable/kernel/spinlock.h` | Linux `spin_lock` without `_irqsave` is common. Today that's unsafe even on one CPU. | Small |
| 6.2 | **Real DMA allocation:** physically contiguous allocation that returns the physical address via `get_area_physical_address`, plus a `MEMF_DMA32`-style constraint | `kernel/kernel/memory.c`, `areas.c` | `dma_alloc_coherent`/`dma_map_*` are in every driver. Required before x86_64 or more than 2 GB of RAM. | Medium |
| 6.3 | **Kernel module → module linking:** let `load_image_inst` resolve `DT_NEEDED` for kernel images from `/system/drivers/lib/` with ref counts | `kernel/kernel/elf.c:1555-1600` | One shared `liblinuxkpi.so` instead of a copy per driver, and `uses` dependencies (`virtio_pci`, `usbcore`) | Medium |
| 6.4 | **MSI/MSI-X:** allocate vectors above the 16 legacy IRQs, program the capability, and route through the local APIC | `bus/pci`, `kernel/irq.c`, `apic.c`, `irq.h` (`IRQ_COUNT`) | e1000e, NVMe, xHCI and virtio prefer it, and INTx sharing on modern machines is unreliable. It needs the IOAPIC/SMP fixes from the SMP audit (S1 to S6) first. | Medium-large |
| 6.5 | **ID-based driver autoload:** at boot, read `drivers.ids` (section 5.4) and load only the matching drivers for present PCI devices, instead of loading every driver in a `/dev` directory to let it probe | `kernel/devices.c:547`, `kernel/vfs/dev.c` | Many compat drivers mean slow, crash-prone probe-everything behaviour. This is Linux's modalias idea. It can stay optional: drivers keep working via the directory scan. | Small-medium |
| 6.6 | **Physical-page descriptor:** a minimal `struct page`-like lookup (phys → descriptor) | `memory.c` | `alloc_pages`, `page_address` and scatterlists in block and net drivers | Small |
| 6.7 | **PCIe extended config space** (ECAM via ACPI MCFG) | `bus/pci` | Needed by some drivers for extended capabilities (AER, LTR, ASPM, SR-IOV). Most can live without it at first. | Small-medium |
| 6.8 | **Firmware directory:** `/system/firmware/` in the image, plus a kernel read helper | image, `pdk` | `request_firmware` (Wi-Fi, some NICs, GPUs) | Small |

---

## 7. What not to put through the layer

- **Graphics.** Pyro's display drivers are appserver userland plugins (`appserver/appserver/drivers/video/*`) with a kernel claim stub. Linux DRM/KMS assumes TTM/GEM, dma-buf, fences, a large chunk of `drivers/gpu/drm` core and, for real GPUs, a userspace Mesa driver. FreeBSD's `drm-kmod` shows it can be done, but it's the biggest single consumer of LinuxKPI and needs constant maintenance. For Pyro, follow the existing plans (`vmware`, SPICE/QXL, virtio-gpu written natively against the appserver interface). One reasonable exception later: reuse Linux's *register headers and modesetting tables* for a native driver by copying them, not by running DRM.
- **HDA audio.** Native `audio/hda` exists and fits the media server. ALSA's `snd-hda-intel` + codec drivers are tens of thousands of lines. Better approach: port codec quirk *tables* by hand where a machine needs them.
- **Legacy ISA/early-PCI hardware.** The existing hand ports work, and a 2001 driver for a 3c509 isn't worth re-importing.
- **Wi-Fi, for now.** It's possible (FreeBSD runs iwlwifi through LinuxKPI plus an 802.11 bridge), but it needs `mac80211`/`cfg80211` (about 100 kloc), firmware, and a wireless control plane in Pyro's network stack that doesn't exist. Revisit after the net and USB bridges are proven.

---

## 8. Decisions for Rick

1. **USB strategy:** (a) a class-driver bridge onto the existing 2003 stack, or (b) port Linux usbcore + xHCI as a replacement bus manager. My recommendation is (b), after tiers 0 and 1 and the net pilot, because of the USB audit's Critical findings and the missing xHCI.
2. **Linux version to pin:** the newest LTS at the start of implementation. Moving to a new LTS is then a deliberate job every year or two.
3. **Where upstream sources live:** `vendor/linux/<ver>/` in the repo (only the directories used, a few MB, reproducible), or fetched at build time with a SHA256 like `syllable_port`. I'd vendor them, because driver builds shouldn't need the network and the patches need a stable base.
4. **Static first, or fix the loader first (6.3):** I'd do static first for the e1000e pilot, then 6.3 before the second and third drivers.
5. **Whether to migrate the 13 hand-ported NICs to generated native glue (5.7):** optional, low risk, and it removes about 1,300 duplicated lines.

---

## 9. Phased plan

| Phase | Work | Done when |
|---|---|---|
| **0. Prereqs** | 6.1 spinlocks, 6.2 DMA alloc, fix the 4 `linux_compat.h` bugs (§2.4), add `-std=gnu11` support for kernel drivers in `syllable_driver` | Existing drivers still pass `sb run`. The spinlock assert is clean on boot. |
| **1. Tier 0 + core tier 1** | `pdk` (DMA, MMIO, PCI, IRQ, locks, timers, workers, nodes); linuxkpi basics, memory, sync, time, workqueue, device, pci | A trivial Linux PCI driver (for example, `drivers/misc/pci_endpoint_test` or a stripped `8139too`) loads through the generated glue and logs `probe()` |
| **2. Net bridge + DSL** | `lkpi_net`; `ldrvgen.py` (config, glue, ids, stubs, report); `pyro_linux_driver()`; **pilot e1000e** under QEMU (`-device e1000e`) | DHCP and `ping` over e1000e in `sb run`. `report.md` shows zero missing symbols. |
| **3. Loader + virtio** | 6.3 module linking; `liblinuxkpi.so`; `virtio_pci` + `virtio_ring` as a `uses` module; virtio-net | virtio-net works; switch `syllable-vm/run.sh` from rtl8139 to virtio-net (network audit E6) |
| **4. Block bridge** | `lkpi_blk`; virtio-blk, then NVMe | Pyro boots from virtio-blk, or mounts an NVMe volume |
| **5. MSI + autoload** | 6.4 MSI/MSI-X (after the SMP audit fixes), 6.5 `drivers.ids` autoload | e1000e and NVMe run on MSI-X; boot loads only the matching NIC driver |
| **6. Re-port check** | Import current Linux `r8169` through the layer and compare with the 2006-era hand port on real hardware | The new driver handles newer RTL8168/8125 chips the old one lacks |
| **7. USB (if option b)** | usbcore + xHCI + hub + usb-storage + HID under linuxkpi, as a new `usb` bus manager, with `libusb.c`'s node API kept on top | A USB 3 stick mounts on an xHCI-only machine |

**Rough size of new code:** pdk about 1.5 kloc, linuxkpi core about 6 to 9 kloc (much of it copied from Linux `lib/` and headers), lkpi_net about 1.5 kloc, lkpi_blk about 1 kloc, ldrvgen about 800 lines of Python. That's comparable to Haiku's `freebsd_network` compat (around 15 kloc) and far smaller than FreeBSD's LinuxKPI, because of the narrow scope.

---

## 10. Risks

- **API drift.** Linux internal APIs change every release. Pinning one LTS controls this, and `report.md` makes upgrades measurable.
- **Locking model mismatch.** Linux assumes preemptible spinlock sections are short and that `might_sleep()` rules hold. Pyro's semaphores can sleep anywhere. A debug build of linuxkpi should assert "no sleeping while a `pdk_spin` is held", per CPU.
- **Kernel stack size.** Linux drivers assume 8 to 16 KB stacks. Pyro has a 4 KB stack for some kernel threads (SMP/ACPI audit A3). `pdk_worker` and the threaded-IRQ threads must be created with at least 16 KB.
- **32-bit only today.** Linux still builds i386 drivers, so this works now, and the same layer is what makes x86_64 drivers easy later (dma_addr_t becomes 64-bit in one place).
- **GPL boundaries.** Fine for an all-GPL kernel image, but anything that loads a compat driver is GPLv2-only. Keep `pdk` under a licence compatible with that (the kernel's GPLv2 is simplest).

---

## 11. What I didn't check

- I didn't build anything or run a driver. The sizes and kloc estimates come from reading the code and from how large comparable layers are.
- I read `rtl8169`, `linux_compat.h`, the device, PCI, IRQ, area, net and USB headers, `devices.c`, `vfs/dev.c` and `elf.c` closely. The other drivers I surveyed with grep (shim use, version strings, glue shape), not line by line.
- I didn't check whether the existing hand ports actually work on hardware today, or how the media server consumes `audio/` versus `sound/` nodes.
- I wrote the prior-art details from memory of those projects, not from reading their current trees. Check the exact macro and file names (for example, Haiku's `HAIKU_FBSD_DRIVER_GLUE` and Genode's dummy-generation tool) before relying on them.
