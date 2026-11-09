# git 2.47.0, in /usr/git/2.47.0, linked from /usr/index/bin.
#
# git has no configure step to cross-build with: its Makefile takes every
# setting as a make variable.  The Syllable ones are in cmake/ports/git.mak
# (copied in as config.mak); the build tree's paths are given on make's
# command line.  Built with GCC 9.5.0 (git needs C99), against the system's
# cURL 8 and OpenSSL 3 (cmake/ports/20-ssl.cmake), zlib 1.3.1 and the tree's
# libpthread, as those ports are.  Syllable-Ports/git-2.47 builds the same
# outside the tree.

set(SYLLABLE_GIT_PREFIX /usr/git/2.47.0)
set(_git_dest ${CMAKE_BINARY_DIR}/ports/git/dest)
set(_git_pthread ${CMAKE_BINARY_DIR}/ports/git/pthread)
set(_git_env ${CMAKE_COMMAND} -E env --unset=TARGET PATH=${SYLLABLE_TOOLCHAIN_ROOT}/bin:$ENV{PATH})
# uname_*: on the command line, so that git's config.mak.uname (read before
# config.mak) matches no system's block, not even the build host's Linux
set(_git_make make prefix=${SYLLABLE_GIT_PREFIX}
    uname_S=Syllable uname_M=i586 uname_O=Syllable uname_R=0.6.7 uname_P=i586 uname_V=0
    CURLDIR=${CMAKE_BINARY_DIR}/ports/curl8/dest/usr/curl/8.23.0
    OPENSSLDIR=${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}
    ZLIB_PATH=${CMAKE_BINARY_DIR}/ports/zlib/dest/system/resources/zlib/1.3.1
    PTHREAD_LIBS=${_git_pthread}/libpthread.so)
ExternalProject_Add(git
  PREFIX ${CMAKE_BINARY_DIR}/ports/git
  URL https://www.kernel.org/pub/software/scm/git/git-2.47.0.tar.gz
  URL_HASH SHA256=a84a7917e0ab608312834413f01fc01edc7844f9f9002ba69f3b4f4bcb8d937a
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  BUILD_IN_SOURCE ON
  # -lpthread: a folder with only the tree's libpthread (not the staging
  # sysroot's other libraries)
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/git.mak <SOURCE_DIR>/config.mak
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_git_pthread}
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:libpthread> ${_git_pthread}/libpthread.so
  BUILD_COMMAND ${_git_env} ${_git_make} -j${SYLLABLE_JOBS} all
  INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_git_dest}
    COMMAND ${_git_env} ${_git_make} DESTDIR=${_git_dest} install
    COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_git_dest}
  DEPENDS curl8 openssl3 zlib
  LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
# a changed git.mak builds it again
ExternalProject_Add_StepDependencies(git configure ${CMAKE_CURRENT_LIST_DIR}/git.mak)
install(DIRECTORY ${_git_dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
_syllable_links(${SYLLABLE_GIT_PREFIX} bin/git:/usr/index/bin/git)
