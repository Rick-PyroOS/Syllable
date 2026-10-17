# Third-party software: Builder ports (Builder/packages/<PORT>) cross-built
# with their own configure scripts, and glibc from the toolchain's sysroot.
#
#   syllable_port(NAME PORT dir URL url SHA256 hash PREFIX /system/resources/...
#                 [CONFIGURE args...] [CACHE file] [MAKE_ARGS args...]
#                 [LINKS target:link ...])
#       Downloads and checks the source, applies the port, configures for
#       i586-pc-syllable with PREFIX, builds, installs into the build tree and
#       strips it; `cmake --install` copies it into the boot volume.
#       CACHE is a CONFIG_SITE file answering the configure checks that cannot
#       run when cross compiling.  LINKS: TARGET (relative to PREFIX) to LINK
#       (absolute); @DIR:/LINKDIR links every file in PREFIX/DIR.

set(SYLLABLE_BUILDER_PORTS ${PROJECT_SOURCE_DIR}/system/apps/utils/Builder/packages)
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
  cmake_parse_arguments(A "" "PORT;URL;SHA256;PREFIX;CACHE" "CONFIGURE;MAKE_ARGS;LINKS" ${ARGN})
  set(dest ${CMAKE_BINARY_DIR}/ports/${name}/dest)
  set(cache /dev/null)
  if(A_CACHE)
    set(cache ${A_CACHE})
  endif()
  set(env ${CMAKE_COMMAND} -E env PATH=${_cross_bin}:$ENV{PATH}
      CC=i586-pc-syllable-gcc CXX=i586-pc-syllable-g++ AR=i586-pc-syllable-ar
      RANLIB=i586-pc-syllable-ranlib STRIP=i586-pc-syllable-strip CONFIG_SITE=${cache})
  ExternalProject_Add(${name}
    PREFIX ${CMAKE_BINARY_DIR}/ports/${name}
    URL ${A_URL} URL_HASH SHA256=${A_SHA256}
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    PATCH_COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/apply-port.sh <SOURCE_DIR> ${SYLLABLE_BUILDER_PORTS}/${A_PORT}
    CONFIGURE_COMMAND ${env} <SOURCE_DIR>/configure --build=x86_64-pc-linux-gnu
                      --host=i586-pc-syllable --prefix=${A_PREFIX} --disable-nls ${A_CONFIGURE}
    BUILD_COMMAND ${env} make -j${SYLLABLE_JOBS} ${A_MAKE_ARGS}
    INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${dest}
            COMMAND ${env} make install DESTDIR=${dest} ${A_MAKE_ARGS}
            COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${dest}
    LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
  install(DIRECTORY ${dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
  _syllable_links(${A_PREFIX} ${A_LINKS})
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
