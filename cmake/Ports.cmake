# Third-party ports on the boot volume (see SyllablePorts.cmake).  GNU sources
# list mirrors.kernel.org first: ftp.gnu.org often refuses connections.

syllable_port(bash
  PORT BASh-4.0
  URL https://mirrors.kernel.org/gnu/bash/bash-4.0.tar.gz
      https://ftp.gnu.org/gnu/bash/bash-4.0.tar.gz
  SHA256 9793d394f640a95030c77d5ac989724afe196921956db741bcaf141801c50518
  PREFIX /system/resources/bash/4.0
  CONFIGURE --without-bash-malloc
  CACHE ${PROJECT_SOURCE_DIR}/cmake/ports/bash.cache
  MAKE_ARGS -j1                        # its readline Makefile races in parallel
  LINKS bin/bash:/bin/bash bin/bash:/bin/sh)

# Only lib and src: the man pages are made by running the built programs,
# which are Syllable programs
syllable_port(coreutils
  PORT coreutils-6.4
  URL https://mirrors.kernel.org/gnu/coreutils/coreutils-6.4.tar.bz2
      https://ftp.gnu.org/gnu/coreutils/coreutils-6.4.tar.bz2
  SHA256 75a6b82bb7e3d07bec5b9bd0fcdfdfdfb88f32728e5089972001b50e4148b87b
  PREFIX /system/resources/coreutils/6.4
  MAKE_ARGS "SUBDIRS=lib src"
  LINKS @bin:/bin)

# ---- The command-line base (Builder profiles base and syllable) -----------------
#
# Release versions.  The system packages go in /system/resources, the rest in
# /resources (= /usr) with their programs linked from /resources/index/bin,
# which /etc/profile's PATH searches.  NCurses and ReadLine are static, as the
# release built them.

set(_gnu https://mirrors.kernel.org/gnu)
set(_gnu2 https://ftp.gnu.org/gnu)

# The terminfo database is compiled with the build host's tic (the built one is
# a Syllable program); it writes the format ncurses 5.7 reads.
syllable_port(ncurses
  PORT NCurses-5.7
  URL ${_gnu}/ncurses/ncurses-5.7.tar.gz ${_gnu2}/ncurses/ncurses-5.7.tar.gz
  SHA256 0a9bdea5c7de8ded5c9327ed642915f2cc380753f12d4ad120ef7da3ea3498f4
  PREFIX /system/resources/ncurses/5.7
  CONFIGURE --without-shared --without-debug --without-ada --without-cxx-binding
            --enable-overwrite --with-build-cc=gcc
  MAKE_ARGS TIC_PATH=/usr/bin/tic
  SYSROOT LINKS @bin:/usr/index/bin)

syllable_port(readline
  PORT ReadLine-6.0
  URL ${_gnu}/readline/readline-6.0.tar.gz ${_gnu2}/readline/readline-6.0.tar.gz
  SHA256 1199d905c025735b34cf67d03078585e18f84707052413279fe68ce48c1cc1de
  PREFIX /system/resources/readline/6.0
  CONFIGURE --disable-shared --with-curses --enable-multibyte
            bash_cv_wcwidth_broken=no
  SYSROOT DEPENDS ncurses)

syllable_port(grep
  PORT GRep-2.5.4
  URL ${_gnu}/grep/grep-2.5.4.tar.bz2 ${_gnu2}/grep/grep-2.5.4.tar.bz2
  SHA256 0abec64e01458fbd032e33351d8071602a959529ea796965e98338ee83a40020
  PREFIX /usr/grep/2.5.4
  CONFIGURE --disable-perl-regexp
  LINKS @bin:/usr/index/bin)

syllable_port(sed
  PORT SEd-4.2.1
  URL ${_gnu}/sed/sed-4.2.1.tar.bz2 ${_gnu2}/sed/sed-4.2.1.tar.bz2
  SHA256 2ac3b36ca37bfeb43c4ef4025778cd66d89c77abb843d90552a515a7c9d2948f
  PREFIX /usr/sed/4.2.1
  LINKS @bin:/usr/index/bin)

syllable_port(gawk
  PORT gawk-3.1.6
  URL ${_gnu}/gawk/gawk-3.1.6.tar.bz2 ${_gnu2}/gawk/gawk-3.1.6.tar.bz2
  SHA256 513fdd5a0bd1b467bd4993eb3adb4690f65fff6857c9e2ed1a3e46d72151cb6c
  PREFIX /usr/gawk/3.1.6
  # No extension loading (extension()): it finds dlfcn.h in the sysroot, but
  # libdl is the tree's own (system/libs/dl), built after the ports
  CONFIGURE ac_cv_header_dlfcn_h=no
  LINKS @bin:/usr/index/bin)

syllable_port(findutils
  PORT FindUtils-4.4.2
  URL ${_gnu}/findutils/findutils-4.4.2.tar.gz ${_gnu2}/findutils/findutils-4.4.2.tar.gz
  SHA256 434f32d171cbc0a5e72cfc5372c6fc4cb0e681f8dce566a0de5b6fccd702b62a
  PREFIX /usr/findutils/4.4.2
  # glibc 2.7's fflush is fine; the cross guess "no" pulls in gnulib's fseeko,
  # which clashes with glibc's large-file fseeko
  # (the port's patch fixes gnulib's wchar.h for glibc 2.7)
  CONFIGURE gl_cv_func_fflush_stdin=yes
  LINKS @bin:/usr/index/bin)

syllable_port(diffutils
  PORT diffutils-2.8.1
  URL ${_gnu}/diffutils/diffutils-2.8.1.tar.gz ${_gnu2}/diffutils/diffutils-2.8.1.tar.gz
  SHA256 c5001748b069224dd98bf1bb9ee877321c7de8b332c8aad5af3e2a7372d23f5a
  PREFIX /usr/diffutils/2.8.1
  LINKS @bin:/usr/index/bin)

syllable_port(tar
  PORT TAr-1.22
  URL ${_gnu}/tar/tar-1.22.tar.bz2 ${_gnu2}/tar/tar-1.22.tar.bz2
  SHA256 3cefb67f197a3f8f5235999fe749ced0889466a71ddcbc96fddca84e5d53aa4d
  PREFIX /usr/tar/1.22
  LINKS @bin:/usr/index/bin)

syllable_port(gzip
  PORT gzip-1.3.12
  URL ${_gnu}/gzip/gzip-1.3.12.tar.gz ${_gnu2}/gzip/gzip-1.3.12.tar.gz
  SHA256 3f565be05f7f3d1aff117c030eb7c738300510b7d098cedea796ca8e4cd587af
  PREFIX /usr/gzip/1.3.12
  LINKS @bin:/usr/index/bin)

syllable_port(which
  PORT which-2.19
  URL ${_gnu}/which/which-2.19.tar.gz ${_gnu2}/which/which-2.19.tar.gz
  SHA256 7d79b874f65118ac846a0deb31a8fbd6816cd81e74930299c82103765d45cd52
  PREFIX /usr/which/2.19
  DEPENDS readline LINKS @bin:/usr/index/bin)

syllable_port(patch
  PORT patch-2.5.4
  URL ${_gnu}/patch/patch-2.5.4.tar.gz ${_gnu2}/patch/patch-2.5.4.tar.gz
  SHA256 dd2fc5a745bfca5450d13d7032fdc47ab102514aae3efb3fe334a6eff87df799
  PREFIX /usr/patch/2.5.4
  # its Makefile has no DESTDIR
  INSTALL_TARGETS install prefix=${CMAKE_BINARY_DIR}/ports/patch/dest/usr/patch/2.5.4
  LINKS @bin:/usr/index/bin)

syllable_port(make
  PORT make-3.81
  URL ${_gnu}/make/make-3.81.tar.bz2 ${_gnu2}/make/make-3.81.tar.bz2
  SHA256 f3e69023771e23908f5d5592954d8271d3d6af09693cecfd29cee6fde8550dc8
  PREFIX /usr/make/3.81
  LINKS @bin:/usr/index/bin)

syllable_port(less
  PORT Less-429
  URL https://www.greenwoodsoftware.com/less/less-429.tar.gz
  SHA256 d3073eceb3c9314d1fab7df92a39d1aea4782db4cafa982ea670cd2f6c713408
  PREFIX /usr/less/429
  DEPENDS ncurses LINKS @bin:/usr/index/bin)

syllable_port(xz
  PORT XZ-Utilities-4.999.9beta
  URL https://tukaani.org/xz/xz-4.999.9beta.tar.bz2
  SHA256 330312c4397608d8b7be362cc7edbfeafa6101614bc2164d816ea767656aa15c
  PREFIX /usr/xz/4.999.9beta
  CONFIGURE --disable-shared
  LINKS @bin:/usr/index/bin)

syllable_port(mktemp
  PORT MkTemp-1.6
  URL https://www.mktemp.org/dist/mktemp-1.6.tar.gz
  SHA256 a1275889c7bb6b3c353d5b9ebb983064e8ef1d29ef8688bd132cec065642d2d9
  PREFIX /usr/mktemp/1.6
  # its Makefile has no DESTDIR
  INSTALL_TARGETS install prefix=${CMAKE_BINARY_DIR}/ports/mktemp/dest/usr/mktemp/1.6
  LINKS @bin:/usr/index/bin)

set(_cc CC=i586-pc-syllable-gcc AR=i586-pc-syllable-ar RANLIB=i586-pc-syllable-ranlib)

# Its default target also runs the tests (Syllable programs)
syllable_port(bzip2
  PORT bzip2-1.0.5
  URL https://sourceware.org/pub/bzip2/bzip2-1.0.5.tar.gz
  SHA256 f7bf5368309d76e5daf3a89d4d1bea688dac7780742e7a0ae1af19be9316fe22
  PREFIX /usr/bzip2/1.0.5
  STYLE plain MAKE_ARGS ${_cc} BUILD_TARGETS libbz2.a bzip2 bzip2recover
  LINKS @bin:/usr/index/bin)

syllable_port(unzip
  PORT UnZip-6.0
  URL https://downloads.sourceforge.net/project/infozip/UnZip%206.x%20%28latest%29/UnZip%206.0/unzip60.tar.gz
  SHA256 036d96991646d0449ed0aa952e4fbe21b476ce994abc276e49d30e686708bd37
  PREFIX /system/resources/unzip/6.0
  STYLE plain MAKE_ARGS -f unix/Makefile ${_cc} BUILD_TARGETS generic
  LINKS @bin:/usr/index/bin)

syllable_port(zip
  PORT Zip-3.0
  URL https://downloads.sourceforge.net/project/infozip/Zip%203.x%20%28latest%29/3.0/zip30.tar.gz
  SHA256 f0e8bb1f9b7eb0b01285495a2699df3a4b766784c1765a8f1aeedf63c0806369
  PREFIX /usr/zip/3.0
  # Its own AtheOS port (atheos/: file attributes), which calls as and make by name
  STYLE plain MAKE_ARGS -f atheos/Makefile ${_cc} AS=i586-pc-syllable-as
                        "MAKE=make -f atheos/Makefile"
  LINKS @bin:/usr/index/bin)

# inetd, telnet and ftp; the rest of INetUtils is in the tree (nettools) or
# not wanted on a desktop
syllable_port(inetutils
  PORT INetUtils-1.6
  URL ${_gnu}/inetutils/inetutils-1.6.tar.gz ${_gnu2}/inetutils/inetutils-1.6.tar.gz
  SHA256 052bd9473b7620387fe63f4a6a3af0e4137dcf0f33d85e0ecfd22f5e11145bde
  PREFIX /usr/inetutils/1.6
  # ftp without ReadLine: the static libreadline carries its own xmalloc, which
  # clashes with gnulib's
  CONFIGURE --disable-servers --disable-clients --enable-inetd --enable-telnet --enable-ftp
            gl_cv_lib_readline=no
  DEPENDS ncurses
  LINKS @bin:/usr/index/bin @libexec:/usr/index/sbin)

# ed: its own configure, which takes the compiler as an argument only
syllable_port(ed
  PORT ed-0.9
  URL ${_gnu}/ed/ed-0.9.tar.bz2 https://download.savannah.gnu.org/releases/ed/ed-0.9.tar.bz2
  SHA256 22fa075554b669178b7c073aab5274c473d311a5a10b1ccdae2f45759e21c7f7
  PREFIX /usr/ed/0.9
  CONFIGURE CC=i586-pc-syllable-gcc
  LINKS @bin:/usr/index/bin)

# nano with NCurses (the release's recipe used S-Lang, which isn't ported)
syllable_port(nano
  PORT nano-2.0.3
  URL https://www.nano-editor.org/dist/v2.0/nano-2.0.3.tar.gz
  SHA256 8c29569d0e1eb29cbd3580313b9796c4e6fd0830071ec885021f22be635f5297
  PREFIX /usr/nano/2.0.3
  DEPENDS ncurses LINKS @bin:/usr/index/bin)

# file: the release's 4.07 has no source left, so 5.11 (2012).  Its magic
# database is compiled by running file, so a host file of the same version
# is built first and used for that (FILE_COMPILE).
set(_file_url https://astron.com/pub/file/file-5.11.tar.gz ftp://ftp.astron.com/pub/file/file-5.11.tar.gz)
set(_file_sha c70ae29a28c0585f541d5916fc3248c3e91baa481f63d7ccec53d1534cbcc9b7)
ExternalProject_Add(file_host
  PREFIX ${CMAKE_BINARY_DIR}/ports/file_host
  URL ${_file_url} URL_HASH SHA256=${_file_sha}
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  INSTALL_DIR ${CMAKE_BINARY_DIR}/host-file
  CONFIGURE_COMMAND env -u TARGET CC=/usr/bin/gcc <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
                    --disable-shared
  BUILD_COMMAND env -u TARGET make -j${SYLLABLE_JOBS}
  INSTALL_COMMAND env -u TARGET make install
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
syllable_port(file
  PORT file-4.07
  URL ${_file_url}
  SHA256 ${_file_sha}
  PREFIX /usr/file/5.11
  CONFIGURE --disable-shared
  MAKE_ARGS FILE_COMPILE=${CMAKE_BINARY_DIR}/host-file/bin/file
  DEPENDS file_host LINKS @bin:/usr/index/bin)

# ---- Libraries for the appserver, translators and media ------------------------

# zlib 1.3.1 in place of the release's 1.2.3 (same soname and ABI, libz.so.1):
# cURL 8 wants a newer one.  Its configure goes by the build host's uname
# (Linux), so it needs no port.  The header and libz.so are linked from
# /usr/index for programs compiled on Syllable.
syllable_port(zlib
  PORT zlib-1.3.1
  URL https://zlib.net/fossils/zlib-1.3.1.tar.gz
  SHA256 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
  PREFIX /system/resources/zlib/1.3.1
  STYLE zlib CONFIGURE --shared SYSROOT BYPRODUCTS ${SYLLABLE_STAGING}/lib/libz.so
  LINKS lib/libz.so.1:/system/libraries/libz.so.1 lib/libz.so:/usr/index/lib/libz.so
        include/zlib.h:/usr/index/include/zlib.h include/zconf.h:/usr/index/include/zconf.h
        lib/pkgconfig/zlib.pc:/usr/index/lib/pkgconfig/zlib.pc)

# FreeType and LibPNG are linked in statically, as in the release (Builder's
# "stage" packages): the staging sysroot only, not the boot volume
syllable_port(freetype
  PORT FreeType-2.3.9
  URL https://download.savannah.gnu.org/releases/freetype/freetype-old/freetype-2.3.9.tar.bz2
  SHA256 51a033bce9904d73e0821e8e2bba24bd319619d7d1b2e9eeccac611580242ab6
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libfreetype.a)
syllable_port(libpng
  PORT LibPNG-1.2.37
  URL https://download.sourceforge.net/libpng/libpng-1.2.37.tar.bz2
  SHA256 682960b55527b54bada90e959c2d42679444a1db43677c77eb645a29645f86d1
  DEST sysroot DEPENDS zlib BYPRODUCTS ${SYLLABLE_STAGING}/lib/libpng.a)

# ---- Webster's libraries: static, build-time only --------------------------------

syllable_port(openssl
  PORT openssl-0.9.8e
  URL https://www.openssl.org/source/old/0.9.x/openssl-0.9.8zh.tar.gz
  SHA256 f1d9f3ed1b85a82ecf80d0e2d389e1fda3fca9a4dba0bf07adbf231e1a5e2fd6
  STYLE openssl DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libssl.a ${SYLLABLE_STAGING}/lib/libcrypto.a)

# cURL (Transferrer, Webster).  The release has it as a shared system package;
# its libtool does not know Syllable, so it is static, build-time only here.
syllable_port(curl
  PORT cURL-7.18.2
  URL https://curl.se/download/archeology/curl-7.18.2.tar.gz
  SHA256 55a711ad1d703f8b9c3ca80e5b859b5222f64566f9a3b82ac6773055ef8cee71
  CONFIGURE --with-ssl=${SYLLABLE_STAGING} --without-libssh2 --disable-ldap --disable-manual
            --with-zlib=${SYLLABLE_STAGING} --without-ca-bundle
            ac_cv_file___dev_urandom_=yes   # a cross build cannot check; Syllable has it
            LIBS=-ldl                       # static libcrypto needs dlopen()
  DEST sysroot DEPENDS zlib openssl BYPRODUCTS ${SYLLABLE_STAGING}/lib/libcurl.a)   # and libdl (CMakeLists.txt)

syllable_port(libxml2
  PORT libxml2-2.6.29
  URL http://xmlsoft.org/sources/old/libxml2-2.6.29.tar.gz
  SHA256 e34a19e0b9d7ac03874acffd624f181c85a98fb976dc84411d6cd1bdda965992
  CONFIGURE --without-threads --without-python --with-zlib=${SYLLABLE_STAGING}
  DEST sysroot DEPENDS zlib BYPRODUCTS ${SYLLABLE_STAGING}/lib/libxml2.a)

syllable_port(sqlite
  PORT sqlite-3.5.5
  URL https://www.sqlite.org/sqlite-3.5.5.tar.gz
  SHA256 c430ec6dca870d7c4af35b07cbdf90a20b8a857cc90c7c93c31c6896a8b5270d
  CONFIGURE --disable-threadsafe --disable-tcl
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libsqlite3.a)

syllable_port(icu
  PORT icu-3.6
  URL https://sourceforge.net/projects/icu/files/ICU4C/3.6/icu4c-3_6-src.tgz/download
  SHA256 5135e8d69d6206d320515df7aeee7027711ab1aef9d8dbf29571a97a9746b041
  STYLE icu CONFIGURE --disable-threads --disable-64bit-libs --disable-extras
                      --disable-samples --disable-tests
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libsicuuc.a ${SYLLABLE_STAGING}/lib/libsicudata.a)

# Image libraries for the translators, static like FreeType and LibPNG
syllable_port(libjpeg
  PORT LibJPEG-8a
  URL https://www.ijg.org/files/jpegsrc.v8a.tar.gz
  SHA256 bf86a5eaafa79b5d60feb27bee08df2db4c9ca2fb40d453e7a2080f7663a713b
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libjpeg.a)
syllable_port(giflib
  PORT GIFLib-4.1.6
  URL https://downloads.sourceforge.net/project/giflib/giflib-4.x/giflib-4.1.6/giflib-4.1.6.tar.bz2
  SHA256 e1c1ced9c5bc8f93ef0faf0a8c7717abf784d10a7b270d2285e8e1f3b93f2bed
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libgif.a)
syllable_port(libtiff
  PORT libtiff-3.8.2
  URL https://download.osgeo.org/libtiff/old/tiff-3.8.2.tar.gz
  SHA256 be88f037080b93ce0a337b2b3ca5e8195f76885deb6c558cc3aa872ee848fc76
  CONFIGURE --without-x --disable-cxx
  MAKE_ARGS "SUBDIRS=port libtiff"
  DEST sysroot DEPENDS zlib libjpeg BYPRODUCTS ${SYLLABLE_STAGING}/lib/libtiff.a)

# Targets to link against
add_library(syllable_z INTERFACE)
target_link_libraries(syllable_z INTERFACE ${SYLLABLE_STAGING}/lib/libz.so)
add_dependencies(syllable_z zlib)
add_library(syllable::z ALIAS syllable_z)

add_library(syllable_freetype INTERFACE)
target_include_directories(syllable_freetype SYSTEM INTERFACE
  ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include)
target_link_libraries(syllable_freetype INTERFACE ${SYLLABLE_STAGING}/lib/libfreetype.a syllable_z)
add_dependencies(syllable_freetype freetype)
add_library(syllable::freetype ALIAS syllable_freetype)

add_library(syllable_png INTERFACE)
target_include_directories(syllable_png SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
target_link_libraries(syllable_png INTERFACE ${SYLLABLE_STAGING}/lib/libpng.a syllable_z)
add_dependencies(syllable_png libpng)
add_library(syllable::png ALIAS syllable_png)

foreach(lib jpeg gif tiff)
  add_library(syllable_${lib} INTERFACE)
  target_include_directories(syllable_${lib} SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
  target_link_libraries(syllable_${lib} INTERFACE ${SYLLABLE_STAGING}/lib/lib${lib}.a)
  add_library(syllable::${lib} ALIAS syllable_${lib})
endforeach()
add_dependencies(syllable_jpeg libjpeg)
add_dependencies(syllable_gif giflib)
add_dependencies(syllable_tiff libtiff)
target_link_libraries(syllable_tiff INTERFACE syllable_jpeg syllable_z)

add_library(syllable_curl INTERFACE)
target_include_directories(syllable_curl SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
# librt: clock_gettime (glibc 2.7)
target_link_libraries(syllable_curl INTERFACE ${SYLLABLE_STAGING}/lib/libcurl.a syllable_z rt)
add_dependencies(syllable_curl curl)
add_library(syllable::curl ALIAS syllable_curl)

foreach(lib ssl crypto xml2 sqlite3)
  add_library(syllable_${lib} INTERFACE)
  target_include_directories(syllable_${lib} SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
  target_link_libraries(syllable_${lib} INTERFACE ${SYLLABLE_STAGING}/lib/lib${lib}.a)
  add_library(syllable::${lib} ALIAS syllable_${lib})
endforeach()
add_dependencies(syllable_ssl openssl)
add_dependencies(syllable_crypto openssl)
add_dependencies(syllable_xml2 libxml2)
add_dependencies(syllable_sqlite3 sqlite)
target_link_libraries(syllable_curl INTERFACE syllable_ssl syllable_crypto libdl)

# ---- Development ------------------------------------------------------------------

# gdbserver (gdb 18.1 with the Syllable target, Builder/packages/gdbserver-18.1):
# lets another machine's gdb debug programs here.  It needs C++17, so it is
# built with GCC 9.5.0 whichever compiler builds the system, and links its C++
# runtime statically (the system's is GCC 4.1.2's).  It only runs on a debug
# image (./sb image --debug: system/index/tasks/start/gdbserver).
set(_gcc95 ${SYLLABLE_TOOLCHAIN_ROOT}/bin/i586-pc-syllable)
syllable_port(gdbserver
  PORT gdbserver-18.1
  URL https://mirrors.kernel.org/gnu/gdb/gdb-18.1.tar.xz
      https://ftp.gnu.org/gnu/gdb/gdb-18.1.tar.xz
  SHA256 cd9fc3fe2b47743840e42c1592d3d87f8302eb18639c0b8b4ba0898002e2348f
  PREFIX /system/resources/gdbserver/18.1
  CONFIGURE --disable-gdb --disable-sim --disable-binutils --disable-ld --disable-gas
            --disable-gprof --disable-gold --disable-libctf --disable-readline
            --without-zstd --without-xxhash --enable-gdbserver
            CC=${_gcc95}-gcc CXX=${_gcc95}-g++
            "LDFLAGS=-static-libstdc++ -static-libgcc"
  CACHE ${PROJECT_SOURCE_DIR}/cmake/ports/gdbserver.cache   # libc's getcwd (see there)
  BUILD_TARGETS all-gdbserver INSTALL_TARGETS install-gdbserver
  LINKS bin/gdbserver:/system/development/bin/gdbserver)

# ---- More ports, one file per area: cmake/ports/NN-<area>.cmake -------------------

file(GLOB _port_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ports/*.cmake)
list(SORT _port_files)
foreach(f ${_port_files})
  include(${f})
endforeach()
