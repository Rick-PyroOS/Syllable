# GRUB 2 upgrade and a boot image instead of the blue screen

Evaluation written 10 Nov 2026, updated 11 Nov 2026 with what has been
built since. Sources: the `Rick-PyroOS/Syllable` tree and upstream GRUB 2.12
source (`grub-core/fs/bfs.c`, `afs.c`, `loader/multiboot.c`,
`loader/i386/multiboot_mbi.c`).

## Status (11 Nov 2026)

Most of the plan is done, on the `feat/grub-upgrade` branches of Syllable and
Syllable-Build-System. Section 8 has the original steps with the finished ones
crossed off. Section 9 has what's left and the next steps.

- **GRUB 2.12 boots Pyro.** `cmake/Grub.cmake` builds it from the upstream
  tarball with two AFS fixes (`system/sys/boot/grub2/patches`), and
  `make-image.sh` writes it to the image. The partition now starts at 1 MiB,
  and the image no longer needs the reference `syllable.qcow2`.
- **The menu is graphical.** It has a flame background, a dark panel, and
  an ember-coloured bar on the selected entry (`system/sys/boot/grub2/theme`).
  Entries: Pyro, kernel log on COM1, kernel log on screen, force PCI.
  `custom.cfg` holds local settings.
- **There is no blue screen any more.** The kernel starts in 1024x768x32 and
  draws its own splash with a progress bar (`kernel/bootsplash.c`) until init
  starts. The text console is black, `quiet=true` keeps it empty, and a
  panic always shows the log on screen.
- **The release CD and installer use GRUB 2.** The CD boots GRUB 2's El Torito
  image. The installer writes a small `core.img` that fits before a
  partition at sector 63, plus `grub.cfg` and `custom.cfg`.
- Everything was tested in QEMU. It hasn't been tested on real hardware yet,
  and `sb image` / `sb boot-test` haven't been run on the Mac yet.

## Short answer

- **GRUB 2 is a good fit and a moderate job.** Upstream GRUB 2 already has an
  AtheOS AFS reader (`afs.mod`), and its on-disk structures line up field for
  field with Syllable's `afs.h` (checked below). The Syllable kernel is a plain
  Multiboot 1 ELF kernel, which GRUB 2's `multiboot` command loads. No GRUB
  patches are needed, only a different config and three things in that
  config that differ from GRUB legacy.
- **Effort:** about 2 to 4 days for disk images built by `sb` plus the ISO,
  and another 2 to 3 days for the on-Syllable installer. Porting the GRUB 2
  `grub-install` tools to run on Syllable would add 1 to 2 weeks, and I
  recommend not doing that (section 5).
- **"The blue screen" is two screens.** I assumed you mean either or both:
  1. The GRUB menu, which is blue because `menu.lst` says
     `color cyan/blue white/blue`.
  2. The kernel's boot console, which is the screen you look at longest.
     `init_kernel()` paints it on purpose: `dbcon_set_color(0, 0, 0, 170)`
     sets palette entry 0 to blue and `dbcon_clear()` fills the text screen
     with attribute `0x17`, white on blue
     (`system/sys/kernel/kernel/init.c:765`, `dbconsole.c:86`).
  GRUB 2 fixes the first with a background image or theme. The second needs a
  small kernel change (section 6). The recommended order is a GRUB 2
  background image first, then let that image stay up while the kernel boots,
  then add a kernel-drawn splash with a progress bar if you want one.

---

## 1. What Syllable boots with today

| Item | Where | Notes |
|---|---|---|
| GRUB source in the tree | `system/sys/boot/grub` | Actually **GRUB 0.92** (`configure.in`: `AM_INIT_AUTOMAKE(grub, 0.92)`), plus `stage2/fsys_afs.c` and `afs.h` |
| GRUB the release build used | Builder port `grub-0.97` (+ `grub-0.97-system`) | `auto-build/build.sh` switches to GCC 3.4.3 just for it (see build-from-scratch-plan §1) |
| Older ports | Builder `grub-0.94`, `grub-0.95`, `grub-1.96` | `grub-1.96` carries "Bean"'s `fs/afs.c`, the ancestor of upstream GRUB 2's afs module |
| Boot sectors in `sb` images | `cmake/scripts/make-image.sh` | Copies the first 63 sectors (MBR + `afs_stage1_5`) and `stage1`, `stage2`, `afs_stage1_5`, `stage2_eltorito` out of a reference `syllable.qcow2`; partition 1 starts at sector 63 (`offset=32256`) |
| Menu | `/boot/grub/menu.lst` | The installer writes it from `system/build-tools/scripts/install/templates/grubgen/top`: `color cyan/blue white/blue`, `timeout 10` |
| Installer | `system/build-tools/scripts/install/do_install.rb` | Runs Syllable's own legacy `grub-install --root-directory=/inst` to the MBR or the partition. It tells users that another system's GRUB can't boot Syllable. That stops being true with GRUB 2 |

A Syllable menu entry today:

```
title   Start Syllable
root    (hd0,0)
kernel  /system/kernel.so root=/dev/disk/ata/hda/0
module  /system/config/kernel.cfg
module  /system/drivers/dev/bus/acpi
module  /system/drivers/dev/bus/pci
module  /system/drivers/dev/bus/ata
module  /system/drivers/dev/hcd/ata_pci
module  /system/drivers/fs/afs
```

## 2. How the kernel is loaded, and what changes under GRUB 2

The kernel side (`system/sys/kernel/kernel/intel.s`, `init.c`):

- Its Multiboot 1 header is `0x1BADB002`, with flags `0x0003` (page-align modules,
  give memory info). There is no a.out kludge and no video request.
  `kernel.so` is an ELF linked at `0x100000`. `_start` halts unless
  `eax == 0x2BADB002`.
- `init_kernel_mb()` copies the info structure, the command line and each
  module's command line into kernel memory, and `init_memory_pools()` reads
  the Multiboot memory map later.
- **Boot modules are identified by their command line, not by order.**
  `init_boot_modules()` (`elf.c:2618`) does `strstr(args, "/drivers/")` and
  builds `/boot/system/drivers/...` from it. `config.c:233` does the same with
  `"/config/"`. A module whose command line lacks the path is silently
  skipped.

What GRUB 2 does differently. These are verified in the 2.12 source.

| # | Difference | Effect on Syllable | Fix in grub.cfg |
|---|---|---|---|
| 1 | `module FILE ARGS` passes only `ARGS` as the module's command line (`multiboot.c:424`, `add_module(argc-1, argv+1)`). GRUB legacy passed `FILE ARGS` | Every driver and `kernel.cfg` would be skipped, and the boot would fail with no boot FS | Repeat the path: `module /system/drivers/fs/afs /system/drivers/fs/afs` |
| 2 | `multiboot FILE ARGS` passes only `ARGS` as the kernel command line (`multiboot.c:332`) | Works, since the kernel parses `key=value` pairs. `sysinfo` would just not show `/system/kernel.so` first, and the installer's `grub-gen.rb` strips it anyway | Optional: `multiboot /system/kernel.so /system/kernel.so root=...` to match legacy exactly |
| 3 | Modules go anywhere above 1 MiB, possibly near the top of 4 GiB (`lowest_addr = 0x100000`, `PREFERENCE_NONE`) | The kernel caps RAM at 1.5 GiB (`init.c` "HACK: Limit memory size"). In a VM with more than 1.5 GiB, a module placed above the cap would sit outside memory the kernel manages | `multiboot --quirk-modules-after-kernel ...` makes GRUB put modules just after the kernel, as legacy did |
| 4 | The info block, command lines and memory map go in `0x10000`–`0xA0000` (`multiboot_mbi.c:470`) | That is also the kernel's real-mode memory pool (`init_kernel((char*)0x10000, ...)`, `init_memory_pools`). The kernel copies everything except the memory map before it uses that pool, so it is probably fine. GRUB legacy also kept these in low memory | Test it. If it's a problem, copy the memory map in `init_kernel_mb()` (a few lines) |
| 5 | When the kernel doesn't ask for video, GRUB 2 switches back to VGA text before jumping (`multiboot.c:124`) | Good for today's text console. It also means a GRUB background image disappears at boot unless the kernel asks to keep graphics (section 6) | None needed |

A GRUB 2 entry equivalent to today's:

```
menuentry "Pyro" {
    search --no-floppy --file --set=root /system/kernel.so
    multiboot --quirk-modules-after-kernel /system/kernel.so /system/kernel.so root=/dev/disk/ata/hda/0
    module /system/config/kernel.cfg        /system/config/kernel.cfg
    module /system/drivers/dev/bus/acpi     /system/drivers/dev/bus/acpi
    module /system/drivers/dev/bus/pci      /system/drivers/dev/bus/pci
    module /system/drivers/dev/bus/ata      /system/drivers/dev/bus/ata
    module /system/drivers/dev/hcd/ata_pci  /system/drivers/dev/hcd/ata_pci
    module /system/drivers/fs/afs           /system/drivers/fs/afs
}
```

Driver arguments still work: anything after the repeated path is parsed as
before.

## 3. AFS support in GRUB 2

Upstream `grub-core/fs/afs.c` is just `#define MODE_AFS 1` + `#include
"bfs.c"`. It was added around 1.96/1.97 from Bean's code (the same code as
the Builder `grub-1.96` port) and rewritten on top of the BFS reader for 2.00.
It reads files, directories (B+tree), in-inode and long symlinks, and labels.
It can't write, and has no UUID, which we don't need.

Layout check against Syllable's `afs.h` (offsets in bytes, 32-bit build):

| Structure | Field | Syllable | GRUB `MODE_AFS` | Match |
|---|---|---|---|---|
| Superblock | location | byte 1024 of the partition | `SUPERBLOCK 2` (sector 2) | ✓ |
| | magic1/2/3 | `AFS1` 0x41465331 @32, 0xdd121031 @68, 0x15b6830e @112 | same values @32, @68, @112 | ✓ |
| | block size / shift | @40 / @44 | `bsize` @40, `log2_bsize` @44 | ✓ |
| | `as_nAllocGrpShift` | @76, "bits to shift a group number to get a byte address" | `log2_ag_size` @76, used as a byte shift | ✓ |
| | root dir run | @116 | `root_dir` @116 | ✓ |
| Block run | | `int32 group; uint16 start, len` | `ag, start, len` | ✓ |
| Inode | mode / flags | @20 / @24 | @20 / @24 | ✓ |
| | mtime | @40 | @40 (AFS has 12 bytes padding) | ✓ |
| | attr dir | @56 | @56 | ✓ |
| | data stream | @76 (after the 4-byte `ai_psVNode` pointer) | @76 | ✓ |
| B+tree header | root | @4 (int64) | @4 | ✓ |
| | depth | @12 | `level` @12 | ✓ (`node_size` is declared but never used) |
| B+tree node | key count / key size | `int` @24 / @28 | `uint32` @24 / @28 | ✓ |
| Keys | compare | signed `char` | `grub_int8_t` under `MODE_AFS` | ✓ |

On paper this is a match, which isn't proof. Two cheap ways to prove it:

- **`grub-fstest` (a host tool that ships with GRUB 2)** reads a disk image
  with GRUB's own driver on Linux. `grub-fstest syllable.img ls
  '(hd0,msdos1)/system/drivers/dev'` and `grub-fstest syllable.img cmp
  '(hd0,msdos1)/system/kernel.so' stage/system/kernel.so` would make a good
  `sb boot-test`-style check over every file under `/system` and `/boot`,
  compared against afstool.
- In QEMU, the GRUB shell commands `ls` and `hashsum --hash md5`.

The risk is in less common shapes: files big enough for indirect or
double-indirect runs, and directories with multi-level B+trees. The `cmp`
sweep covers both. A side benefit is that a Linux user's own GRUB 2
(`insmod afs`) can boot Syllable. The installer currently says that's
impossible.

## 4. Building GRUB 2 for `sb`

- **Which GRUB:** latest upstream stable, 2.12 or newer, platform
  **i386-pc (BIOS) only**. UEFI doesn't help, because the kernel and drivers
  depend on the BIOS (V86 `realint` for VESA, `disk/bios`). Syllable on a UEFI
  machine still needs CSM.
- **How to build:** either `grub-pc-bin` plus `grub2-common` from the
  toolchain image's distro (the image is linux/amd64, so the packages exist),
  or a pinned upstream tarball built with
  `./configure --target=i386 --with-platform=pc` and the host GCC. Both need no
  GCC 3, which removes the last reason the old build swapped compilers. I
  recommend the pinned tarball for reproducible images, and the distro
  package to prototype.
- **Boot sectors without the reference disk:** `make-image.sh` stops copying
  63 sectors from `syllable.qcow2`. Instead:
  1. Write a fresh MBR partition table. **Start partition 1 at sector 2048
     (1 MiB)** instead of 63. A 62-sector gap (31 KiB) only just fits a core
     image with `biosdisk part_msdos afs`. 1 MiB fits everything, including
     `normal`, `multiboot`, `gfxterm` and `png`, so GRUB doesn't depend on
     loading `.mod` files from AFS at all. This means changing `offset=32256`
     to `1048576` (`AFSTOOL_OFFSET`, `sb run`/`boot-test`, the linux-afs test
     scripts). The kernel reads the partition table, so it doesn't care.
  2. `grub-mkimage -O i386-pc -p '/boot/grub' -c early.cfg biosdisk
     part_msdos afs search_fs_file normal multiboot ...`, where `early.cfg` is
     `search --file --set=root /boot/grub/grub.cfg` plus
     `set prefix=($root)/boot/grub`. The same `core.img` then works on any
     disk and partition.
  3. Write `boot.img` (446 bytes, keeping the partition table) and `core.img`
     at sector 1, either with `grub-bios-setup` against a device map that
     points at the image file, or with a small script (`dd` plus patching the
     boot.img LBA and the diskboot blocklist). The script is what the
     installer needs too (section 5).
  4. `/boot/grub/grub.cfg`, `fonts/`, the background or theme and any extra
     `.mod`s become ordinary files in the stage tree (CMake `install(FILES)`),
     replacing `menu.lst` and the four stage files in `make-image.sh`'s `grub=`
     list. As a bonus, image builds no longer need `syllable.qcow2`.
- **ISO (M4 in the build plan):** use `grub-mkrescue` (xorriso) in place of
  `stage2_eltorito`. It gives El Torito plus a hybrid USB image. The menu
  entry uses `rootfs=iso9660` and the CD drivers, as the live CD does today.
- **Menu generation:** a template for `grub.cfg` in the tree, with
  entries for normal, text and rescue boot, as planned in
  `init-server-design-2026-11-08.md` §2.4.

## 5. The installer on Syllable

`do_install.rb` currently runs the legacy Syllable `grub-install`. There are
two ways to do this with GRUB 2:

- **A. Port the GRUB 2 userland (`grub-install`, `grub-probe`,
  `grub-bios-setup`) to Syllable.** Needs a new `grub-core/osdep` backend for
  Syllable device names (`/dev/disk/ata/hda/raw`, `/0`), disk geometry and
  partition offsets. That is 1 to 2 weeks and carries an ongoing patch set.
- **B. Ship the prebuilt `boot.img` and `core.img` (from section 4, which
  find their own partition through `search`) and have the installer write
  them** (recommended). It needs a small helper (Ruby or C, about 100 lines) to
  write boot.img's code area without touching the partition table, write
  core.img to sectors 1..N, and patch the two sector pointers.
  `grub-gen.rb` is rewritten to emit `grub.cfg` in the format from section 2.
  2 to 3 days with testing.

What gets dropped: the "install to partition" (`p`) option. GRUB 2 only does
that with blocklists, which it warns against. Users who chainload from
another boot manager can use that system's GRUB 2 with `insmod afs` instead.
Existing installs keep GRUB legacy until they reinstall the MBR, and nothing
forces that.

## 6. Showing a boot image instead of the blue screen

### 6a. The GRUB menu (needs GRUB 2)

GRUB legacy can only do 16-colour text, apart from distro "splashimage"
patches that Syllable never had. In GRUB 2:

```
insmod all_video
insmod gfxterm
insmod png
loadfont /boot/grub/fonts/unicode.pf2   # gfxterm shows nothing without a font
set gfxmode=1024x768,800x600,auto
terminal_output gfxterm
background_image -m stretch /boot/grub/pyro-boot.png
set menu_color_normal=white/black       # "black" background = transparent over the image
set menu_color_highlight=black/light-gray
set timeout=5
```

- **Image:** PNG (8-bit RGB or RGBA, not interlaced), JPEG or TGA. Make it
  the size of `gfxmode` so it isn't scaled.
- **Full theme instead:** `set theme=/boot/grub/themes/pyro/theme.txt` with a
  `desktop-image`, a styled `boot_menu`, and a countdown `progress_bar`. Fonts
  are made with `grub-mkfont` on the host.
- **No menu at all:** `set timeout_style=hidden` with `timeout=2`. GRUB shows
  only the image, and Esc or Shift opens the menu. This is the closest to
  "a picture instead of a menu".

### 6b. Keeping the image up while the kernel boots

After GRUB, the kernel switches to its own 80x25 white-on-blue console, and
that screen stays until the appserver sets a graphics mode. There are three
levels of fix:

1. **Not blue, and quiet (about 1 hour, works with legacy GRUB too).**
   Change `dbcon_set_color(0,0,0,170)` to black and the `0x1720` fill to
   `0x0720`, and add a `quiet=true` kernel argument that stops `dbcon_write`
   from writing to the screen. `printk` still goes to the serial port and the
   kernel log, as it does now. This gives a black screen, or one centered
   "Pyro" line, but no image.
2. **Keep GRUB's image on screen (about half a day, needs GRUB 2).** Set
   Multiboot header flag bit 2 (video). That means adding the five address
   words, then `mode_type=0` and the width, height and depth, for example
   1024x768x32. GRUB then hands over in that graphics mode instead of
   switching to text (`multiboot_mbi.c:198`). If the kernel then doesn't touch
   the screen (skip `dbcon_clear` and `dbcon_write` when the info block reports
   a framebuffer), the last thing GRUB drew, the background image, stays up
   until the appserver changes mode. I haven't confirmed whether GRUB redraws or
   clears the screen as it boots an entry. With gfxterm a clear repaints the
   background, so the image should stay up, perhaps with one "Booting" line.
   Needs a QEMU test.
3. **A real kernel splash with progress (about 2 to 4 days).** Same header
   change. In addition, the kernel reads the framebuffer address, pitch and
   format from the Multiboot info (flags bits 11 and 12), maps it with a
   physical-address area, and draws a splash image. The image is loaded as a
   boot module (`module /system/boot/splash.img /system/boot/splash.img`), so
   new artwork doesn't need a kernel rebuild. It's converted to raw or RLE BGRA
   at build time, because a PNG decoder in the kernel isn't worth it. A
   progress bar advances in `init_boot_modules()` and driver init. Init could
   push it further through a small `/dev` node, which ties in with the init
   server design. Needed with it:
   - Text fallback when there's no framebuffer, for GRUB legacy or
     `splash=false`.
   - A panic path that draws text into the framebuffer with a built-in 8x16
     font, or switches back to text. Otherwise a panic shows as a frozen
     splash.
   - Make sure the boot-test marker still goes to serial.

   The appserver's VESA driver sets its own mode through the BIOS anyway, so
   the handoff needs no change. One option: have the appserver or the login
   window draw the same image first, so there's no black flash between the
   splash and the desktop.

**Recommendation:** do 6a with the GRUB 2 upgrade, add 6b-1 now (it's
nearly free and helps under legacy GRUB too), and then do 6b-2. Go to 6b-3
only if you want a progress bar. 6b-2 already gives one continuous image from
power-on to the desktop.

## 7. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| AFS reader misreads some Syllable volumes (big or fragmented files, deep directories) | Low (layouts match) | `grub-fstest cmp` sweep over the whole image in CI; test real-hardware installs made by old Syllable releases |
| Modules lost because of the command line change | Certain if missed | Repeat the path in every `module` line; generate grub.cfg from one template |
| Modules above the 1.5 GiB RAM cap | Medium on VMs with >1.5 GiB | `--quirk-modules-after-kernel` |
| GRUB's low-memory info block vs the kernel's real-mode pool | Low | QEMU test with 512 MiB, 2 GiB and 4 GiB; copy the memory map early if needed |
| core.img too big for a 63-sector gap | High with graphics modules | Partition at 1 MiB for new images; the installer checks the gap on existing disks and falls back to a minimal core.img that loads `.mod`s from `/boot/grub/i386-pc` |
| Installer regressions on real hardware (disk numbering, SATA/USB mappings in `grub-gen.rb`) | Medium | `search --file` in place of hard-coded `(hdN,M)`, which removes most of that mapping logic |
| Licence moves from GPLv2 (legacy) to GPLv3 (GRUB 2) | — | Fine for a bootloader you ship; ship or link the GRUB source |
| UEFI-only machines | — | Out of scope: the kernel needs BIOS services |

## 8. Suggested order

Crossed off: done on `feat/grub-upgrade` (11 Nov). The notes say how it was
done where that differs from the plan.

1. ~~Build GRUB 2 i386-pc in the toolchain image, and run `grub-fstest` against
   today's `syllable.img`.~~ All files matched, on a synthetic image, the 0.6.7
   tree and the 8 Nov build. It found two GRUB bugs, now patched (see "Step 1
   result" below).
2. ~~Rework `make-image.sh` (1 MiB partition, boot.img/core.img, `grub.cfg`)~~,
   and boot with `sb run` and `sb boot-test` ~~at three RAM sizes~~. The
   image boots to the login screen in QEMU at 512 MiB and 1 GiB. Still to do:
   run `sb boot-test` on the Mac, and test 2 GiB and 4 GiB.
3. ~~Background image or theme (6a), plus the black, quiet console (6b-1).~~
   A full theme, with bold fonts for the countdown and help line.
4. ~~Multiboot video flag and keeping the image up (6b-2).~~ GRUB's
   `multiboot` command sets the mode from the header, so `gfxpayload` in
   grub.cfg has no effect.
5. ~~ISO~~ ~~with `grub-mkrescue`~~. Done with `grub-mkimage -O
   i386-pc-eltorito` and xorriso in `release/make-release.sh` instead, so the
   CD layout is unchanged. There is no hybrid USB image yet (section 9).
6. ~~Installer helper and the `grub.cfg` generator (section 5, B)~~, then drop
   the GRUB 0.92 tree and the GCC 3 step. The release installer
   (`release/cd/Install/install.sh` in Syllable-Build-System) writes
   prebuilt `boot.bin` and `core.bin` with `dd`. No separate helper was
   needed. Dropping the old GRUB tree is still to do.
7. ~~Optional: kernel splash with progress (6b-3).~~ `kernel/bootsplash.c`,
   with the splash as a boot module (`/boot/grub/splash.img`, made by
   `system/sys/boot/splash/make-splash.py`), `splash=false` for the log on
   screen, and a panic screen.

## 9. Next steps

In rough order:

1. **Land it.** Commit `feat/grub-upgrade` in both repos and open the pull
   requests. Put the zip fix in `cmake/scripts/ensure-source.sh` (ports
   delivered as `.zip`, such as librsvg) on its own `fix/` branch.
2. **Test on the Mac and on hardware.** Run `sb image` and `sb boot-test`,
   `sb run` at 2 GiB and 4 GiB (risk table: the low-memory info block), a
   full `sb release` with the installer run end to end in QEMU, and at least
   one real BIOS machine (or UEFI with CSM). The installer window didn't come
   up in the CD test here, but that test used a dev tree, not a release
   build, so a real `sb release` is needed to know.
3. **Send the two AFS patches upstream** to grub-devel: double-indirect
   reads, and the uninitialized free in `find_file()`.
4. **Show init's servers on the progress bar.** Today the bar fills when init
   starts, so the appserver, input server, registrar and the rest start
   behind a full bar.
   - a. Init reports, the kernel draws (about a day). Add a small device such
     as `/dev/misc/bootsplash`. Init writes lines to it such as
     `step 3/7 Starting input server` or `fail Input server`. The kernel moves
     the bar and writes the line under it with its 8x16 font. The servers
     themselves don't change.
   - b. Split the bar. The kernel's own work (modules, root mount, drivers)
     takes the first half. Each server in init's start list takes an equal
     share of the rest, so adding a server adds a step. This fits the server
     list in `init-server-design-2026-11-08.md`.
   - c. Hand the screen over later. The kernel keeps the splash up until the
     appserver opens its graphics driver, not until init starts. Servers
     started before the appserver then show on the splash. For those started
     after it, the appserver or the login window draws the same picture and
     bar and continues. This is the bigger part, because it changes the
     appserver.
5. **Close the black gaps.** There's about half a second of black while GRUB
   loads the kernel and modules, and about a second while the appserver
   switches modes before the login screen. 4c fixes the second.
6. **Splash at other resolutions.** At a mode other than 1024x768, the picture
   is centred, or cropped if the screen is smaller. A second picture for
   800x600, or scaling in `make-splash.py`, would cover that.
7. **Clean up the old boot loader.** Remove `system/sys/boot/grub` (GRUB 0.92),
   the Builder `grub-0.9x`/`grub-1.96` ports, the GCC 3.4.3 step in
   `auto-build/build.sh`, and the old Ruby installer's `grub-gen.rb` and
   `do_install.rb` GRUB code (`system/build-tools/scripts/install`).
8. **Optional:**
   - A hybrid USB image of the release (`grub-mkrescue`, or isohybrid's MBR).
   - `timeout_style=hidden` for a picture-only boot, with Esc or Shift to
     open the menu.

---

## Step 1 result (10 Nov, later)

Done. The test scripts (`sweep.sh`, `mktree.py`, `mbr.py`) are kept with the
project notes, not in this repo.

- GRUB 2's AFS reader read every file exactly as Syllable's own AFS code
  wrote it. That covers a synthetic stress image (3,038 files) and the real
  Syllable Desktop 0.6.7 base tree (10,773 files plus symlinks) at today's
  sector-63 layout.
- **GRUB bug:** reads past the indirect range fail because of a missing
  block-to-byte shift in `read_bfs_file()`. There is a one-line patch in
  `system/sys/boot/grub2/patches/0001-fs-afs-fix-double-indirect-reads.patch`. Booting is not
  affected (kernel and drivers never reach double-indirect), but we should
  build GRUB with the patch and send it upstream. That moves section 4 from
  "distro package to prototype" to "pinned source build plus our patch".
- The 0.6.7 release confirms the shipped bootloader is GNU GRUB 0.97.
- Rick's own `syllable.img` (8 Nov) has been tested on the Mac: all 13,184
  files match `syllable.manifest`.
- **Second GRUB bug, and this one matters for booting:** any failed lookup on
  AFS or BFS (a missing file, or a symlink to `/boot/...`) frees an
  uninitialized pointer. GRUB 2.06 aborts on Rick's image. The fix is
  `system/sys/boot/grub2/patches/0002-fs-bfs-dont-free-uninitialized-node.patch`. With it, a
  patched GRUB is required, not optional, because `search --file` and
  missing-file checks in grub.cfg would otherwise risk a crash at boot.
