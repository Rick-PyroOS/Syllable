# Third-party software: Builder ports (Builder/packages/<PORT>) cross-built
# with their own configure scripts, and glibc from the toolchain's sysroot.
#
#   syllable_port(NAME PORT dir URL url SHA256 hash PREFIX /system/resources/...
#                 [CONFIGURE args...] [CACHE file] [MAKE_ARGS args...]
#                 [LINKS target:link ...] [STYLE zlib] [DEST sysroot] [SYSROOT]
#                 [DEPENDS ports...] [BYPRODUCTS files...])
#       Downloads and checks the source, applies the port, configures for
#       i586-pc-syllable with PREFIX, builds, installs into the build tree and
#       strips it; `cmake --install` copies it into the boot volume.
#       CACHE is a CONFIG_SITE file answering the configure checks that cannot
#       run when cross compiling.  LINKS: TARGET (relative to PREFIX) to LINK
#       (absolute); @DIR:/LINKDIR links every file in PREFIX/DIR.
#       STYLE zlib: zlib's own configure (no --host, builds in its source).
#       STYLE openssl: OpenSSL's Configure (linux-elf, static, no assembler),
#       in its source.
#       STYLE icu: ICU (source/configure), cross-built with a host ICU built
#       first in the same tree (ICU needs its own tools at build time).
#       DEST sysroot: a static, build-time-only library (the release's "stage"
#       packages, e.g. FreeType): installed into the staging sysroot that later
#       components compile against (SYLLABLE_STAGING), not onto the boot volume.
#       SYSROOT: a system package whose headers and libraries later components
#       also need at build time (e.g. zlib).

set(SYLLABLE_BUILDER_PORTS ${PROJECT_SOURCE_DIR}/system/apps/utils/Builder/packages)
set(SYLLABLE_STAGING ${CMAKE_BINARY_DIR}/sysroot/usr)
file(MAKE_DIRECTORY ${SYLLABLE_STAGING}/include ${SYLLABLE_STAGING}/lib ${SYLLABLE_STAGING}/bin)
get_filename_component(_cross_bin ${CMAKE_C_COMPILER} DIRECTORY)
cmake_host_system_information(RESULT SYLLABLE_JOBS QUERY NUMBER_OF_LOGICAL_CORES)

function(_syllable_links prefix)
  foreach(spec ${ARGN})
    string(REPLACE ":" ";" parts ${spec})
    list(GET parts 0 target)
    list(GET parts 1 link)
    if(target MATCHES "^@(.*)")
      set(dir ${CMAKE_MATCH_1})
      # a shell loop: CMake lists break on file names like coreutils' "["
      install(CODE "
        set(root \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX})
        file(MAKE_DIRECTORY \${root}${link})
        execute_process(COMMAND sh -c \"for f in \\\"\$1\\\"/*; do ln -sfn ${prefix}/${dir}/\\\${f##*/} \\\"\$2\\\"/; done\"
                        sh \${root}${prefix}/${dir} \${root}${link} COMMAND_ERROR_IS_FATAL ANY)")
    else()
      get_filename_component(linkdir ${link} DIRECTORY)
      install(CODE "
        set(root \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX})
        file(MAKE_DIRECTORY \${root}${linkdir})
        file(CREATE_LINK ${prefix}/${target} \${root}${link} SYMBOLIC)")
    endif()
  endforeach()
endfunction()

function(syllable_port name)
  cmake_parse_arguments(A "SYSROOT" "PORT;URL;SHA256;PREFIX;CACHE;STYLE;DEST"
                        "CONFIGURE;MAKE_ARGS;LINKS;DEPENDS;BYPRODUCTS" ${ARGN})
  set(dest ${CMAKE_BINARY_DIR}/ports/${name}/dest)
  set(cache /dev/null)
  if(A_CACHE)
    set(cache ${A_CACHE})
  endif()
  if(A_DEST STREQUAL "sysroot")
    set(A_PREFIX ${SYLLABLE_STAGING})
    set(destdir "")
    if(NOT A_STYLE STREQUAL "openssl")       # autoconf's options; OpenSSL's Configure has its own
      list(APPEND A_CONFIGURE --disable-shared --enable-static)
    endif()
  else()
    set(destdir ${dest})
  endif()
  # --unset=TARGET: the build image exports TARGET=i586-pc-syllable, which
  # Makefiles that use their own TARGET variable (ICU) pick up
  set(env ${CMAKE_COMMAND} -E env --unset=TARGET PATH=${_cross_bin}:${SYLLABLE_STAGING}/bin:$ENV{PATH}
      CC=i586-pc-syllable-gcc CXX=i586-pc-syllable-g++ AR=i586-pc-syllable-ar
      RANLIB=i586-pc-syllable-ranlib STRIP=i586-pc-syllable-strip CONFIG_SITE=${cache}
      CPPFLAGS=-I${SYLLABLE_STAGING}/include LDFLAGS=-L${SYLLABLE_STAGING}/lib)
  if(A_STYLE STREQUAL "zlib")
    set(configure ${env} CFLAGS=-O2 <SOURCE_DIR>/configure --prefix=${A_PREFIX} ${A_CONFIGURE})
    set(install ${env} make install prefix=${destdir}${A_PREFIX})
    set(in_source BUILD_IN_SOURCE ON)
  elseif(A_STYLE STREQUAL "openssl")
    # Configure takes the compiler from CC; no-asm: its x86 assembler is ELF/Linux
    set(configure ${env} perl <SOURCE_DIR>/Configure linux-elf no-shared no-asm no-threads
                  --prefix=${A_PREFIX} --openssldir=${A_PREFIX}/ssl ${A_CONFIGURE})
    set(install ${env} make install_sw INSTALL_PREFIX=${destdir})
    set(in_source BUILD_IN_SOURCE ON)
    # -j1: OpenSSL 0.9.8's Makefiles are not parallel-safe.  EX_LIBS empty: its
    # programs would link -ldl, which is not in the sysroot; only the static
    # libraries are used, and their users link -ldl themselves.
    set(A_MAKE_ARGS -j1 EX_LIBS= build_libs ${A_MAKE_ARGS})
    set(install ${CMAKE_COMMAND} -E make_directory ${destdir}${A_PREFIX}/lib ${destdir}${A_PREFIX}/include/openssl
        COMMAND ${CMAKE_COMMAND} -E copy libssl.a libcrypto.a ${destdir}${A_PREFIX}/lib
        COMMAND sh -c "cp -L include/openssl/*.h $0/include/openssl/" ${destdir}${A_PREFIX})
  elseif(A_STYLE STREQUAL "icu")
    # ICU 3.6 cannot fully cross-build: its data step runs the target's own
    # tools.  So a host ICU is built first (tools and data), only the libraries
    # (common, stubdata) are cross-built, and the host's data archive becomes
    # libsicudata.a through genccode (an assembler file of the data).
    set(host <BINARY_DIR>/host)
    set(hostenv env -u TARGET LD_LIBRARY_PATH=${host}/lib)
    set(configure
      ${CMAKE_COMMAND} -E make_directory ${host}
      COMMAND ${CMAKE_COMMAND} -E chdir ${host} env -u TARGET CC=/usr/bin/gcc CXX=/usr/bin/g++
              CFLAGS=-O1 "CXXFLAGS=-O1 -std=gnu++98 -Wno-narrowing" sh <SOURCE_DIR>/source/configure
              --disable-threads --disable-extras --disable-samples --disable-tests
      COMMAND ${CMAKE_COMMAND} -E chdir ${host} env -u TARGET make -j1    # its data Makefile races in parallel
      COMMAND ${env} sh <SOURCE_DIR>/source/configure --build=x86_64-pc-linux-gnu
              --host=i586-pc-syllable --prefix=${A_PREFIX} --with-cross-build=${host}
              ${A_CONFIGURE})
    set(A_BUILD_COMMAND
      ${CMAKE_COMMAND} -E make_directory <BINARY_DIR>/lib <BINARY_DIR>/bin   # the top-level make would
      COMMAND ${env} make -j${SYLLABLE_JOBS} -C common
      COMMAND ${env} make -C stubdata
      COMMAND ${hostenv} ${host}/bin/genccode -a gcc -e icudt36 -d <BINARY_DIR>
              <SOURCE_DIR>/source/data/in/icudt36l.dat   # the full data, as shipped
      COMMAND ${env} i586-pc-syllable-gcc -c <BINARY_DIR>/icudt36l_dat.s -o <BINARY_DIR>/icudt36l_dat.o
      COMMAND ${env} i586-pc-syllable-ar rcs <BINARY_DIR>/lib/libsicudata.a <BINARY_DIR>/icudt36l_dat.o)
    set(install ${env} make -C common install DESTDIR=${destdir}
        COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/lib/libsicudata.a ${destdir}${A_PREFIX}/lib)
    set(in_source "")
  else()
    set(configure ${env} <SOURCE_DIR>/configure --build=x86_64-pc-linux-gnu
                  --host=i586-pc-syllable --prefix=${A_PREFIX} --disable-nls ${A_CONFIGURE})
    set(install ${env} make install DESTDIR=${destdir} ${A_MAKE_ARGS})
    set(in_source "")
  endif()
  if(A_BUILD_COMMAND)
    set(build ${A_BUILD_COMMAND})
  else()
    set(build ${env} make -j${SYLLABLE_JOBS} ${A_MAKE_ARGS})
  endif()
  set(after_install "")
  if(NOT A_DEST STREQUAL "sysroot")
    list(APPEND after_install
      COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${dest})
  endif()
  if(A_SYSROOT)
    # headers and libraries for later components to compile and link against
    list(APPEND after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${dest}${A_PREFIX}/include ${SYLLABLE_STAGING}/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${dest}${A_PREFIX}/lib ${SYLLABLE_STAGING}/lib)
  endif()
  ExternalProject_Add(${name}
    PREFIX ${CMAKE_BINARY_DIR}/ports/${name}
    URL ${A_URL} URL_HASH SHA256=${A_SHA256}
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    DEPENDS ${A_DEPENDS}
    BUILD_BYPRODUCTS ${A_BYPRODUCTS}
    ${in_source}
    PATCH_COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/apply-port.sh <SOURCE_DIR> ${SYLLABLE_BUILDER_PORTS}/${A_PORT}
    CONFIGURE_COMMAND ${configure}
    BUILD_COMMAND ${build}
    INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${dest}
            COMMAND ${install}
            ${after_install}
    LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
  if(NOT A_DEST STREQUAL "sysroot")
    install(DIRECTORY ${dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
    _syllable_links(${A_PREFIX} ${A_LINKS})
  endif()
endfunction()

# glibc 2.7, built by the toolchain into its sysroot, installed in the Syllable
# layout (/system/resources/glibc/2.7) without headers, static libraries or
# debug info.  The kernel's loader searches /system/libraries first, so each
# shared library is linked there, as Syllable's package index does.
set(SYLLABLE_GLIBC_PREFIX system/resources/glibc/2.7)
set(_glibc_src ${SYLLABLE_SYSROOT}/${SYLLABLE_GLIBC_PREFIX})
set(_glibc_dest ${CMAKE_BINARY_DIR}/ports/glibc/dest)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ports/glibc/stamp
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${_glibc_dest}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_glibc_dest}
  COMMAND rsync -a --exclude include --exclude "*.a" --exclude "*.o" ${_glibc_src}/ ${_glibc_dest}/
  COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_glibc_dest}
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/ports/glibc/stamp
  COMMENT "Preparing glibc 2.7")
add_custom_target(glibc ALL DEPENDS ${CMAKE_BINARY_DIR}/ports/glibc/stamp)
install(DIRECTORY ${_glibc_dest}/ DESTINATION ${SYLLABLE_GLIBC_PREFIX} USE_SOURCE_PERMISSIONS)
file(GLOB _glibc_libs RELATIVE ${_glibc_src}/lib ${_glibc_src}/lib/lib*.so.*)
foreach(lib ${_glibc_libs})
  install(CODE "file(CREATE_LINK /boot/${SYLLABLE_GLIBC_PREFIX}/lib/${lib}
    \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/libraries/${lib} SYMBOLIC)")
endforeach()

# The GCC runtime programs are linked against (libgcc_s, libstdc++), from the
# compiler that built the system, where the release keeps it
# (/system/resources/gcc/4.1.2/lib), linked from /system/libraries.
set(SYLLABLE_GCCRT_PREFIX system/resources/gcc/${SYLLABLE_COMPILER}/lib)
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=libgcc_s.so
                OUTPUT_VARIABLE _libgcc_s OUTPUT_STRIP_TRAILING_WHITESPACE)
get_filename_component(_gccrt_dir ${_libgcc_s} REALPATH)
get_filename_component(_gccrt_dir ${_gccrt_dir} DIRECTORY)
file(GLOB _gccrt_files ${_gccrt_dir}/libgcc_s.so* ${_gccrt_dir}/libstdc++.so*)
set(_gccrt_dest ${CMAKE_BINARY_DIR}/ports/gccrt/dest)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ports/gccrt/stamp
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${_gccrt_dest}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_gccrt_dest}
  COMMAND cp -a ${_gccrt_files} ${_gccrt_dest}/
  COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_gccrt_dest}
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/ports/gccrt/stamp
  DEPENDS ${_gccrt_files}
  COMMENT "Preparing the GCC ${SYLLABLE_COMPILER} runtime")
add_custom_target(gccrt ALL DEPENDS ${CMAKE_BINARY_DIR}/ports/gccrt/stamp)
install(DIRECTORY ${_gccrt_dest}/ DESTINATION ${SYLLABLE_GCCRT_PREFIX} USE_SOURCE_PERMISSIONS)
foreach(lib libgcc_s.so.3 libstdc++.so.6)
  if(EXISTS ${_gccrt_dir}/${lib})
    install(CODE "file(CREATE_LINK /boot/${SYLLABLE_GCCRT_PREFIX}/${lib}
      \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/libraries/${lib} SYMBOLIC)")
  endif()
endforeach()
