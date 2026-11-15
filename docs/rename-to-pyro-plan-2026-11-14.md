# Renaming Syllable to Pyro: inventory and plan (14 Nov 2026)

This is a plan only. Nothing has been renamed. Counts come from `git grep -i syllable` over the tracked files in each checkout under `~/Code/Rick-PyroOS` on 14 Nov 2026 (Syllable on `main`, Syllable-Build-System on `feat/run-clear-print-qemu`, Syllable-Ports on `port/git-2.47`).

## 1. Summary

- The main **Syllable** repo has 71,440 case-insensitive matches. 61,200 of them are false positives: Unicode character names such as `HANGUL SYLLABLE GA` in the dead in-tree glibc 2.3.2 locale data. That leaves **about 11,400 real occurrences in about 2,700 files**, plus **2,715 tracked paths** with "syllable" in the name.
- **About 3,600 of those 11,400 (and 1,650 of the 2,715 paths) are in `system/apps/utils/Builder/packages`**, the old Builder recipes. Only 6 of those packages are still used: the toolchain ports (GCC 4.1.2, GCC 9.5.0, binutils 2.17, binutils 2.35, glibc 2.7). Everything else there is historical.
- The changes that matter are few but deep: the **GCC target triplet `i586-pc-syllable`**, the **`__SYLLABLE__` macro**, the **`<syllable/...>` header directory**, **`libsyllable.so.8`**, the **`elf_i386_syllable` linker emulation**, glibc's **`sysdeps/unix/sysv/syllable`**, the kernel's **`uname -s` = `syllable`**, and the build system's **`syllable_*()` CMake functions, `SYLLABLE_*` variables and Docker names**.
- Some of this is already Pyro: the GRUB menu (`grub.cfg`), the AFS volume name in `make-image.sh`, the boot splash, the website and the `.nvim` README title.
- **Recommendation:** rename in six phases (section 4). Do everything visible to users and developers first, while the toolchain still says `syllable`. Change the triplet, glibc and `uname` last, in one toolchain rebuild, with old names kept as aliases. The AtheOS precedent already in the tree (`atheos/` headers kept as "a synonym for syllable, for backwards compatibility only", and `__ATHEOS__` still predefined by GCC) is the model: **new name canonical, old names kept as aliases indefinitely.**
- **Don't rename:** copyright and credit lines ("The Syllable Team", "Kurt Skauen", "The Knights of Syllable"), dated docs, the legacy Builder recipes, and the old WebKit port in Webster. Section 5 lists them.

## 2. Naming scheme

| Thing | Today | Proposed | Old name kept as |
|---|---|---|---|
| Product name in prose and UI | Syllable / "Syllable (Pyro)" | Pyro | (history only) |
| GCC/binutils triplet | `i586-pc-syllable` (`x86_64-pc-syllable` in planning docs) | `i586-pc-pyro` (`x86_64-pc-pyro` later) | `i586-pc-syllable-*` symlinks to the tools; config.gcc/config.bfd match both |
| Predefined macros | `__SYLLABLE__`, `__syllable__`, `__ATHEOS__`, assert `system=syllable` | add `__PYRO__`, `__pyro__`, assert `system=pyro` | keep `__SYLLABLE__`, `__syllable__`, `__ATHEOS__` defined permanently |
| Linker emulation | `elf_i386_syllable` | `elf_i386_pyro` | `elf_i386_syllable` as an extra emulation for a release or two |
| GCC thread model | `--enable-threads=syllable`, `gthr-syllable.h` | `pyro`, `gthr-pyro.h` | none needed (internal to the toolchain build) |
| glibc OS add-on | `--enable-add-ons=syllable`, `sysdeps/unix/sysv/syllable` | `pyro` | none needed (internal) |
| `uname -s` (kernel `g_pzSystem`) | `syllable` | `pyro` | none possible: config.guess and port scripts must learn `pyro` first |
| Public header dir | `<syllable/...>` (and `<atheos/...>`) | `<pyro/...>` | `syllable/` and `atheos/` as one-line forwarding headers |
| GUI library | `libsyllable.so.8`, `-lsyllable` | `libpyro.so.N` at the next real ABI break | `libsyllable.so.8` kept for old binaries; `libsyllable.so` dev link |
| CMake platform and DSL | `CMAKE_SYSTEM_NAME Syllable`, `syllable_program()` and 13 more functions, 43 `SYLLABLE_*` variables | `Pyro`, `pyro_program()`, `PYRO_*` | wrapper functions and variable fallbacks |
| sb and Docker | `syllable-os-x86`, `syllable-build`, `/opt/syllable`, 23 `SYLLABLE_*` env vars, `out/syllable-VERSION.qcow2` | `pyro-*`, `/opt/pyro`, `PYRO_*`, `out/pyro-VERSION.qcow2` | old env vars read as fallbacks; image tags aliased |
| Repos and checkout folders | `Syllable`, `Syllable-Build-System`, `Syllable-Ports` | `Pyro`, `Pyro-Build-System`, `Pyro-Ports` (or merge into the existing PyroOS/Pyro later) | GitHub redirects; sb accepts either folder name |

**Decision needed before Phase 4:** the triplet. Upstream GNU `config.sub` and `config.guess` know `syllable` (binutils 2.35's copies, lines 1438 and 1128) but not `pyro`. A new triplet means every autoconf port needs a patched `config.sub`/`config.guess`. `cmake/scripts/apply-port.sh` already swaps in automake 1.15's copies when a port's are too old, so the fix is one patched pair shipped with the toolchain, plus a patch sent to config-patches@gnu.org. The alternative is to keep `i586-pc-syllable` as an internal-only name forever, as many OSes keep legacy triplets. My recommendation is to switch, because the triplet shows up in every path under `/system/development` and in every native compile, and the cost is one-time.

## 3. Inventory

### 3.1 Main Syllable repo, by area

Hits are real occurrences (Unicode false positives dropped). "Paths" counts tracked files whose path contains "syllable". The last three columns split the hits by what the line looks like: code identifiers or paths, prose and strings, and copyright or credit lines (a rough heuristic).

| Area | Hits | Files | Paths | Ident/path | Prose/string | Credit | Risk |
|---|---:|---:|---:|---:|---:|---:|---|
| Toolchain ports: GCC 4.1.2 + 9.5.0, binutils 2.17 + 2.35, glibc 2.7 (`Builder/packages/Syllable/...`) | 423 | 124 | 278 | 245 | 178 | 0 | **High**: triplet, ABI, full toolchain rebuild |
| Kernel + drivers (`system/sys/kernel`) | 382 | 254 | 1 | 179 | 192 | 11 | Medium: `uname`, boot banner, `syllable_driver()` |
| System headers (`system/sys/include/syllable`) | 521 | 188 | 321 | 318 | 83 | 120 | **High**: include paths for every program and port |
| appserver + libsyllable | 624 | 208 | 197 | 289 | 167 | 166 | **High**: `libsyllable.so.8` soname, `-lsyllable` |
| Media, translators, boot | 118 | 65 | 0 | 89 | 14 | 1 | Low |
| In-tree libs that are built (libpthread, libdl, codeview, libkcrypto) | 46 | 23 | 0 | 24 | 22 | 0 | Low |
| Dead in-tree libs (glibc-2.3.2, glibc, libpthread-1, compatibility) | 175 | 84 | 88 | 60 | 115 | 0 | None: not built |
| Webster (old WebKit port: `platform/syllable`, `*Syllable.cpp`) | 937 | 164 | 83 | 769 | 157 | 10 | Medium; best done with the WebKit upgrade |
| GNU apps in tree (mostly generated `config.status`) | 245 | 43 | 0 | 236 | 9 | 0 | Low |
| Other apps (Dock, Sourcery, aterm, AttribEdit, slbmgr...) | 1,156 | 552 | 5 | 674 | 362 | 114 | Low-medium: About boxes, app signatures |
| Keymaps, fonts, icons (`system/other`) | 173 | 169 | 2 | 1 | 170 | 0 | Low: one header line per keymap; `SyllableConsole-Regu.ttf` |
| Image tree + on-disk docs (`system/image`, `system/doc`) | 1,495 | 54 | 53 | 163 | 1,048 | 31 | Low: hostname, profile, bookmarks to the dead syllable.org |
| Old build-tools (`system/build-tools`) | 152 | 22 | 0 | 61 | 84 | 0 | Low |
| CMake build + host tools (`CMakeLists.txt`, `cmake/`, `tools/`) | 819 | 48 | 8 | 665 | 154 | 0 | Medium: every component's CMakeLists calls `syllable_*()` |
| Editor/IDE (`.nvim`, `.nvim.lua`, `.idea`, `.raycast`, `.clang*`, `.syllable-box*`) | 156 | 27 | 3 | 83 | 72 | 1 | Low |
| `docs/` (24 docs) | 635 | 24 | 0 | 392 | 242 | 1 | Low; dated docs stay as written |
| Legacy Builder recipes (rest of `Builder/packages`) | 3,186 | 625 | 1,648 | 1,630 | 1,489 | 5 | None if left alone |
| Builder profiles, resources, scripts | 181 | 38 | 28 | 37 | 141 | 0 | Low: `Layout.cmake` reads the profile |
| **Total** | **~11,430** | | **2,715** | | | | |

The most frequent tokens (real hits): `Syllable` 6,724; `syllable` 4,840; `libsyllable` 780; `__SYLLABLE__` 644 (119 outside Builder); `FrameLoaderClientSyllable` and the other WebKit platform classes ~800 together; `-lsyllable` 174; `SYLLABLE_STAGING` 168; `syllable_program` 78, `syllable_driver` 71, `syllable_port` 79 and the other CMake DSL functions ~500 together; triplet strings 260 (`i586-pc-syllable` 179 plus tool names).

### 3.2 The kernel

- `system/sys/kernel/kernel/version.c:32`: `const char *g_pzSystem = "syllable";` This is what `uname -s` and `get_system_info()`'s `zKernelSystem` return. It is **ABI-visible**: `config.guess` on a running system matches `i*86:syllable:*:*`, and `cmake/ports/50-git.cmake` passes `uname_S=Syllable`.
- `kernel/init.c:762`: boot banner `"Syllable %s build %s starting..."`. `boot-test.sh` and `sb` may match the serial log on it, so check them in the same change.
- `drivers/hcd/usb_ehci/hcd.c:337`: USB root hub manufacturer string `"Syllable 0.6"`.
- `drivers/bus/acpi/include/acpi/platform/acsyllable.h`: the ACPICA OS layer (`__ACSYLLABLE_H__`, selected in `acenv.h` under `__SYLLABLE__`).
- The NTFS driver's `ntfs_ntutc2syllable()`/`ntfs_syllable2ntutc()`, the AGP guards `_SYLLABLE_AGPGART_*_H_`, and `SYLLABLE_KERNEL_DEBUG`: cosmetic internal names.
- 66 `syllable_driver()` calls in driver CMakeLists.
- 76 of the 382 hits are "the Syllable kernel" license and header text.
- Kernel code includes `<syllable/...>` through a CMake hack: `cmake/Syllable.cmake:82` makes `build/kernel-headers/syllable` a symlink to the **atheos/** headers, because the kernel was written when `syllable/` was a copy of `atheos/` (section 3.8).

### 3.3 GCC, binutils, glibc (the toolchain)

The toolchain image (`Syllable-Build-System/toolchain`) builds from `Syllable/system/apps/utils/Builder/packages`. The ports there that are live:

- **GCC 9.5.0** (`Syllable/gcc-9.5.0/gcc-9.5.0.patch`, 96 hits, mirrored in `Syllable-Ports/gcc-9.5.0`, 62 hits): new files `gcc/config/i386/syllable.h`, `syllable.opt`, `libgcc/config/gthr-syllable.h`, `libgcc/config/i386/t-syllable`. Patched `config/gthr.m4`, `gcc/config.gcc` (`i[34567]86-*-syllable*`), `gcc/configure{,.ac}`, `libgcc/config.host`, `libgcc/configure`, `libstdc++-v3/configure` and `crossconfig.m4`, `libtool.m4` (`dynamic_linker='Syllable kernel loader'`). `TARGET_OS_CPP_BUILTINS` defines `__ATHEOS__`, `__SYLLABLE__`, `__syllable__` and asserts `system=syllable`. The link spec passes `-m elf_i386_syllable`. The recipe installs into `framework/libraries/gcc/i586-pc-syllable/9.5.0`.
- **GCC 4.1.2** (`Syllable/gcc-4.1.2`, 30 hits, plus `GNU-CompilerCollection-4.1.2*`): `config/mt-syllable`, `gcc/config/i386/{syllable.h,t-syllable,x-syllable}`, `t-slibgcc-syllable`, `gthr-syllable.h`. **This is still the default compiler** (`SYLLABLE_COMPILER=4.1.2`), so it has to be renamed too, not just 9.5.0.
- **binutils 2.35** (24 hits) and **binutils 2.17** (16 hits): `bfd/config.bfd`, `gas/configure.tgt`, `ld/configure.tgt` (`targ_emul=elf_i386_syllable`), `ld/Makefile.{am,in}`, and `ld/emulparams/elf_i386_syllable.sh`.
- **glibc 2.7** (236 hits in 258 files, plus `glibc-2.7-system`): `patches/syllable/` add-on with `sysdeps/unix/sysv/syllable/**`, built with `--enable-add-ons=syllable`; `configure` maps the host OS to the add-on. Installed under `/system/resources/glibc/2.7`, which has no "syllable" in it, and libc's soname doesn't change.
- **Build scripts**: `toolchain/Dockerfile` (38 hits: `TARGET=i586-pc-syllable`, `PREFIX=/opt/syllable`, image stages `syllable-build-base`, `syllable-build-system`, `syllable-native-gcc`, `syllable-apps`), `toolchain/build.sh` (14), `toolchain/scripts/*` (58 in 15 files), `toolchain/tests/syllable/`.
- **Native GCC packages** for running on Syllable (`sb setup` builds them into `apps/`): their install paths contain the triplet, so installed copies on existing disks keep the old name until reinstalled.

### 3.4 Ports and the port machinery

- `cmake/SyllablePorts.cmake` (61 hits; `syllable_port()`, `_syllable_links()`, `SYLLABLE_GLIBC_PREFIX`, `SYLLABLE_STAGING` x168 across ports), `cmake/Ports.cmake` (116), `cmake/ports/*` (152 in 11 files, including `openssl-syllable.conf`, `libtool-syllable.sh`).
- `cmake/scripts/apply-port.sh` tests each port's `config.sub` with `i586-pc-syllable` and swaps in automake's copy if it fails. This is the hook for shipping a `pyro`-aware `config.sub`/`config.guess`.
- Port-specific overrides that name Syllable: `50-git.cmake` (`uname_S=Syllable ...`), perl-cross (`--targetarch`, its config.sub no longer knows Syllable), OpenSSL's own target config (`openssl-syllable.conf`), the cups and cairo ports.
- **Syllable-Ports repo**: 277 hits in 57 files, 9 paths. Mostly the GCC 9.5.0 (62) and binutils 2.35 (34) copies, gdbserver 18.1 (54: a `syllable` target in gdbserver's configure and `*-syllable-low.c`), git 2.47 (36), gcc-4.1.2 (30), and one line each in the config scripts of m4, bison, flex, automake and autoconf.
- Port source patches that test `__SYLLABLE__`: 119 outside Builder in 29 files, 542 in Builder. Because GCC will keep defining `__SYLLABLE__`, **none of these need to change**; new patches should use `__PYRO__`.

### 3.5 libsyllable, appserver and headers

- `system/sys/appserver/libsyllable/CMakeLists.txt:43`: `syllable_library(libsyllable SONAME libsyllable.so.8 ...)`. Every GUI binary has `DT_NEEDED libsyllable.so.8`, including the apps on the 0.6.x golden image that `sb` compares against, and the ports. There are 174 `-lsyllable` link flags.
- `os::` is the C++ namespace, so no API symbols contain "syllable". Renaming the library is a soname and file-name change, not an API change.
- The Doxygen docs (`main.ddoc`: "The Syllable GUI API", `libatheos.dox`).
- appserver config: the default fixed font is `"Syllable-Console"` (`server/config.cpp:51`), backed by `system/other/fonts/base/SyllableConsole-Regu.ttf`. User font settings store the family name, so a rename needs a fallback mapping.
- Headers live in `system/sys/include/syllable/` (the repo directory), with subdirectories `syllable/` (39 headers), `atheos/` (39), `kernel/`, `posix/`, `gui/`, `util/`, `storage/` and others. They install to `/system/development/headers`, so `<syllable/types.h>` is `/system/development/headers/syllable/types.h`. In-tree includes: `<syllable/...>` 239, `<atheos/...>` 1,221 (840 outside Builder). The toolchain build also takes this repo directory as the `kernel-headers` build context.

### 3.6 Build system (CMake and sb)

- `CMakeLists.txt`: `project(Syllable ...)`, `if(NOT CMAKE_SYSTEM_NAME STREQUAL "Syllable")`, `SYLLABLE_HOST_DIR`, `SYLLABLE_VERSION`, the `include(Syllable...)` modules.
- `cmake/Platform/Syllable.cmake`, `cmake/syllable-i586.cmake` (toolchain file: `CMAKE_SYSTEM_NAME Syllable`, `/opt/syllable`, `i586-pc-syllable-*`), `cmake/Syllable.cmake` (the DSL: `syllable_driver`, `_program`, `_library`, `_static_library`, `_plugin`, `_resources`, `_catalogs`, `_attribute`, `_userland_headers`), `cmake/SyllableSDK.cmake`, `cmake/sdk/` (the SDK `./sb compile` uses outside the tree), `Grub.cmake` (`SYLLABLE_GRUB_DIR`), `Image.cmake` (`out/syllable.version`, `syllable.img`), `Documentation.cmake`, `NativeCompilers.cmake`. 43 distinct `SYLLABLE_*` CMake variables.
- `tools/` (host tools, `gen_compile_commands.py`, `sync_sysroot.py`, `base_flags.txt`: `-D__SYLLABLE__`).
- **sb** (199 hits): 23 environment variables (`SYLLABLE_DIR`, `_BUILD_DIR`, `_PORTS_DIR`, `_AFS_DIR`, `_WEBSITE_DIR`, `_OUT_DIR`, `_VOLUME`, `_COMPILER`, `_DISPLAY`, `_AUDIO`, `_SMP`, `_SSH_PORT`, `_AGENT_PORT`, `_DEBUG_INFO`, `_KERNEL_DEBUG`, `_JOBS`, `_IMAGE_DIR`, `_IMAGE_OVERLAY`, `_SRC`, `_VERSION`, `_APP`, `_SDK_DEBUG`, `_AFSTOOL_SOURCE`); Docker images `syllable-os-x86`, `syllable-toolchain-x86`, `syllable-base-x86`; volumes `syllable-build`, `syllable-build-9.5.0`, `syllable-sysroot`, `syllable-golden`, `syllable-build-wt`; output files `out/syllable-VERSION.qcow2`, `out/syllable.img`, `syllable-VERSION.iso.gz`; `Syllable/syllable.qcow2(.zip)` (the golden 0.6.x release disk); the default sibling folder names `Syllable`, `Syllable-Build-System`, `Syllable-Ports`.
- Also `docs/sb.md` (137), `docs/build-and-run.md` (54), `release/` (INSTALL.txt, cd scripts, make-release.sh), `tools/completions/_sb`, `tools/symbolize.py`, `test/*.sh`, `os/Dockerfile` (7), `CLAUDE.md`.

### 3.7 User-visible strings and on-disk configuration

- About boxes and titles: Dock ("Syllable" menu icon `Syllable.png`, `ICON_SYLLABLE`, "Syllable %d.%d.%d" in About), aterm ("Syllable Terminal", "Syllable virtual terminal"), Syllable Manager (`slbmgr`: window title, signature `application/x-vnd.JMH-Syllable Manager`), Sourcery, CodeEdit, AttribEdit, archiver, cpumon, mount, dlogin, Eyes, Printer and Switcher dock plugins, the launcher's VirtualDesktop plugin, SimpleBurn, Preferences/Printers. 48 string literals outside Webster and Builder.
- DiskManager partition table: type `0x2a` shown as "Syllable" / "AFS".
- `system/image/etc/hostname` = `syllable`; `etc/hosts`; `etc/profile`: `export SYSTEM=Syllable`, xterm title "... - Syllable Terminal".
- Bookmarks in `home/.template` and `home/root` (22 files) point at syllable.org sites, which are gone.
- Application signatures `application/x-vnd.syllable-*` (about 20: login, MediaPlayer, MediaConverter, ColdFish, Xpdf, Run, Monitor, sb-dock, Transferrer...) and `text/x-syllable-richtext`. These are identifiers stored in file attributes and the registrar's associations. Treat them like Haiku's `x-vnd.Be-*`: **keep them**, and use `x-vnd.pyro-*` only for new apps.
- `system/image/documentation` (989 hits in 17 files) and `system/doc/ChangeLog*`: historical, mostly.
- 171 keymaps: each has one header comment naming Syllable.

### 3.8 AtheOS-era names

- `system/sys/include/syllable/atheos/`: 39 headers. 35 are byte-for-byte copies of `syllable/`, and 4 have diverged (`types.h`, `fs_attribs.h`, `messages.h`, `ptrace.h`). The README calls it "a synonym for syllable, for backwards compatibility only", but the **kernel actually builds against it**, through the `kernel-headers/syllable -> atheos` symlink in `cmake/Syllable.cmake`.
- 4,713 "atheos" hits in 1,801 files: `atheos` 3,347, `AtheOS` 801 (mostly credits), `__ATHEOS__` 138 (GCC still defines it), `libatheos` 117, `ATHEOS_SRC`/`ATHEOS_INCLUDE_PATH` ~190 (old Makefiles), `<atheos/...>` includes 1,221.
- `system/sys/appserver/libatheos`: not built (`appserver/CMakeLists.txt` only builds libsyllable, appserver and registrar).
- `system/libs/glibc/sysdeps/unix/sysv/atheos` and `glibc-2.3.2`: not built.
- linux-afs (Linux driver for the AFS file system): "Syllable's (and AtheOS's) native file system", and the Linux file system type is `sylafs` (105 hits in 21 files).

**Recommendation for AtheOS names:** don't rename them. Fix the header duplication in Phase 0 instead (reconcile the 4 diverged headers, make `atheos/` forwarders, drop the kernel symlink hack), because the rename otherwise multiplies it into three copies. Keep `__ATHEOS__` defined.

### 3.9 Editor config and other checkouts

- `Syllable/.nvim.lua` (28 hits: `:SyllableDebugApp`, `vim.g.syllable_debug_app`, `vim.g.syllable_run`, `vim.g.mcp_checks = { "syllable-style" }`, the `syllable_program%s*%(` matcher, the path `.../Syllable-Build-System`), `.nvim/` (67 hits in 12 files, including `mcp/checks/syllable-style.md`), `.idea/Syllable.iml`, `.raycast/`, `.clangd`, `.clang-tidy`, `.syllable-box`, `.syllable-box.example`.
- **neovim-clion** (51 hits in 18 files, 1 path; `~/.config/nvim` is a checkout of it): `PLAN.md`, the errno.h default `system/sys/include/syllable/posix/errno.h`, the Makefile-to-CMake converter's `syllable_*` output and `:SyllableMakefileToCMake`.
- **dotfiles/nvim** (82 hits in 19 files): an older copy of the same config. neovim-clion's PLAN.md says to delete it, so delete rather than rename.
- **website** (Hugo, already titled Pyro): 1,038 hits in 65 content files, mostly translated old pages.
- **syllable-vm** (15 hits), **Dock2** (187, an old Dock copy), **Places** (18), **CircleMenu** (5): not in git, small, low priority.
- **Syllable-bochs, -cjson, -librsvg, -sqlite**: git worktrees of the Syllable repo, so the same content. Their `.git` files hold absolute paths, so renaming the main checkout folder needs `git worktree repair` afterwards.
- **PyroOS/Pyro** (the older Pyro repo, `Native/Bastards/Ported` layout): 5,089 hits, already partly renamed (`libpyro.so.1`, `__PYRO__` in a few port patches). That is the earlier attempt this plan replaces. It also shows that `__PYRO__` alone breaks ports that only test `__SYLLABLE__`, which is why both stay defined.

## 4. Phased plan

Each phase leaves a bootable system that passes `sb boot-test` and the desktop test. Phases 1 to 3 need no toolchain rebuild. Phase 4 is the only one that does.

### Phase 0: clean up first (about 1 day; shrinks the job by about a third)

1. Delete the dead code: `system/libs/glibc-2.3.2` (removes the 61k Unicode false positives as well), `system/libs/glibc`, `system/libs/libpthread-1`, `system/libs/compatibility`, `system/sys/appserver/libatheos`, the generated `config.status` files under `system/apps/gnu`, and possibly `system/build-tools` (check `do_install.rb` is still needed for the installer first).
2. Move the 6 live toolchain packages out of `Builder/packages/Syllable/` to `Syllable-Ports` (which already mirrors GCC 9.5.0 and binutils 2.35) or to `toolchain/ports/` in the Build-System, so the toolchain no longer reaches into the Builder tree. After that, leave the rest of `Builder/packages` alone as an archive, or move it out of the main repo.
3. Reconcile the 4 diverged `atheos/` headers with `syllable/`, make every `atheos/*.h` a one-line `#include <syllable/x.h>`, and delete the `kernel-headers/syllable -> atheos` symlink in `cmake/Syllable.cmake`. **Check:** the kernel and all drivers build, and the ABI-sensitive structs (`types.h`, `ptrace.h`, `messages.h`) haven't changed size (compile-time `static_assert`s on `sizeof`).
4. Add `scripts/rename-audit.sh`: `git grep -ciE 'syllable'` minus an allow-list (credits, dated docs, the Builder archive, compatibility aliases), so each phase can show what's left.

### Phase 1: names people see (1 to 2 days, low risk, no ABI change)

1. Kernel banner (`init.c`: "Pyro %s build %s starting..."), USB root hub string, and the matching patterns in `test/boot-test.sh` and `sb boot-test`.
2. About boxes and window titles listed in 3.7; Dock's menu icon and label (`Pyro.png`, `ICON_PYRO`); "Syllable Manager" becomes "Pyro Manager" (keep its signature).
3. `etc/hostname` = `pyro`, `etc/profile` `SYSTEM=Pyro` and the terminal title, the DiskManager label ("Pyro (AFS)"), the keymap header comments.
4. Replace the dead syllable.org bookmarks with the Pyro website and the repo.
5. The console font: rename to `PyroConsole-Regu.ttf` and family `Pyro-Console`, and make the appserver map a stored `Syllable-Console` to it.
6. Living docs: README, `docs/build-and-run.md`, `docs/ports-dsl.md`, `docs/sb.md`, `system/sys/include/.../README`, the Doxygen titles. Leave dated docs (`*-2026-11-*.md`) as written.

### Phase 2: build system, sb, editor (2 to 3 days, low-medium risk)

1. CMake: add `cmake/Pyro.cmake` with `pyro_program()`, `pyro_driver()` and the rest. Keep `cmake/Syllable.cmake` as a shim that defines each `syllable_*()` as a call to `pyro_*()`. Do the same for `PyroPorts.cmake`/`pyro_port()` and `PyroSDK.cmake`. Then sed the ~500 call sites in the tree's CMakeLists. Keep the shims for out-of-tree SDK projects.
2. `SYLLABLE_*` CMake variables become `PYRO_*`. For the cache options people pass on the command line (`SYLLABLE_COMPILER`, `SYLLABLE_DEBUG_INFO`, `SYLLABLE_KERNEL_DEBUG`, `SYLLABLE_JOBS`), read the old name if the new one isn't set and print a deprecation note.
3. `project(Pyro ...)`. Keep `CMAKE_SYSTEM_NAME Syllable` until Phase 4, because it is tied to the toolchain file and to ports that test it. Rename the toolchain file to `pyro-i586.cmake` and leave `syllable-i586.cmake` as a one-line `include()`.
4. **sb:** accept `PYRO_*` env vars with `SYLLABLE_*` fallbacks (`SRC=${PYRO_DIR:-${SYLLABLE_DIR:-...}}`); look for sibling folders by both names; name new images `out/pyro-VERSION.qcow2` and still find `out/syllable-VERSION.qcow2`. Docker: `docker tag syllable-os-x86 pyro-os-x86` and so on, so no toolchain rebuild is needed; switch sb to the new tags and remove the old ones in Phase 4. Volumes can't be renamed: create `pyro-build` with a fresh `sb configure` (an incremental rebuild), or keep the volume names until Phase 4. Update `_sb` completions, `release/`, `test/`, `docs/sb.md`, `CLAUDE.md`.
5. Neovim: in `.nvim.lua` and neovim-clion, add `:PyroDebugApp` and `vim.g.pyro_*`, keep the old command and variables as aliases for a while, rename `syllable-style.md` to `pyro-style.md` (keep the old check name accepted), update the converter to write `pyro_*()`, and make the errno.h default point at the new header path from Phase 3. Delete `dotfiles/nvim` as planned. Rename `.idea/Syllable.iml`, `.syllable-box*` (read the old name as a fallback), and `.raycast` titles.

### Phase 3: headers and macros (2 to 3 days, medium risk; still no toolchain rebuild)

1. Make `<pyro/...>` the real headers: `git mv` the inner `syllable/` directory to `pyro/`, then generate `syllable/*.h` forwarders (`#include <pyro/x.h>`). Point `atheos/*.h` at `<pyro/...>` too. Install all three into `/system/development/headers`.
2. Change the tree's own includes from `<syllable/...>` to `<pyro/...>` (239 sites) with a script. **Optionally** convert the 840 `<atheos/...>` includes outside Builder as well; that is the moment to retire them from in-tree code.
3. Rename the repo directory `system/sys/include/syllable` to `system/sys/include` (or `include/pyro`). This one changes the toolchain's `kernel-headers` build context and `SYLLABLE_HEADERS`, so do it together with the toolchain path change in `toolchain/build.sh`. Because the build context path only affects how the image is built, the existing image keeps working.
4. Macros: add `#define __PYRO__ 1` in a header every program already gets (`pyro/types.h` or `macros.h`) and `-D__PYRO__` in `tools/base_flags.txt`, so in-tree code can start using `__PYRO__` before GCC defines it. Don't touch the 644 existing `__SYLLABLE__` tests.
5. Keep internal identifiers such as include guards (`__F_SYLLABLE_*`), `ntfs_syllable2ntutc` and `acsyllable.h` unless they are being edited anyway. Renaming them gains nothing and adds diff noise.

### Phase 4: toolchain, triplet, glibc, uname (about 1 to 2 weeks, high risk; one coordinated change)

Do this in one branch across Syllable, Syllable-Build-System and Syllable-Ports, because the toolchain image, the sysroot and the CMake toolchain file must agree.

1. **config.sub/config.guess:** take current GNU config, add `pyro*` to the OS list next to `syllable*` and `i*86:pyro:*:*) echo "$UNAME_MACHINE"-pc-pyro` to config.guess, and ship them in the toolchain image (`/opt/pyro/host/share/config/`). Change `apply-port.sh` to test with `i586-pc-pyro` and install this pair. Send the patch upstream to config-patches@gnu.org.
2. **binutils 2.17 and 2.35:** match `i[3-7]86-*-pyro*` (and keep `*-syllable*`) in `config.bfd`, `gas/configure.tgt`, `ld/configure.tgt`; add `elf_i386_pyro.sh` and list `elf_i386_syllable` as an extra emulation; patch the bundled `config.sub`.
3. **GCC 4.1.2 and 9.5.0:** `config.gcc`, `libgcc/config.host`, `libstdc++-v3` crossconfig and the configure scripts match `*-*-pyro*` and `*-*-syllable*`; rename `syllable.h`/`.opt`/`t-syllable`/`x-syllable`/`mt-syllable`/`t-slibgcc-syllable`/`gthr-syllable.h` to their `pyro` names; `TARGET_OS_CPP_BUILTINS` adds `__PYRO__`, `__pyro__`, `system=pyro`, and keeps `__SYLLABLE__`, `__syllable__`, `__ATHEOS__`; the link spec uses `-m elf_i386_pyro`; `dynamic_linker='Pyro kernel loader'`; thread model `pyro`. Patch the bundled `config.sub`.
4. **glibc 2.7:** rename the add-on and `sysdeps/unix/sysv/syllable` to `pyro`, `--enable-add-ons=pyro`, and teach its configure the `pyro` host OS. Install prefix and soname don't change, so programs already on disk keep working.
5. **gdbserver 18.1** (Syllable-Ports): add the `pyro` target in its configure, rename `*-syllable-low.c`.
6. **Toolchain image:** `TARGET=i586-pc-pyro`, `PREFIX=/opt/pyro`, image names `pyro-*`. For one release, install `i586-pc-syllable-*` symlinks to the `i586-pc-pyro-*` tools and an `/opt/syllable -> /opt/pyro` link, so stray scripts and Mac-side tools keep working.
7. **Kernel:** `g_pzSystem = "pyro"`. Update `50-git.cmake` (`uname_S=Pyro`) and any port that branches on `uname`. On-target native builds need the new config.guess before this lands, which is why it ships in the same phase.
8. CMake: `CMAKE_SYSTEM_NAME Pyro`, `cmake/Platform/Pyro.cmake` (keep `Platform/Syllable.cmake` including it), `pyro-i586.cmake` with `/opt/pyro` and `i586-pc-pyro-*`. Remove the temporary Docker tags and old volumes from Phase 2.
9. Rebuild the native GCC packages (`sb setup` builds them), so the compilers that run on Pyro report the new triplet too.
10. **Check:** a full `sb setup` from scratch, `sb image-full`, `sb boot-test --smp 2`, the desktop test, the parity test against the golden image, `toolchain/tests`, building every port, a native "hello" compiled on the running system with the native GCC 9.5.0, and that a 0.6.x binary from the golden disk still runs.

### Phase 5: libsyllable to libpyro (at the next ABI break; medium-high risk)

The soname is `libsyllable.so.8`, and old binaries need it. Rename it only when the ABI changes anyway: for example when the system moves to GCC 9.5.0 with the new C++ ABI, or when the planned appserver and lambda-event changes break the class layout.

1. Build `libpyro.so.9` (`-lpyro`); add a `libsyllable.so -> libpyro.so` dev link so the 174 `-lsyllable` sites and port Makefiles still link.
2. Keep the real `libsyllable.so.8` from the last 0.x build installed beside it for old binaries. A plain symlink is not enough once the ABI differs. If the ABI hasn't changed, a `libsyllable.so.8 -> libpyro.so.8` symlink is enough, but then there's little reason to rename yet.
3. Change the `-lsyllable` sites in the tree.

### Phase 6: repos and folders (half a day, once the code is done)

1. Rename the GitHub repos Rick-PyroOS/Syllable, Syllable-Build-System and Syllable-Ports (GitHub redirects old URLs, including clones and open PRs). Decide then whether this replaces PyroOS/Pyro or gets merged into it later, as the README plans.
2. Rename the local folders, then run `git worktree repair` in the main checkout for Syllable-bochs, -cjson, -librsvg and -sqlite, and update `~/bin/sb`, Raycast and the Neovim paths. sb already looks for both names from Phase 2.
3. linux-afs: add `pyroafs` as a second file system type name next to `sylafs` (optional).

## 5. What to leave alone

- **Copyright and credit lines**: "Copyright (C) ... The Syllable Team", "Kurt Skauen" and the AtheOS kernel headers, "The Knights of Syllable", ChangeLogs. Rewriting attribution in GPL/LGPL code is wrong, and the README even says new headers should use the Syllable text. Change that rule to "Pyro" for new files only.
- **Dated docs** in `docs/` (`*-2026-10-*.md`, `*-2026-11-*.md`): they record what was true then.
- **The legacy Builder recipes** (3,186 hits, 1,648 paths) once the 6 toolchain ports have moved out.
- **Webster's WebKit port** (`PLATFORM(SYLLABLE)`, `platform/syllable`, 937 hits): rename it during the WebKit upgrade (`docs/webster-webkit-upgrade.md`), where the platform layer gets rewritten anyway.
- **`__SYLLABLE__` tests** in port patches and sources, **`x-vnd.syllable-*` signatures**, the **`.syllable-port` stamp** files in port build trees, and **include guards**.

## 6. Risks and how to test them

| Risk | Where | Mitigation |
|---|---|---|
| A port's configure rejects `i586-pc-pyro` | every autoconf port | patched config.sub via `apply-port.sh`; build every port in Phase 4 |
| Native builds on the running system fail after `uname -s` changes | config.guess, ports' uname checks | ship config.guess in the same phase; test a native configure on the running system |
| Old binaries fail to load | `libsyllable.so.8`, glibc path | soname untouched until Phase 5; parity test with golden disk apps |
| Kernel header drift when collapsing `atheos/` | 4 diverged headers | `sizeof` static_asserts, kernel + drivers build, boot test |
| Saved settings lose the console font | appserver font config | name mapping `Syllable-Console` to `Pyro-Console` |
| Broken scripts after folder and Docker renames | sb, toolchain/build.sh, `.nvim.lua`, Raycast, worktrees | fallbacks for old env vars and folder names, `git worktree repair`, image tags |
| Long mixed-state period | everything | `rename-audit.sh` with an allow-list, run in each phase |

## 7. Effort

Phase 0: about 1 day. Phase 1: 1 to 2 days. Phase 2: 2 to 3 days. Phase 3: 2 to 3 days. Phase 4: 1 to 2 weeks, mostly toolchain rebuilds (hours each) and port fixes. Phase 5: tied to the next ABI break. Phase 6: half a day. Phases 0 to 3 (about 1 to 1.5 weeks) give a system that looks and builds as Pyro everywhere a person sees it, with the toolchain still on `i586-pc-syllable`.

## 8. Questions for Rick

1. Triplet: switch to `i586-pc-pyro` in Phase 4 (recommended), or keep `i586-pc-syllable` as an internal name?
2. Repos: rename Rick-PyroOS/Syllable and the other two in place, or move the work into PyroOS/Pyro?
3. Dead code: OK to delete the unused glibc copies, libatheos and the generated `config.status` files in Phase 0, and to move the legacy Builder recipes out of the main repo?
