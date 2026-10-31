# The port definition language (`syllable_port`)

Oct 25, 2026

A reference for the small CMake DSL that the ports files use to declare third‑
party software. It covers every keyword (`DEPENDS`, `SHARED`, `SYSROOT`,
`STYLE`, …), what each one does as it is implemented, the two "out‑of‑band"
port forms the DSL does not cover, and the full build‑order (dependency) graph
across all the ports files. It is what you want when adding or reading a port.

---

## Where it lives, and how it is read

The DSL is *one function*, `syllable_port(...)`, defined in
`Syllable/cmake/SyllablePorts.cmake:88` (the canonical signature and a
prose comment are at lines 4–36). Every port file is just a list of calls
to it:

- `Syllable/cmake/Ports.cmake` — the base system ports and the static
  "stage" libraries; at its end (line 404–408) it `GLOB`s
  `cmake/ports/*.cmake`, sorts them and `include`s each one.
- `Syllable/cmake/NativeCompilers.cmake` — gmp/mpfr/mpc/isl and the GCC
  9.5.0 / binutils 2.35 build for the image.
- `Syllable/cmake/ports/10-compilers.cmake` — the build tools (m4, bison,
  flex, gperf, texinfo, pkgconf), the prebuilt native GCCs, glibc and the
  GCC runtime.
- `Syllable/cmake/ports/20-ssl.cmake` — OpenSSL 3.5.9 + cURL 8.23.0.
- `Syllable/cmake/ports/30-cairo.cmake` — the cairo stack (FreeType, libpng,
  Expat, fontconfig, gettext, GLib, cairo, Pango, libxml2 2.7.8).
- `Syllable/cmake/ports/40-cups.cmake` — CUPS 1.3.4.

The entry point is the toolchain file that `sb` passes:
`sb` → `cmake -DCMAKE_TOOLCHAIN_FILE=/src/cmake/syllable-i586.cmake`, which
pulls in `SyllablePorts.cmake` and so on. The `sb` script itself does no parsing;
it only runs CMake in the `syllable-os-x86` image.

The function body (the part that turns a call into a build) is
`SyllablePorts.cmake:88–242`, driven by one call to
`cmake_parse_arguments` (line 89).

---

## The signature

```
syllable_port(NAME
  PORT    <builder port dir>
  URL     <mirror> [<mirror> ...]
  SHA256  <hash>
  PREFIX  <install prefix>
  [CACHE          <config.site file>]
  [CONFIGURE      <args> ...]
  [MAKE_ARGS      <args> ...]
  [LINKS          <spec> ...]
  [STYLE          zlib | plain | openssl | icu]
  [DEST           sysroot]
  [SYSROOT]
  [SHARED]
  [DEPENDS        <port> ...]
  [BYPRODUCTS     <file> ...]
  [BUILD_TARGETS  <target> ...]
  [INSTALL_TARGETS <target> ...])
```

The parser (line 89–90) splits the arguments into three groups:

- **flags** (single, on/off): `SYSROOT`, `SHARED`
- **single value**: `PORT`, `SHA256`, `PREFIX`, `CACHE`, `STYLE`, `DEST`
- **list of values**: `URL`, `CONFIGURE`, `MAKE_ARGS`, `LINKS`, `DEPENDS`,
  `BYPRODUCTS`, `BUILD_TARGETS`, `INSTALL_TARGETS`

`NAME` is the first positional argument (the function's `name` param).

---

## Field reference (at a glance)

| Field | Type | What it does |
|---|---|---|
| **NAME** | required | The target id. Names the `ExternalProject` target, the build/install dir (`ports/<name>/dest`), and the group of install symlinks. Also how other ports name it in `DEPENDS`. |
| **PORT** | single | The Builder port dir under `SYLLABLE_BUILDER_PORTS` (= `system/apps/utils/Builder/packages`). `apply-port.sh` applies it over the downloaded source; `ensure-source.sh` re-applies it when it changes. |
| **URL** | list | One or more **download mirrors**, tried in turn until one succeeds (the tarball source). |
| **SHA256** | single | The hash of that tarball; becomes `URL_HASH SHA256=…` — an integrity check on download. |
| **PREFIX** | single | Where the port installs its files (e.g. `/usr/bison/2.3`). Overridden to the staging sysroot when `DEST sysroot`. |
| **CONFIGURE** | list | Args for the configure step — autoconf `./configure` flags, OpenSSL's `Configure`, or zlib's own. For default autoconf ports appended after a built‑in `--build … --host i586-pc-syllable --disable-nls`; with `SHARED` also `--enable-shared --disable-static`. |
| **CACHE** | single | A `CONFIG_SITE`‑style answers file for configure checks that can't run when cross‑compiling; exported as `CONFIG_SITE=` (default `/dev/null`, e.g. `glib.cache`, `gdbserver.cache`). |
| **MAKE_ARGS** | list | Args to the `make` build **and** install step; also used to name the compiler for plain‑Makefile ports. |
| **LINKS** | list | Install‑time symlink specs `TARGET:LINK` (target relative to `PREFIX`, link absolute); `@DIR:/LINKDIR` links **every** file in `PREFIX/DIR` into `LINKDIR` (a `pkgconfig` subdir is skipped). Done by `_syllable_links`. |
| **STYLE** | single | Build variant: `zlib`, `plain` (just its Makefile — bzip2/Info‑ZIP), `openssl` (`Configure linux-elf no-shared no-asm`), `icu` (host ICU built first, then cross). Default = autoconf cross. |
| **DEST** | single | If `sysroot`, install into the **staging sysroot** (`SYLLABLE_STAGING`, for later components to compile against) instead of the boot volume; forces static unless the style is `openssl`. |
| **SYSROOT** | flag | Mirror this port's headers + libraries into the staging sysroot so later ports compile/link against it (also runs `stage-fixup.sh`). |
| **SHARED** | flag | Build shared libs (`--enable-shared --disable-static`) and teach libtool about Syllable via `libtool-syllable.sh`; also auto‑adds `pkgconfig_host` to `DEPENDS`. |
| **DEPENDS** | list | Other `syllable_port` names that must be built **first** → passed to ExternalProject's `DEPENDS`, ordering the build (e.g. `DEPENDS glib cairo`). `SHARED` quietly adds `pkgconfig_host`. |
| **BYPRODUCTS** | list | Output files the target promises to make (`BUILD_BYPRODUCTS`), so linkers that need e.g. `libcups.so` are ordered after it. |
| **BUILD_TARGETS** / **INSTALL_TARGETS** | list | The make target to **build** (default: all) / to **install** with (default: `install`) — for a tree of which only a part is wanted (`gdbserver` builds `all-gdbserver`). |

Each row is expanded, with the "why" notes and the cross‑cutting behaviors, in the section below.

---

## The fields, each one

### `NAME` (required, positional)
The target id. It names three things at once: the CMake/`ExternalProject`
target `name`, the per‑port scratch build/install dir
`ports/<name>/` (`ports/<name>/dest` is where the package installs itself),
and the group of boot‑volume symlinks. It is also the string other ports name
in their `DEPENDS`.

### `PORT` (required, single)
The Builder port directory under `SYLLABLE_BUILDER_PORTS`
(= `system/apps/utils/Builder/packages`, `SyllablePorts.cmake:38`).
`apply-port.sh` applies it over the downloaded source, and `ensure-source.sh`
re‑applies it each time the step runs. This is the "what patch does this
package" knob — the per‑package fixes live in `Builder/packages/<PORT>`.

### `URL` (required, list)
One or more **source mirrors**, tried in turn until one downloads. Each entry
goes to `ExternalProject_Add`'s `URL`; the first that works is used. (GNU
sources list `mirrors.kernel.org` first because `ftp.gnu.org` often refuses
connections.)

### `SHA256` (required, single)
The hash of that tarball; becomes `URL_HASH SHA256=<hash>` — an integrity
check on download. If the hash fails the build stops, it does not fall through
to a wrong file.

### `PREFIX` (single)
Where the package installs its files, e.g. `/usr/bison/2.3`. On the boot
volume this is how `/system/resources/<Pkg>/<ver>` and `/usr/<Pkg>/<ver>`
are laid out, and `LINKS` then point the index dirs at it. (Overridden by
`DEST sysroot`, see below.)

### `CACHE` (single, optional)
A `CONFIG_SITE`‑style answers file, used to answer configure checks that **can
not run when cross compiling** — checks that need the target's runtime (a
running target program, an architecture guess, glibc's `getcwd`, …). It is
exported as `CONFIG_SITE=<file>` for the configure step (default `/dev/null`).
Examples: `bash.cache`, `gdbserver.cache` (libc's `getcwd`), `glib.cache`.

### `CONFIGURE` (list, optional)
Arguments passed to the configure step. For the default autoconf ports these
are appended after a built‑in `--build=x86_64-pc-linux-gnu
--host=i586-pc-syllable --disable-nls` (line 175–176). For `STYLE zlib`/`plain`/
`openssl`/`icu` they feed that style's own configure. With `SHARED` the port
also gets `--enable-shared --disable-static` prepended. This is where most of
a port's per‑package tweaks live.

### `MAKE_ARGS` (list, optional)
Passed to the `make` build **and** install step (line 182). Use it to restrict
what make builds (`SUBDIRS=lib src`), to force `-j1` when a port's makefiles
race (`bash`), or to pass the compiler for plain‑Makefile ports
(`CC=… AR=…`, see bzip2/zip below).

### `LINKS` (list, optional)
Install‑time symlink specs for the boot volume. Two forms (see
"the `@dir` form" below):
- `TARGET:LINK` — one symbolic link, `TARGET` relative to `PREFIX` to an
  absolute `LINK`.
- `@DIR:/LINKDIR` — one link for **every** file in `PREFIX/DIR` into `LINKDIR`
  (a `pkgconfig` subdir is skipped).
Implemented by `_syllable_links` (line 65–86); emitted as `install(CODE …)` so
the links are created at `cmake --install` time, not during the port's build.

### `STYLE` (single, optional)
Selects *how* the package is built. Default is autoconf cross. The styles:

- **zlib** — zlib's own `configure` (no `--host`), built in its source
  (`BUILD_IN_SOURCE`). Used for zlib.
- **plain** — no configure; just its `Makefile` in its source. `MAKE_ARGS`
  must name the compiler. Used for bzip2 and Info‑ZIP.
- **openssl** — OpenSSL's `Configure` (`linux-elf no-shared no-asm no-threads`),
  in its source; static libs only. Used for openssl 0.9.8zh.
- **icu** — ICU is cross‑built with a **host** ICU built first in the same tree
  (ICU needs its own tools at build time); only the libraries are cross‑built.
  Used for icu 3.6.

### `DEST sysroot` (single, optional)
Change where the package installs *to*: instead of `PREFIX`, it installs into
the **staging sysroot** `SYLLABLE_STAGING` (= `<builddir>/sysroot/usr`,
line 39). It is a **build‑time‑only** library: it never goes onto the boot
volume, and the autoconf configure is forced `--disable-shared --enable-static`
(OpenSSL excepted). This is the release's "stage" packages
(FreeType, LibPNG, static cURL/libxml2, …). It pairs with `BYPRODUCTS`.

### `SYSROOT` (flag, optional)
The *opposite* direction: the port still installs to its `PREFIX` on the boot
volume (and gets its `LINKS`), **and additionally** copies its headers and
libraries into the staging sysroot so later components compile and link against
it (line 201–207: copy `include`/`lib`, then run `stage-fixup.sh`). Used for
libraries that are both a shipped package and a build dependency — NCurses,
zlib, the shared FreeType/libpng, fontconfig, GLib, cairo, Pango.

Note `SYSROOT` and `DEST sysroot` are not combined in any current port: one
copies *into* the sysroot, the other installs *into* the sysroot.

### `SHARED` (flag, optional)
Build **shared** libraries (`--enable-shared --disable-static`), teach the
package's libtool about Syllable after configure (`libtool-syllable.sh`), and
point its configure at the host pkg‑config that reads only the staging sysroot
(`PKG_CONFIG=<host pkg-config>`, `PKG_CONFIG_LIBDIR=<staging>/lib/pkgconfig`,
line 114–117).

Two side effects to remember:
- It **auto‑adds `pkgconfig_host` to `DEPENDS`** (line 119), so every shared
  port waits for the host pkg‑config to be built first.
- "only here": it is deliberately scoped to this one step so that toggling it
  does not force a rebuild of every other port.

### `DEPENDS` (list, optional)  ← the headline one
Other ports that must be **built first**. It feeds `ExternalProject_Add`'s
`DEPENDS` (line 213), so CMake orders the targets and only runs this one after
its dependencies finish. Example: `pango` has `DEPENDS glib cairo`, and
`cairo` has `DEPENDS libpng_shared fontconfig`. `SHARED` quietly prepends
`pkgconfig_host` to this list. Caveat: a port can also need a *tool at build
time* without declaring it — e.g. bison and flex call `m4` by name and rely on
it being on `PATH` (see 10‑compilers.cmake); that is **not** a `DEPENDS`, so if
you add a port that needs a tool to build, write the dependency as `DEPENDS`.

### `BYPRODUCTS` (list, optional)
Extra output files the target promises to produce; passed to
`BUILD_BYPRODUCTS` (line 214). It lets CMake order things that link against a
particular file (`libz.so`, `libfreetype.a`, `libcups.so`, …) after the port
makes them. Typical for the `DEST sysroot` stage libs.

### `BUILD_TARGETS` / `INSTALL_TARGETS` (list, optional)
The make targets to **build** (default: all) and to **install** with (default:
`install`, line 91–93). Use them when only part of a tree is wanted —
`gdbserver` builds `all-gdbserver` / `install-gdbserver`.

---

## What a call becomes (the pipeline)

`syllable_port` does not create a normal configure/build/install triple. It
folds all three into **one** `ExternalProject_Add` BUILD step, because the
cross‑compile has no incremental state worth keeping:

1. Build the cross‑compile environment `env`: `CC/CXX/AR/RANLIB/STRIP`
   (`i586-pc-syllable‑*`), `CONFIG_SITE=<CACHE>`, and
   `CPPFLAGS/LDFLAGS` into the staging sysroot (line 110–113). `SHARED` adds
   the pkg‑config vars and `--enable-shared --disable-static`.
2. Pick the configure/build/install commands by `STYLE` (lines 121–184).
3. Create `ExternalProject_Add(name …)` (line 208–232):
   - `PREFIX ports/<name>`, `URL`/`URL_HASH SHA256=…`, `DEPENDS …`,
     `BUILD_BYPRODUCTS …`, optionally `BUILD_IN_SOURCE`.
   - `PATCH_COMMAND apply-port.sh <source> <Builder port>` — applies the
     per‑package patch (line 216).
   - `CONFIGURE_COMMAND ""` and `INSTALL_COMMAND ""` — both **empty**; the real
     work is in `BUILD_COMMAND` so that *any* change re‑runs the whole step
     from the tarball.
   - `BUILD_COMMAND` (line 222–230):
     `ensure-source.sh` re‑extracts the source → configure → build →
     `rm -rf dest` → install → `after_install` → `cleanup`.
4. `after_install` (line 196–207): strip the installed tree
   (`strip-tree.sh`; for boot‑volume ports only), and for `SYSROOT` ports copy
   `include`+`lib` into the staging sysroot and run `stage-fixup.sh`.
5. `cleanup` (line 192–195): delete the source and build trees, keeping only
   `dest` — unless `SYLLABLE_PORTS_KEEP=ON` (line 46), which keeps them for
   poking at a port.
6. Patch sensitivity (line 234–237): glob the Builder port dir and add it to the
   ExternalProject's `patch` step dependencies, so a changed *port patch* alone
   forces a re‑build.
7. For boot‑volume ports only (not `DEST sysroot`): `install(DIRECTORY dest →
   . )` copies the installed tree onto the boot volume, then
   `_syllable_links(PREFIX LINKS …)` creates the index links (line 238–241).

The net effect: a port downloads + checks the tarball, applies its patch,
configures for `i586-pc-syllable`, builds, installs itself into `dest/`, strips
it, (for `SYSROOT` ports) mirrors it into the staging sysroot, (for boot‑volume
ports) copies it onto the boot volume and makes its `LINKS`, then throws the
source and build away. The boot‑volume copy and the `LINKS` are the only part
that happens at `cmake --install` time.

---

## The `LINKS` `@dir` form, exactly

`_syllable_links(prefix spec …)` (line 65–86) takes each `spec` as `target:link`
split on the first `:`.

- **Single link** (`target` not starting with `@`):
  create one symbolic link, `prefix/target` → `link`, under the boot‑volume root
  (`$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}`). Example, zlb:
  `LINKS lib/libz.so.1:/system/libraries/libz.so.1` makes
  `/system/libraries/libz.so.1 → …/zlib/1.2.3/lib/libz.so.1`, so the kernel's
  loader (which searches `/system/libraries` first) finds `libz.so.1`
  (10‑compilers and zlib, line 254/255 of Ports.cmake).
- **Directory form** (`@DIR`): link **every** file in `prefix/DIR` into
  `linkdir`, with a shell loop (CMake lists would break on filenames like
  coreutils' `[`); it skips a `pkgconfig` subdir. Example:
  `LINKS @bin:/usr/index/bin` links every program in
  `prefix/bin` into `/usr/index/bin`, the dir `/etc/profile`'s `PATH`
  searches.

Both are written as `install(CODE "…")`, so they run under `cmake --install`
with the boot‑volume as `DESTDIR`, not during the port's build.

---

## Out‑of‑band ports (the DSL does not cover these)

Three things are written with plain CMake instead of `syllable_port`, because
they do not fit "download autoconf source and cross‑build it":

### 1. `ExternalProject_Add` ports
A few packages get a hand‑written `ExternalProject_Add` and then
`_syllable_links` / `install(DIRECTORY …)` for the boot volume (20‑ssl,
40‑cups) or stay a host build only (`file_host`). Their keyword set is the
standard CMake ExternalProject one:

- `URL` + `URL_HASH SHA256=…` — source and its integrity check.
- `DOWNLOAD_DIR` / `DOWNLOAD_NAME` / `DOWNLOAD_EXTRACT_TIMESTAMP`
  (`ON` keeps the file's original time) — where and how the source is staged.
- `BUILD_IN_SOURCE ON` — configure/build in the source tree.
- `PATCH_COMMAND …` — apply‑patch step (cups uses
  `apply-port.sh … Builder/packages/cups-1.3.4`).
- `CONFIGURE_COMMAND` / `BUILD_COMMAND` / `INSTALL_COMMAND` — the actual steps;
  OpenSSL's is its own `Configure` (`linux-elf`, static, with the
  `no‑…` flags) and CUPS's is `autoconf` then `./configure`.
- `DEPENDS` — same build‑ordering as the DSL `DEPENDS` (openssl3
  `DEPENDS cacert`, cups `DEPENDS zlib`).
- `BUILD_BYPRODUCTS` — the outputs the project promises
  (`libcups.so`, so the `syllable::cups` link target can be ordered).
- `LOG_CONFIGURE/BUILD/INSTALL/OUTPUT_ON_FAILURE ON` — log these steps.

These are: `cacert` (20‑ssl, a CA cert download, no build), `openssl3` (20‑ssl,
the real OpenSSL 3 for the image, `DEPENDS cacert`), `curl8` (20‑ssl, shared,
`SHARED DEPENDS openssl3`), `file_host` (Ports.cmake, the host `file` used to
compile `file`'s own magic database), and `cups` (40‑cups, `DEPENDS zlib`).

### 2. `syllable::` interface link targets
The static stage libraries are wrapped as CMake interface libraries so other
code links by name, e.g. `syllable::z`, `syllable::freetype`,
`syllable::png`, `syllable::jpeg`, `syllable::tiff`, `syllable::curl`,
`syllable::ssl`, `syllable::crypto`, `syllable::xml2`, `syllable::sqlite3`
(Ports.cmake, line 331–378) and `syllable::cups` (40‑cups, line 58–62).
Each `add_library(syllable_<x> INTERFACE)` exposes the staging sysroot's
include dir and links the right `lib< x>.a` (plus a `target_link_libraries`
chain, e.g. tiff links `jpeg`+`z`, curl links `ssl`+`crypto`+`libdl`), and
`add_dependencies(syllable_<x> <port>)` ties it to the port's build.
`add_library(syllable::<x> ALIAS syllable_<x>)` is the name consumers use.

### 3. `add_custom_command` / `add_custom_target` unpacks
The native GCCs and the two runtime libraries are not built by a port but
assembled, and show up as custom targets with their own `DEPENDS`:

- **10‑compilers.cmake** unpacks *prebuilt* native compiler packs
  (`gcc‑<v>.i586.tar.gz` + `binutils‑<v>.i586.tar.gz`) from
  `SYLLABLE_NATIVE_TOOLCHAINS` (`/apps`, made by
  `toolchain/build.sh -a`), merging both into `/usr/gcc/<v>` with
  `unpack-merged.sh`. It is **conditional**: if the tarballs are absent,
  "the image just goes without." `native_gcc_<v>` is an `ALL` custom target
  depending on a stamp that in turn `DEPENDS` on the two tarballs and the
  script (line 29–34).
- **NativeCompilers.cmake** instead *builds* GCC 9.5.0 from source
  (`native-gcc95`, `DEPENDS gmp mpfr mpc isl native-binutils95`). The same
  `/usr/gcc/9.5.0` can arrive by either route.
- **glibc** and **the GCC runtime** (10‑compilers' glibc install and
  `gccrt`, `SYLLABLE_GCCRT_PREFIX = /system/resources/gcc/<v>/lib`) are
  custom targets (`add_custom_target(glibc ALL …)`, `gccrt`) that copy/strip the
  toolchain's glibc and the compiler's `libgcc_s`/`libstdc++` into their
  release paths and link them from `/system/libraries`.

---

## Build‑order graph (every port, and its `DEPENDS`)

The dependency graph is `DEPENDS` plus the `add_dependencies` on the
`syllable::` targets. `+pkgconfig_host` marks the auto‑added dependency from
`SHARED`. "stage" = `DEST sysroot`, build‑time only; a path = boot volume.

### NativeCompilers.cmake — building the 9.5.0 compiler
| Port | target | DEPENDS |
|---|---|---|
| gmp | stage | — |
| mpfr | stage | gmp |
| mpc | stage | mpfr |
| isl | stage | gmp |
| native‑binutils95 | /usr/gcc/9.5.0 | — |
| native‑gcc95 | /usr/gcc/9.5.0 | gmp mpfr mpc isl native‑binutils95 |

### Ports.cmake — base system and stage libraries
| Port | target | DEPENDS |
|---|---|---|
| bash | /system/resources/bash/4.0 | — |
| coreutils | /system/resources/coreutils/6.4 | — |
| ncurses | /system/resources/ncurses/5.7 (SYSROOT) | — |
| readline | /system/resources/readline/6.0 (SYSROOT) | ncurses |
| grep / sed / gawk / findutils / diffutils / tar / gzip / make / xz | /usr/… | — |
| which | /usr/which/2.19 | readline |
| patch | /usr/patch/2.5.4 | — |
| less | /usr/less/429 | ncurses |
| mktemp | /usr/mktemp/1.6 | — |
| bzip2 (plain) | /usr/bzip2/1.0.5 | — |
| unzip / zip (plain) | /usr or system/resources | — |
| inetutils | /usr/inetutils/1.6 | ncurses |
| ed | /usr/ed/0.9 | — |
| nano | /usr/nano/2.0.3 | ncurses |
| file_host (EP, host build) | build‑only | — |
| file | /usr/file/5.11 | file_host |
| zlib (zlib style, SYSROOT) | /system/resources/zlib/1.2.3 | — |
| freetype | stage | — |
| libpng | stage | zlib |
| openssl 0.9.8zh (openssl style) | stage | — |
| curl 7.18.2 | stage | zlib openssl |
| libxml2 2.6.29 | stage | zlib |
| sqlite 3.5.5 | stage | — |
| icu 3.6 (icu style) | stage | — |
| libjpeg / giflib | stage | — |
| libtiff 3.8.2 | stage | zlib libjpeg |
| gdbserver 18.1 | /system/resources/gdbserver/18.1 | — (built with gcc 9.5.0) |
| pkgconfig_host (EP, host build) | build‑only | — |
| glibc / gccrt (custom targets) | system/resources/{glibc, gcc} | toolchain sysroot |

### ports/10-compilers.cmake — build tools, native GCCs, runtime
| Port | target | DEPENDS |
|---|---|---|
| m4 | /usr/m4/1.4.8 | — |
| bison | /usr/bison/2.3 | — (needs m4 on PATH, not a DEPENDS) |
| flex | /usr/flex/2.5.39 | — (needs m4 on PATH, not a DEPENDS) |
| gperf | /usr/gperf/3.0.2 | — |
| texinfo | /usr/texinfo/4.12 | ncurses |
| pkgconf | /usr/pkgconf/1.8.1 | — (built with gcc 9.5.0, static‑libgcc) |
| native‑gcc 4.1.2 / 9.5.0 (unpack) | /usr/gcc/<v> | the two native tarballs (conditional) |

### ports/20-ssl.cmake — OpenSSL 3 + cURL 8
| Port | target | DEPENDS |
|---|---|---|
| cacert (EP) | /usr/openssl/3.5.9/ssl/cert.pem | — |
| openssl3 (EP) | /usr/openssl/3.5.9 | cacert |
| curl8 (port, SHARED) | /usr/curl/8.23.0 | openssl3 + pkgconfig_host |

### ports/30-cairo.cmake — the cairo stack (all SHARED)
| Port | target | DEPENDS |
|---|---|---|
| freetype_shared | /usr/freetype/2.3.9 (SYSROOT) | zlib freetype + pkgconfig_host |
| libpng_shared | /usr/libpng/1.2.37 (SYSROOT) | zlib libpng + pkgconfig_host |
| expat | /usr/expat/2.0.0 (SYSROOT) | + pkgconfig_host |
| fontconfig | /usr/fontconfig/2.5.0 (SYSROOT) | freetype_shared expat + pkgconfig_host |
| gettext | /usr/gettext/0.17 | + pkgconfig_host |
| glib | /usr/glib/2.14.4 (SYSROOT) | gettext + pkgconfig_host |
| cairo | /usr/cairo/1.4.14 (SYSROOT) | libpng_shared fontconfig + pkgconfig_host |
| pango | /usr/pango/1.20.2 (SYSROOT) | glib cairo + pkgconfig_host |
| libxml2_shared | /usr/libxml2/2.7.8 | zlib + pkgconfig_host |

### ports/40-cups.cmake — printing
| Port | target | DEPENDS |
|---|---|---|
| cups (EP) | /usr/cups/1.3.4 | zlib (+ BUILD_BYPRODUCTS libcups.so) |
| syllable::cups (interface target) | — | add_dependencies → cups, links syllable_z |

### Longest chains, for orientation
- `native‑gcc95 ← native‑binutils95, isl ← gmp, mpc ← mpfr ← gmp`
- `file ← file_host`
- `curl (7.18.2) ← openssl ← (stage)` and `curl ‹ zlib`
- `libtiff ← libjpeg, zlib`
- `pango ← glib ← gettext ; pango ← cairo ← {libpng_shared, fontconfig} 
  ← {freetype_shared, expat} ← {zlib, …}` ; every shared port also
  `← pkgconfig_host`.

---

## Cross‑cutting behaviors to remember

- **`SHARED` ⇒ `DEPENDS pkgconfig_host`** automatically (line 119); you do not
  write it.
- **`DEST sysroot` forces static** (OpenSSL excepted) and the port never
  reaches the boot volume; **`SYSROOT`** additionally mirrors an on‑volume port
  into the staging sysroot. They are not used together.
- **Build order is `DEPENDS` only** — a build‑time tool (m4, autoconf, make on
  `PATH`) is *not* a dependency unless you write `DEPENDS`.
- **Everything re‑runs from the tarball** when any build command or the port
  patch changes (`ensure-source.sh`), so the cross build keeps no incremental
  state.
- **`SYLLABLE_PORTS_KEEP=ON`** keeps each port's source and build trees
  (default: they are deleted after install).
- **`BYPRODUCTS`** is how a downstream port is ordered to wait for a specific
  output file (a `libz.so`, a `libcups.so`); the `syllable::` targets are how
  CMake *consumers* express the same ordering at link time.
- **The two "pkg‑config"s are different**: `pkgconfig` (10‑compilers) is the
  *target* program installed on Syllable; `pkgconfig_host` (SyllablePorts.cmake)
  is a *host* program the shared ports' configure runs to read the staging
  sysroot's `.pc` files.

### The `syllable::` link targets (CMake consumers, not ports)
| Target | links | add_dependencies |
|---|---|---|
| syllable::z | libz.so | zlib |
| syllable::freetype | libfreetype.a + syllable_z | freetype |
| syllable::png | libpng.a + syllable_z | libpng |
| syllable::jpeg / ::gif / ::tiff | libjpeg.a / libgif.a / libtiff.a (tiff + jpeg + z) | libjpeg / giflib / libtiff |
| syllable::curl | libcurl.a + z + rt (+ ssl, crypto, libdl) | curl |
| syllable::ssl / ::crypto | libssl.a / libcrypto.a | openssl |
| syllable::xml2 / ::sqlite3 | libxml2.a / libsqlite3.a | libxml2 / sqlite |
| syllable::cups | libcups.so + syllable_z | cups |
