# Webster: the WebKit browser (Applications/Webster, with its libraries in
# Applications/Webster/lib, as the release has it).
#
# WebKit generates sources into its own tree and has its own Makefiles
# (Makefile.syllable), so the tree is copied into the build directory and built
# there with them, using the cross compilers: JavaScriptCore, WebCore, WebView,
# then the browser (ABrowse_NG).  Its libraries are the static ports (ICU,
# libxml2, SQLite, cURL, OpenSSL) plus the tree's libsyllable, libpthread, libdl.
set(_web ${CMAKE_CURRENT_BINARY_DIR}/webster)
set(_lib ${CMAKE_CURRENT_BINARY_DIR}/lib)     # -l names the Makefiles use
set(_bin ${CMAKE_CURRENT_BINARY_DIR}/bin)     # g++/gcc/strip they call by name
set(_cross ${_cross_bin}/i586-pc-syllable)
file(MAKE_DIRECTORY ${_lib} ${_bin})
foreach(tool gcc g++ strip)
  set(real ${_cross}-${tool})
  set(flags "")
  if(NOT tool STREQUAL "strip")         # the libraries' directories, as in CC/CXX
    set(flags "-L${_lib} -L${SYLLABLE_STAGING}/lib ")
  endif()
  file(WRITE ${_bin}/${tool} "#!/bin/sh\nexec ${real} ${flags}\"$@\"\n")
  file(CHMOD ${_bin}/${tool} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach()

set(_env ${CMAKE_COMMAND} -E env --unset=TARGET
  PATH=${_bin}:${SYLLABLE_HOST_DIR}/bin:${SYLLABLE_TOOL_PATH}:${_cross_bin}:$ENV{PATH}
  "CC=${_cross}-gcc -L${_lib} -L${SYLLABLE_STAGING}/lib"
  "CXX=${_cross}-g++ -L${_lib} -L${SYLLABLE_STAGING}/lib"
  CPLUS_INCLUDE_PATH=${SYLLABLE_HEADERS}:${SYLLABLE_STAGING}/include:${SYLLABLE_STAGING}/include/libxml2
  C_INCLUDE_PATH=${SYLLABLE_HEADERS}:${SYLLABLE_STAGING}/include:${SYLLABLE_STAGING}/include/libxml2
  "CFLAGS=-include stddef.h")    # gperf 3.1's tables use size_t (Webster had gperf 3.0)

ExternalProject_Add(webster
  SOURCE_DIR ${_web}
  # rsync (the tree has symbolic links, which copy_directory cannot copy), on
  # every build so edits reach the copy; it keeps mtimes, so make rebuilds only
  # what changed.  Note: generated sources are made only when DerivedSources is
  # missing; after editing a grammar or .in file, delete DerivedSources there.
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND rsync -a --exclude CMakeLists.txt --exclude objs/ --exclude DerivedSources/
                 ${CMAKE_CURRENT_SOURCE_DIR}/ ${_web}/
  BUILD_IN_SOURCE ON
  DEPENDS icu libxml2 sqlite curl openssl libpng libjpeg zlib libsyllable libpthread libdl hosttools
  CONFIGURE_COMMAND
           # the tree's libraries under the names the Makefiles link
           ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:libsyllable> ${_lib}/libsyllable.so
   COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:libpthread> ${_lib}/libpthread.so
   COMMAND ${CMAKE_COMMAND} -E create_symlink ${SYLLABLE_STAGING}/lib/libsicudata.a ${_lib}/libicudata.a
  # Generated sources first, in their own make runs: generated in the same run,
  # make has already chosen its built-in yacc rule (grammar.y -> grammar.c, as
  # C) before bison writes the .cpp.  -r: only Webster's own rules.
  BUILD_COMMAND ${_env} make -r -C JavaScriptCore -f Makefile.syllable gensources
        COMMAND ${_env} make -r -j8 -C JavaScriptCore -f Makefile.syllable
        COMMAND ${_env} make -r -C WebCore -f Makefile.syllable gensources
        COMMAND ${_env} make -r -j8 -C WebCore -f Makefile.syllable
        COMMAND ${_env} make -r -j8 -C WebView
        COMMAND ${_env} make -r -j8 -C ABrowse_NG
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${_web}/ABrowse_NG/Webster
  LOG_BUILD ON LOG_OUTPUT_ON_FAILURE ON)

install(PROGRAMS ${_web}/ABrowse_NG/Webster DESTINATION applications/Webster)
install(PROGRAMS ${_web}/JavaScriptCore/objs/libjavascriptcore.so.1 ${_web}/WebCore/objs/libwebcore.so.1
                 ${_web}/WebView/libwebview.so.1
        DESTINATION applications/Webster/lib)
syllable_attribute(applications/Webster/Webster os::Category Internet)
