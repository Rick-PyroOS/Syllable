# Webster: upgrading WebKit

Status as of 2026-11-01. A research write-up, not a port.

**The newest WebKit that GCC 9.5.0 can build is the WebKitGTK 2.40.x line (2.40.5 is its last release).** 2.42 requires GCC 10.2. Missing native TLS is not what limits us. The gates are C++11 threads in GCC's Syllable thread layer and the lack of a multi-process IPC stack, which forces a single-process (WebKitLegacy-style) port. Target **2.38.x** first, because it is the last release with an upstream non-Apple WebKitLegacy port (WinCairo: Cairo + cURL, no GLib) to copy from, then roll forward to 2.40.5. GCC 4.1.2 cannot build any of this and stays only for today's 2008 Webster.

## Where Webster is today

Webster is WebKit trunk **r29946**, a nightly from 2 February 2008. `Pyro/Bastards/WebKit/Makefile` pins `VERSION = r29946`, and the tree in `system/apps/internet/Webster` matches it (newest ChangeLog entries 2008-01-28 and 2008-02-02).

| Layer | What the tree has | What that means for an upgrade |
| --- | --- | --- |
| JavaScriptCore | Pre-SquirrelFish `kjs/` tree-walking interpreter, PCRE, `USE_SYSTEM_MALLOC` | No bytecode, no JIT. Anything from mid-2008 on is a different engine. |
| Threading | `ThreadingNone.cpp` | WebKit made threads mandatory in r95511 (Sept 2011). |
| Graphics | Native `platform/graphics/syllable/*` on `os::View` | Replaced by WebKit's Cairo backend (see the cairo branch below). |
| Network | Own `ResourceHandleManager` on cURL 7.18.2 + OpenSSL 0.9.8zh, both static | Maps onto upstream's `platform/network/curl` backend. |
| Embedding | `WebView` (libwebview) + `ABrowse_NG`, one process | The same model the upgrade must keep. |
| Dependencies | ICU 3.6, libxml2 2.6.29, SQLite 3.5.5, libpng, libjpeg, all static | All below 2.40's minimums. |
| Build | `Makefile.syllable` via CMake `ExternalProject`, GCC 4.1.2 | Upstream is CMake + Ruby + Python 3 + Perl. |

## Toolchain

- **GCC 4.1.2.** WebKit hard-required GCC 4.7 from r162126 (16 January 2014, C++11 template aliases). The real 4.1.2 ceiling is earlier, probably 2010 to 2012; not pinned by a test compile and not worth chasing.
- **GCC 9.5.0.** Covers C++17 and the subset of C++20 WebKit used while GCC 9 was its floor. Our gcc4-compatible `std::string` ABI and SJLJ exceptions compile but are untested upstream.
- **TLS is not a blocker.** No native TLS (glibc `--without-__thread`, gcc `--disable-tls`), but under GCC 9.5 `__thread`/`thread_local` work through libgcc emutls on gthread keys. WebKit 2.40's `Thread::current()` and `ThreadSpecific` use `pthread_key_create` on non-Darwin; the only `thread_local` in WTF/JSC is two counters in `FastMalloc.cpp` behind `ASSERT_ENABLED`. (GCC 4.1.2 predates emutls.)
- **C++11 threads are the blocker.** `gthr-syllable.h` doesn't define `__GTHREADS_CXX0X`, so `<mutex>`, `<condition_variable>`, `<thread>` and `std::call_once` are empty. WTF's `WordLock.cpp`, under every WTF lock, uses `std::mutex` and `std::condition_variable`; 64 files in WTF/JSC/bmalloc use `std::call_once`. See `docs/cxx11-threads.md`.

## The ceiling

| Constraint | Newest WebKit it allows | Evidence |
| --- | --- | --- |
| GCC 9.5.0 | WebKitGTK **2.40.5** (floor GCC 9.3; 2.42.0 needs 10.2) | `Source/cmake/WebKitCommon.cmake` at 2.38.0 (8.3), 2.40.0/2.40.5 (9.3), 2.42.0 (10.2) |
| GCC 4.1.2 | Before r162126 (Jan 2014), realistically earlier | WTF `Compiler.h` GCC 4.7 check |
| No native TLS | No limit under GCC 9.5 | pthread keys in WTF 2.40 |
| No C++11 gthreads | About 2013 until `__GTHREADS_CXX0X` exists | `WordLock.cpp` |
| No AF_UNIX / SCM_RIGHTS / shared memory | WebKit2 multi-process is out. Last upstream non-Apple single-process port: WinCairo WebKitLegacy in **2.38.x**, removed Feb 2023 | `OptionsWin.cmake`: 2.38 `ENABLE_WEBKIT_LEGACY ON`, 2.40 `OFF` |
| 32-bit x86 JavaScriptCore | 2.40 has asm LLInt on x86-32 (no JIT); CLoop forced from May 2024 | `PlatformEnable.h`; commit 8e3653b4 |

## Graphics: the cairo branch

The `cairo` branch already has the architecture WebKit's Cairo backend needs. The appserver draws with Cairo on bitmap memory (`graphics_cairo.cpp`, `cairo_image_surface_create_for_data` on `SrvBitmap::m_pRaster`); libos has Cairo-backed fills, pens, shapes, transforms and PostScript output, and text through Pango-Cairo. Webster's glue becomes "give WebKit a Cairo image surface over an `os::Bitmap`", not a new graphics backend.

The branch currently builds older libraries than 2.40 needs; these are being upgraded with the rest of the ports:

| Library | cairo branch today | WebKit 2.40 needs | Target |
| --- | --- | --- | --- |
| Cairo | 1.4.12 | 1.14+ (WinCairo 1.15.12+) | 1.18 |
| pixman | bundled with old Cairo | 0.40+ for Cairo 1.18 | current |
| Fontconfig | 2.5.0 | 2.8+ | current |
| FreeType | 2.3.x | 2.4.2+ | current |
| HarfBuzz | none | 0.9.18+, with ICU | current (new port) |
| Pango / glib | 1.20.2 / 2.14 | not used by WebKit | as the desktop needs |

Cairo, Fontconfig and FreeType keep backward ABI compatibility, so the appserver and Pango code should keep working across the bump. Upgrade the system Cairo rather than linking a private copy into Webster: two Cairos in one process clash.

## Networking: cURL and OpenSSL

- **cURL 8.23.0** (current). WebKit 2.40 needs 7.77+; every cURL call and option its network code uses exists in 8.23 (`CURLOPT_PROTOCOLS`, `CURLOPT_SSL_ENABLE_NPN` are deprecated, warnings only). cURL 8.18 dropped OpenSSL 1.1.1 and older, so it needs OpenSSL 3.0+; its CI tests OpenSSL 4.0.3. Configure with `--disable-ipv6 --disable-unix-sockets` (`socket(AF_UNIX)` silently makes a TCP socket on Syllable) and the threaded resolver. nghttp2 for HTTP/2 can come later.
- **OpenSSL**: up to **4.1.0** (current, 23 Oct 2026), or **3.5.9 LTS** (supported to April 2030) for fewer upgrades. Both need GCC 9.5 (C99, `__atomic` builtins); thread-local state uses pthread keys. Two fixes:
  - `pthread_rwlock_*` return ENOSYS in libpthread, and OpenSSL uses rwlocks whenever `PTHREAD_RWLOCK_INITIALIZER` is defined. Implement rwlocks (preferred) or patch OpenSSL to use mutexes on Syllable.
  - WebKit's `platform/network/curl/OpenSSLHelper.cpp` (line 240) reads `ASN1_TIME` fields directly; 4.0 made `ASN1_STRING` opaque. A few-line patch to use accessors. 3.5/3.6 need no patch.
- Today's Webster keeps its static cURL 7.18.2 and OpenSSL 0.9.8zh until the new WebKit replaces it.

## What 2.38/2.40 needs from Syllable

| Need | Syllable today | Work |
| --- | --- | --- |
| `<mutex>`, `<condition_variable>`, `std::call_once` | Empty | **Hard.** `__GTHREADS_CXX0X` in `gthr-syllable.h`, rebuild libstdc++ |
| POSIX semaphores | Missing | Add `sem_*` to libpthread (WTF thread suspend) |
| `pthread_rwlock_*` | ENOSYS | Implement (OpenSSL) |
| Thread-suspend signal ucontext | All zeros | Build without `HAVE(MACHINE_CONTEXT)`; WTF falls back to an approximate stack pointer |
| `pthread_getattr_np` | Missing | Syllable case in `StackBounds.cpp` from `get_thread_info` |
| `sched_yield` | ENOSYS | `snooze(0)`, return 0 |
| Anonymous `mmap` + `mprotect` | Work | None; no JIT on x86-32 |
| File `mmap` | Broken | Avoid `MappedFileData` or fix it (POSIX audit P0-6) |
| ICU | 3.6 | 61.2+ (77 planned) |
| libxml2 / SQLite | 2.6.29 / 3.5.5 | 2.9.7+ / 3.23.1+ |
| libpng, libjpeg, zlib | Old | Current; libwebp optional |
| Build host tools | CMake, Perl, bison, flex, gperf | Add Ruby 2.5+ and Python 3 to the build image |
| Embedding | 2008 WebView client API | Rewrite on WinCairo WebKitLegacy 2.38 clients |

## Path

1. C++11 gthreads in GCC 9.5 (also unblocks newer Doxygen and most current C++).
2. libpthread: `sem_*`, rwlocks, `sched_yield`, stack bounds.
3. `PORT=JSCOnly` 2.38.x with `-DENABLE_C_LOOP=OFF -DENABLE_JIT=OFF` (asm LLInt), test suite under QEMU.
4. Ports: ICU, Cairo 1.18 + pixman, FreeType, Fontconfig, HarfBuzz, cURL 8.23, OpenSSL, libxml2, SQLite, libpng/libjpeg.
5. `PORT=Syllable` (`OptionsSyllable.cmake`, modelled on `OptionsWin.cmake`): WebCore + Syllable WebKitLegacy 2.38.x on WinCairo's Cairo/cURL/WebKitLegacy pieces, Syllable glue for windows, input, timers, clipboard and the bitmap blit; ABrowse_NG on the new WebView.
6. Roll forward to 2.40.5, carrying the WebKitLegacy layer one release. Beyond that needs GCC 10.2+ (2.42) or the planned GCC 15.

## Risks

- 2.40's WebCore is several times today's 18 MB `libwebcore`; 32-bit address space and loader limits are untested at that size.
- Upstream never builds with the gcc4-compatible string ABI or SJLJ; expect a few fixes.
- Kernel TLD slot count must cover WebKit's pthread keys plus emutls.
- The 2.38 to 2.40 WebCore client-API delta hasn't been sized.
- 2.40.5 (2023) lacks every security fix since; a big step from 2008, not safe for hostile sites.

## Sources

- WebKit tags webkitgtk-2.38.0, 2.38.6, 2.40.0, 2.40.5, 2.42.0 (https://github.com/WebKit/WebKit)
- r162126, Require GCC 4.7: https://lists.webkit.org/pipermail/webkit-changes/attachments/20140116/9233edd9/attachment-0001.html
- r95511, threads always on: https://trac.webkit.org/r95511
- WebKitGTK GCC requirements: https://trac.webkit.org/wiki/WebKitGTK/GCCRequirement?version=26
- C++20 in WebKit: https://lists.webkit.org/pipermail/webkit-dev/2021-December/032064.html
- Windows WebKitLegacy removed: https://lists.webkit.org/pipermail/webkit-unassigned/2023-February/1088040.html
- CLoop on x86-32: https://lists.webkit.org/pipermail/webkit-changes/2024-May/302222.html
- OpenSSL 4.1.0 NEWS: https://raw.githubusercontent.com/openssl/openssl/openssl-4.1.0/NEWS.md
- OpenSSL 4.0 release: https://openssl-library.org/post/2026-04-14-openssl-40-final-release/
- cURL 8.23.0 DEPRECATE.md and CI: https://github.com/curl/curl/tree/curl-8_23_0
- Local: `docs/cxx11-threads.md`, Syllable-Build-System `docs/upgrade-plan.md`, project `docs/posix-audit-2026-10-30.md`
