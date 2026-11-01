# Xpdf (applications/Xpdf/Xpdf): an autoconf project with a Syllable GUI.
# Its Makefiles build in the source tree and add resources from it, so the
# source is copied into the build tree, configured and built there with the
# host rescopy on PATH.  Only the GUI (make xpdf) is built, as the Builder
# profile does.
set(_xpdf ${CMAKE_CURRENT_BINARY_DIR}/xpdf-build)
set(_env ${CMAKE_COMMAND} -E env
  PATH=${SYLLABLE_HOST_DIR}/bin:${SYLLABLE_TOOL_PATH}:${_cross_bin}:$ENV{PATH}
  CC=i586-pc-syllable-gcc CXX=i586-pc-syllable-g++ AR=i586-pc-syllable-ar
  RANLIB=i586-pc-syllable-ranlib
  CPLUS_INCLUDE_PATH=${SYLLABLE_HEADERS}:${SYLLABLE_STAGING}/include:${SYLLABLE_STAGING}/include/freetype2
  C_INCLUDE_PATH=${SYLLABLE_HEADERS}:${SYLLABLE_STAGING}/include
  CPPFLAGS=-I${SYLLABLE_STAGING}/include
  "LDFLAGS=-L${SYLLABLE_STAGING}/lib -L${CMAKE_CURRENT_BINARY_DIR}/lib")
ExternalProject_Add(xpdf
  SOURCE_DIR ${_xpdf}
  DOWNLOAD_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_xpdf}
           COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${_xpdf}
           COMMAND ${CMAKE_COMMAND} -E rm -f ${_xpdf}/CMakeLists.txt
           # -lsyllable needs a libsyllable.so; the build tree has only the soname
           COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/lib
           COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:libsyllable>
                   ${CMAKE_CURRENT_BINARY_DIR}/lib/libsyllable.so
  BUILD_IN_SOURCE ON
  DEPENDS freetype zlib libsyllable hosttools
  CONFIGURE_COMMAND ${_env} sh ./configure --build=x86_64-pc-linux-gnu --host=i586-pc-syllable
                    --with-freetype2-includes=${SYLLABLE_STAGING}/include/freetype2
                    --with-freetype2-library=${SYLLABLE_STAGING}/lib
  BUILD_COMMAND ${_env} make xpdf
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${_xpdf}/xpdf/Xpdf
  LOG_CONFIGURE ON LOG_BUILD ON LOG_OUTPUT_ON_FAILURE ON)
install(PROGRAMS ${_xpdf}/xpdf/Xpdf DESTINATION applications/Xpdf)
