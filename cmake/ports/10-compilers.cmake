# Native compilers and build tools on the image (next-steps item 3).
#
# Two compiler sets, each with its binutils, in /usr/gcc/<version>
# (= /resources/gcc/<version>): GCC 4.1.2 + binutils 2.17 (the system's
# compiler, the default) and GCC 9.5.0 + binutils 2.35.  /etc/profile's
# use-gcc switches between them.  They're Canadian-cross builds made by the
# toolchain image (Syllable-Build-System: toolchain/build.sh -a, into apps/,
# which sb mounts at /apps).  Without those packages the image just goes without.

set(SYLLABLE_NATIVE_TOOLCHAINS /apps CACHE PATH
    "Native compiler packages (gcc-<v>.i586.tar.gz, binutils-<v>.i586.tar.gz) from toolchain/build.sh -a")

foreach(v 4.1.2 9.5.0)
  if(v STREQUAL "4.1.2")
    set(_bu binutils-2.17)
  else()
    set(_bu binutils-2.35)
  endif()
  set(_g ${SYLLABLE_NATIVE_TOOLCHAINS}/gcc-${v}.i586.tar.gz)
  set(_b ${SYLLABLE_NATIVE_TOOLCHAINS}/${_bu}.i586.tar.gz)
  if(NOT EXISTS ${_g} OR NOT EXISTS ${_b})
    message(STATUS "No native GCC ${v} in ${SYLLABLE_NATIVE_TOOLCHAINS} (toolchain/build.sh -a makes it): not on the image")
    continue()
  endif()
  set(_dir ${CMAKE_BINARY_DIR}/ports/native-gcc-${v})
  set(_dest ${_dir}/dest/usr/gcc/${v})
  # Each package unpacks to one folder (4.1.2/, or gcc-9.5.0/ and
  # binutils-2.35/ in older packages): both are merged into /usr/gcc/<v>
  add_custom_command(OUTPUT ${_dir}/stamp
    COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/unpack-merged.sh ${_dest} ${_g} ${_b}
    COMMAND ${CMAKE_COMMAND} -E touch ${_dir}/stamp
    DEPENDS ${_g} ${_b} ${PROJECT_SOURCE_DIR}/cmake/scripts/unpack-merged.sh
    COMMENT "Unpacking the native GCC ${v} and ${_bu}" VERBATIM)
  add_custom_target(native_gcc_${v} ALL DEPENDS ${_dir}/stamp)
  install(DIRECTORY ${_dir}/dest/ DESTINATION . USE_SOURCE_PERMISSIONS)
  # Both on PATH at once by version (gcc-9.5.0, g++-4.1.2...), whichever
  # use-gcc has made plain gcc; each driver finds its own binutils in its prefix
  if(v STREQUAL "4.1.2")
    _syllable_links(/usr/gcc/${v} bin/gcc:/usr/index/bin/gcc-${v} bin/g++:/usr/index/bin/g++-${v}
                    bin/cpp:/usr/index/bin/cpp-${v} bin/c++:/usr/index/bin/c++-${v})
  else()
    # 9.5.0's are scripts: it searches glibc's and the system's headers itself,
    # after its own (libstdc++'s #include_next), so they come out of the
    # C_INCLUDE_PATH /etc/profile sets for 4.1.2 (use-gcc 9.5.0 does the same)
    foreach(t gcc g++ cpp c++)
      set(_w ${_dir}/wrappers/${t}-${v})
      file(WRITE ${_w} [=[#!/bin/sh
# GCC @V@'s @T@ without glibc's and the system's headers in C_INCLUDE_PATH
# (it searches them itself, after its own; see /etc/profile)
_drop() { echo ":$1:" | sed -e 's|:/system/resources/glibc/2.7/include:|:|g' \
                            -e 's|:/system/development/headers:|:|g' -e 's|^:||' -e 's|:$||'; }
for v in C_INCLUDE_PATH CPLUS_INCLUDE_PATH; do
  eval "p=\$$v"; p=$(_drop "$p")
  # an empty entry would be the current folder
  if [ -n "$p" ]; then eval "$v=\$p; export $v"; else unset $v; fi
done
exec /usr/gcc/@V@/bin/@T@ "$@"
]=])
      file(READ ${_w} _text)
      string(REPLACE "@V@" ${v} _text "${_text}")
      string(REPLACE "@T@" ${t} _text "${_text}")
      file(WRITE ${_w} "${_text}")
      install(PROGRAMS ${_w} DESTINATION usr/index/bin)
    endforeach()
  endif()
endforeach()

# What the native compilers compile and link against: glibc's headers, start
# files (crt*.o) and static parts (libc_nonshared.a...), which the runtime
# install (SyllablePorts.cmake) leaves out.  /etc/profile's C_INCLUDE_PATH and
# LIBRARY_PATH point here.
install(DIRECTORY ${_glibc_src}/include/ DESTINATION ${SYLLABLE_GLIBC_PREFIX}/include)
install(DIRECTORY ${_glibc_src}/lib/ DESTINATION ${SYLLABLE_GLIBC_PREFIX}/lib
        FILES_MATCHING PATTERN "*.o" PATTERN "*.a")
# libc.so's linker script names /system/development/resources/glibc/2.7/lib/
# libc_nonshared.a, the release's place for them (the toolchain's sysroot links it the same way)
install(CODE "
  file(MAKE_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/development/resources/glibc)
  file(CREATE_LINK /boot/${SYLLABLE_GLIBC_PREFIX}
       \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/development/resources/glibc/2.7 SYMBOLIC)")

# ---- Build tools, shared by both compiler sets ------------------------------------
#
# The release's versions (Builder/packages).  M4=m4: bison and flex run m4 by
# name, found on PATH on Syllable (the build host's m4 passes their configure
# check).  Perl, Autoconf, Automake and libtool: at the end.

syllable_port(m4
  PORT m4-1.4.8
  URL ${_gnu}/m4/m4-1.4.8.tar.bz2 ${_gnu2}/m4/m4-1.4.8.tar.bz2
  SHA256 d0e6734743f9eca530ea90c289c0f1770bdf37f846561557f0b0db7f87b7d7cc
  PREFIX /usr/m4/1.4.8
  LINKS @bin:/usr/index/bin)

syllable_port(bison
  PORT bison-2.3
  URL ${_gnu}/bison/bison-2.3.tar.bz2 ${_gnu2}/bison/bison-2.3.tar.bz2
  SHA256 b10d7e9e354be72aee4e4911cf19dd27b5c527d4e7200857365b5fcdeea0dffb
  PREFIX /usr/bison/2.3
  CONFIGURE M4=m4
  LINKS @bin:/usr/index/bin)

# 2.5.33 (the release's) is gone from every mirror; 2.5.39 is the nearest
# release on GitHub.  Its scanner is shipped generated, so it builds without
# running flex.
syllable_port(flex
  PORT flex-2.5.33
  URL https://github.com/westes/flex/releases/download/flex-2.5.39/flex-2.5.39.tar.gz
  SHA256 71dd1b58158c935027104c830c019e48c73250708af5def45ea256c789318948
  PREFIX /usr/flex/2.5.39
  CONFIGURE M4=m4 ac_cv_func_malloc_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes
  MAKE_ARGS "SUBDIRS=lib ."
  LINKS @bin:/usr/index/bin)

syllable_port(gperf
  PORT gperf-3.0.2
  URL ${_gnu}/gperf/gperf-3.0.2.tar.gz ${_gnu2}/gperf/gperf-3.0.2.tar.gz
  SHA256 bc825cafc8a597983db1b44e86bc6399430f67f2a8633e6cdcf8fbf200186bf7
  PREFIX /usr/gperf/3.0.2
  LINKS @bin:/usr/index/bin)

# makeinfo (most GNU `make install`s want it) and info
syllable_port(texinfo
  PORT texinfo-4.12
  URL ${_gnu}/texinfo/texinfo-4.12.tar.gz ${_gnu2}/texinfo/texinfo-4.12.tar.gz
  SHA256 1db1df5530fd4521c14aa53cb6bd793eb8b412c70cb55a014d8ae5a2b64d4752
  PREFIX /usr/texinfo/4.12
  DEPENDS ncurses LINKS @bin:/usr/index/bin)

# pkg-config as pkgconf (plain C, no GLib): pkg-config 0.22's bundled GLib
# 1.2.8 has configure checks that can't be answered when cross compiling.
# Built with GCC 9.5.0 (C99), its libgcc linked in.
syllable_port(pkgconfig
  PORT pkg-config-0.22
  URL https://distfiles.ariadne.space/pkgconf/pkgconf-1.8.1.tar.xz
  SHA256 644361ada2942be05655d4452eb018791647c31bba429b287f1f68deb2dc6840
  PREFIX /usr/pkgconf/1.8.1
  CONFIGURE --disable-shared COMPILER 9.5.0
  LINKS @bin:/usr/index/bin bin/pkgconf:/usr/index/bin/pkg-config
        share/aclocal/pkg.m4:/usr/index/share/aclocal/pkg.m4)

# ---- Perl and the GNU build system ------------------------------------------------
#
# Current versions (the release's Perl 5.10 can't be cross-built; Automake
# before 1.15.1 doesn't run on a current Perl).  Perl comes from perl-cross,
# which configures with test programs it only compiles, built with GCC 9.5.0
# (Perl needs C99), its libgcc linked in.  The XS modules are linked into
# perl itself (--all-static); DynaLoader stays (List::Util, which File::Temp
# needs, comes only with it) and links the tree's libdl (CMakeLists.txt).
# /usr/bin/perl: the scripts below name it (#!/usr/bin/perl).

set(_perl_prefix /usr/perl/5.40.2)
set(_perl_dest ${CMAKE_BINARY_DIR}/ports/perl/dest)
set(_perl_env ${CMAKE_COMMAND} -E env --unset=TARGET PATH=${SYLLABLE_TOOLCHAIN_ROOT}/bin:$ENV{PATH})
ExternalProject_Add(perl_cross
  PREFIX ${CMAKE_BINARY_DIR}/ports/perl_cross
  URL https://github.com/arsv/perl-cross/releases/download/1.6.4/perl-cross-1.6.4.tar.gz
  URL_HASH SHA256=b6202173b0a8a43fb312867d85a8cd33527f3f234b1b6e591cdaa9895c9920c7
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_NO_EXTRACT ON
  CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")
ExternalProject_Add(perl
  PREFIX ${CMAKE_BINARY_DIR}/ports/perl
  URL https://www.cpan.org/src/5.0/perl-5.40.2.tar.gz
  URL_HASH SHA256=10d4647cfbb543a7f9ae3e5f6851ec49305232ea7621aed24c7cfbb0bef4b70d
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  BUILD_IN_SOURCE ON
  # perl-cross is unpacked over the Perl source (its configure, Makefiles and
  # the patches for Perl 5.40.2 it applies itself)
  PATCH_COMMAND tar -C <SOURCE_DIR> --strip-components=1
                    -xzf ${CMAKE_BINARY_DIR}/downloads/perl-cross-1.6.4.tar.gz
          # static XS modules: MakeMaker's "static" leaves out their .pm files
          # (POSIX.pm...), "all" builds them as .so: both static and pm_to_blib
          COMMAND sed -i "s/LINKTYPE=static static$/LINKTYPE=static static pm_to_blib/" <SOURCE_DIR>/Makefile
  # --targetarch: perl-cross's config.sub no longer knows Syllable.  Its
  # function checks link with ccflags and libs only (not ldflags), so the
  # staging sysroot's -L (libdl.so) goes into ccflags too, and the libraries
  # are named (no hints file for Syllable: nanosleep is a Linux hint).  ext/re
  # can't be linked statically into 5.40 (duplicate regcomp symbols).
  CONFIGURE_COMMAND ${_perl_env} ./configure --target=i586-pc-syllable
    --targetarch=i586-pc-syllable --target-tools-prefix=${_gcc95}- --host-cc=/usr/bin/gcc
    --prefix=${_perl_prefix} -Dosname=syllable --all-static --disable-mod=ext/re
    "-Dlibs=-lm -lcrypt -ldl" "-Dperllibs=-lm -lcrypt -ldl" -Dd_nanosleep
    -Dinc_version_list=none
    # the release's hints/syllable.sh (Builder/packages/perl-5.8.8), which
    # Perl no longer ships: no hard links on AFS, glibc's malloc
    -Ud_link -Ddont_use_nlink -Uusemymalloc -Ud_locconv -Dldlibpthname=LIBRARY_PATH
    "-Accflags=-I${SYLLABLE_STAGING}/include -L${SYLLABLE_STAGING}/lib"
    "-Aldflags=-static-libgcc -L${SYLLABLE_STAGING}/lib"
  BUILD_COMMAND ${_perl_env} make -j${SYLLABLE_JOBS}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_perl_dest}
          COMMAND ${_perl_env} make install DESTDIR=${_perl_dest}
          COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_perl_dest}
  DEPENDS perl_cross
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
install(DIRECTORY ${_perl_dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
_syllable_links(${_perl_prefix} @bin:/usr/index/bin bin/perl:/usr/bin/perl)

# Perl and shell scripts, configured with the build host's Perl (/usr/bin/perl,
# which the Syllable link above matches) and its m4, sed and grep by name, so
# the same names are found on Syllable.  aclocal finds the other packages'
# macros (libtool.m4, pkg.m4) in /usr/index/share/aclocal (/etc/profile's
# ACLOCAL_PATH).
syllable_port(autoconf
  PORT AutoConfigure--current
  URL ${_gnu}/autoconf/autoconf-2.72.tar.xz ${_gnu2}/autoconf/autoconf-2.72.tar.xz
  SHA256 ba885c1319578d6c94d46e9b0dceb4014caafe2490e437a0dbca3f270a223f5a
  PREFIX /usr/autoconf/2.72
  CONFIGURE PERL=/usr/bin/perl M4=m4
  LINKS @bin:/usr/index/bin)

syllable_port(automake
  PORT AutoMake--current
  URL ${_gnu}/automake/automake-1.16.5.tar.xz ${_gnu2}/automake/automake-1.16.5.tar.xz
  SHA256 f01d58cd6d9d77fbdca9eb4bbd5ead1988228fdb73d6f7a201f5f8d6b118b469
  PREFIX /usr/automake/1.16.5
  CONFIGURE PERL=/usr/bin/perl
  LINKS @bin:/usr/index/bin)

# The port's patch: Syllable in libtool.m4 (shared libraries, Linux-style
# sonames), as the release's libtool-2.2.6 port.  It makes aclocal.m4 look
# out of date: the autotools are no-ops in its make (the shipped files fit).
# libltdl isn't installed.  (The installed libtool script itself names the
# cross compiler; packages make their own with libtoolize.)
syllable_port(libtool
  PORT libtool-2.4.7
  URL ${_gnu}/libtool/libtool-2.4.7.tar.xz ${_gnu2}/libtool/libtool-2.4.7.tar.xz
  SHA256 4f7f217f057ce655ff22559ad221a0fd8ef84ad1fc5fcb6990cecc333aa1635d
  PREFIX /usr/libtool/2.4.7
  CONFIGURE M4=m4 SED=sed GREP=grep "EGREP=grep -E" "FGREP=grep -F" --disable-ltdl-install
  MAKE_ARGS ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true
  LINKS @bin:/usr/index/bin @share/aclocal:/usr/index/share/aclocal)

# Doxygen 1.8.17: the release's 1.5.1 is gone from every mirror, and 1.8.18
# and later need C++11 threads, which GCC 9.5.0's Syllable thread layer
# (gthr-syllable.h) lacks.  1.8.17 has its own (qtools' QThread on POSIX
# threads): Syllable's libpthread from the tree (CMakeLists.txt), its pthread.h
# after glibc's headers, as on Syllable (C_INCLUDE_PATH).  CMake, cross-built
# with the tree's toolchain file and GCC 9.5.0 (C++14), libgcc and libstdc++
# linked in; flex, bison and Python from the build host.
set(_doxygen_dest ${CMAKE_BINARY_DIR}/ports/doxygen/dest)
set(_doxygen_pthread ${CMAKE_BINARY_DIR}/ports/doxygen/pthread)
ExternalProject_Add(doxygen
  PREFIX ${CMAKE_BINARY_DIR}/ports/doxygen
  URL https://downloads.sourceforge.net/project/doxygen/rel-1.8.17/doxygen-1.8.17.src.tar.gz
  URL_HASH SHA256=2cba988af2d495541cbbe5541b3bee0ee11144dcb23a81eada19f5501fd8b599
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  # the port's patch: Syllable in qtools' qglobal.h (__SYLLABLE__), as the
  # release's doxygen-1.5.1 port
  PATCH_COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/apply-port.sh <SOURCE_DIR> ${SYLLABLE_BUILDER_PORTS}/doxygen-1.8.17
  # -lpthread: a folder with only the tree's libpthread (not the staging
  # sysroot: the other ports would find it there)
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${_doxygen_pthread}
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:libpthread> ${_doxygen_pthread}/libpthread.so
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGET PATH=/opt/syllable/host/bin:$ENV{PATH}
      ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> -G Ninja
      -DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/cmake/syllable-i586.cmake
      -DSYLLABLE_TOOLCHAIN_ROOT=${SYLLABLE_TOOLCHAIN_ROOT} -DSYLLABLE_COMPILER=9.5.0
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/doxygen/1.8.17
      "-DCMAKE_C_FLAGS=-idirafter ${PROJECT_SOURCE_DIR}/system/sys/include/syllable"
      "-DCMAKE_CXX_FLAGS=-idirafter ${PROJECT_SOURCE_DIR}/system/sys/include/syllable"
      "-DCMAKE_EXE_LINKER_FLAGS=-static-libgcc -static-libstdc++ -L${_doxygen_pthread}"
      -DCMAKE_POLICY_DEFAULT_CMP0056=NEW    # the linker flags in its checks too (FindThreads)
  BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=TARGET PATH=/opt/syllable/host/bin:$ENV{PATH}
                ${CMAKE_COMMAND} --build <BINARY_DIR> -j ${SYLLABLE_JOBS}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_doxygen_dest}
          COMMAND ${CMAKE_COMMAND} -E env DESTDIR=${_doxygen_dest} ${CMAKE_COMMAND} --install <BINARY_DIR>
          COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_doxygen_dest}
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
install(DIRECTORY ${_doxygen_dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
_syllable_links(/usr/doxygen/1.8.17 @bin:/usr/index/bin)
