# mandoc 1.14.6: man, apropos, whatis, makewhatis, mandoc, demandoc and
# soelim, in /usr/mandoc/1.14.6, linked from /usr/index/bin.
#
# mandoc's configure is its own (not autoconf): it reads configure.local and
# compiles and runs a test program for each check.  A cross build can't run
# them, so the port's patches/configure.local (Builder/packages/mandoc-1.14.6)
# answers every check for glibc 2.7, and names the compiler.  Linked against
# the system's zlib 1.3.1 (gzip-compressed pages).
#
# Finding the pages: the ports install them in their own folders
# (/usr/<package>/<version>/share/man, /system/resources/...), and
# /etc/profile's MANPATH searches /resources/index/manuals (= /usr/index/
# manuals).  After everything has installed, cmake/scripts/index-manuals.sh
# links every port's pages into /usr/index/manuals/manN and indexes them
# for apropos and whatis (mandoc.db) with the build host's makewhatis
# (mandoc_host, below): see the end of the top-level CMakeLists.txt.

set(SYLLABLE_MANDOC_PREFIX /usr/mandoc/1.14.6)
set(_mandoc_url https://mandoc.bsd.lv/snapshots/mandoc-1.14.6.tar.gz)
set(_mandoc_sha 8bf0d570f01e70a6e124884088870cbed7537f36328d512909eb10cd53179d9c)
set(_mandoc_port ${SYLLABLE_BUILDER_PORTS}/mandoc-1.14.6)
set(_mandoc_dest ${CMAKE_BINARY_DIR}/ports/mandoc/dest)

# The build host's mandoc, for makewhatis at install time.  mandoc is
# makewhatis when it's called by that name.
set(SYLLABLE_HOST_MAKEWHATIS ${CMAKE_BINARY_DIR}/ports/mandoc_host/src/mandoc_host/makewhatis)
ExternalProject_Add(mandoc_host
  PREFIX ${CMAKE_BINARY_DIR}/ports/mandoc_host
  URL ${_mandoc_url} URL_HASH SHA256=${_mandoc_sha}
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  BUILD_IN_SOURCE ON
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/mandoc-host.local <SOURCE_DIR>/configure.local
            COMMAND env -u TARGET ./configure
  BUILD_COMMAND env -u TARGET make -j${SYLLABLE_JOBS} mandoc
  INSTALL_COMMAND ${CMAKE_COMMAND} -E create_symlink mandoc <SOURCE_DIR>/makewhatis
  BUILD_BYPRODUCTS ${SYLLABLE_HOST_MAKEWHATIS}
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
ExternalProject_Add_StepDependencies(mandoc_host configure ${CMAKE_CURRENT_LIST_DIR}/mandoc-host.local)

set(_mandoc_env ${CMAKE_COMMAND} -E env --unset=TARGET
    PATH=${_cross_bin}:${SYLLABLE_STAGING}/bin:$ENV{PATH}
    SYLLABLE_STAGING=${SYLLABLE_STAGING})
ExternalProject_Add(mandoc
  PREFIX ${CMAKE_BINARY_DIR}/ports/mandoc
  URL ${_mandoc_url} URL_HASH SHA256=${_mandoc_sha}
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  BUILD_IN_SOURCE ON
  PATCH_COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/apply-port.sh <SOURCE_DIR> ${_mandoc_port}
  CONFIGURE_COMMAND ${_mandoc_env} ./configure
  BUILD_COMMAND ${_mandoc_env} make -j${SYLLABLE_JOBS}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_mandoc_dest}
          COMMAND ${_mandoc_env} make DESTDIR=${_mandoc_dest} install
          COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_mandoc_dest}
  # mandoc_host: so that `sb build mandoc` also builds what installing needs
  DEPENDS zlib mandoc_host
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
# a changed port (configure.local) patches and configures it again
file(GLOB_RECURSE _mandoc_port_files CONFIGURE_DEPENDS ${_mandoc_port}/*)
ExternalProject_Add_StepDependencies(mandoc patch ${_mandoc_port_files})
install(DIRECTORY ${_mandoc_dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
_syllable_links(${SYLLABLE_MANDOC_PREFIX} @bin:/usr/index/bin sbin/makewhatis:/usr/index/bin/makewhatis)

# coreutils' man pages (ls, cp, cat...).  The coreutils port builds only lib
# and src (cmake/Ports.cmake), because its man/ Makefile remakes the pages by
# running the built programs.  The release tarball already has them made
# (man/*.1), so they are installed from it as they are, into coreutils' own
# folder, where the index finds them.  (tar 1.22 has no man page at all.)
set(_coreutils_man ${CMAKE_BINARY_DIR}/ports/coreutils_man/src/coreutils_man/man)
ExternalProject_Add(coreutils_man
  PREFIX ${CMAKE_BINARY_DIR}/ports/coreutils_man
  URL https://mirrors.kernel.org/gnu/coreutils/coreutils-6.4.tar.bz2
      https://ftp.gnu.org/gnu/coreutils/coreutils-6.4.tar.bz2
  URL_HASH SHA256=75a6b82bb7e3d07bec5b9bd0fcdfdfdfb88f32728e5089972001b50e4148b87b
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")
add_dependencies(mandoc coreutils_man)
install(DIRECTORY ${_coreutils_man}/ DESTINATION system/resources/coreutils/6.4/share/man/man1
        FILES_MATCHING PATTERN "*.1")
