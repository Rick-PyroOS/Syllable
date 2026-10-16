# Syllable's own build tools, built as 32-bit Linux programs for the build
# machine (the main build runs them):
#
#   rescopy     system/apps/utils/rescopy      list/add/extract executable resources
#   catcomp     system/apps/utils/catalog      compile .cd string catalogs
#   addattrib   system/apps/utils/fsutils      set a file attribute (user.* xattr)
#   keymapconv  system/apps/utils/keymapconv   compile .map keymaps
#
# They are compiled unmodified (catcomp gets patches/catcomp-host.patch) with
# the libsyllable sources they need.  include/ shadows the Syllable headers
# that clash with glibc or need the kernel; shim.cpp implements the Syllable
# calls on Linux (attributes as user.* xattrs).  Syllable's include tree comes
# after glibc's (-idirafter), so glibc wins where both have a header.
#
# Built by the top-level project as an external project with the host
# compiler; it can also be built on its own (needs gcc-multilib g++-multilib).

cmake_minimum_required(VERSION 3.20)
project(SyllableHostTools C CXX)

get_filename_component(SYLLABLE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(SYS ${SYLLABLE_SOURCE_DIR}/system)
set(LIBSYL ${SYS}/sys/appserver/libsyllable)
set(UTILS ${SYS}/apps/utils)

find_program(PATCH patch REQUIRED)

# 32-bit, as Syllable's on-disk formats (resources, catalogs) assume i386 layout
add_compile_options(-m32 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE)
add_link_options(-m32)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-idirafter ${SYS}/sys/include/syllable)

# Syllable's code: warnings off (it is not ours); include/host-prelude.h
# supplies the <string.h> Syllable's libc headers would have pulled in.
set(SYL_FLAGS -w -fno-strict-aliasing -include ${CMAKE_CURRENT_SOURCE_DIR}/include/host-prelude.h)

add_library(syllable-host STATIC
  ${LIBSYL}/util/string.cpp ${LIBSYL}/util/optionparser.cpp
  ${LIBSYL}/util/resources.cpp ${LIBSYL}/util/catalog.cpp
  ${LIBSYL}/storage/file.cpp ${LIBSYL}/storage/fsnode.cpp ${LIBSYL}/storage/path.cpp
  ${LIBSYL}/storage/tempfile.cpp ${LIBSYL}/storage/seekableio.cpp
  ${LIBSYL}/storage/streamableio.cpp
  shim.cpp stubs.cpp)
set_source_files_properties(shim.cpp stubs.cpp PROPERTIES COMPILE_OPTIONS "-Wall;-Wno-unused-parameter")
foreach(src string optionparser resources catalog)
  set_source_files_properties(${LIBSYL}/util/${src}.cpp PROPERTIES COMPILE_OPTIONS "${SYL_FLAGS}")
endforeach()
foreach(src file fsnode path tempfile seekableio streamableio)
  set_source_files_properties(${LIBSYL}/storage/${src}.cpp PROPERTIES COMPILE_OPTIONS "${SYL_FLAGS}")
endforeach()
target_link_libraries(syllable-host PUBLIC pthread)

function(host_tool name)
  add_executable(${name} ${ARGN})
  target_compile_options(${name} PRIVATE ${SYL_FLAGS})
  target_link_libraries(${name} PRIVATE syllable-host)
  install(TARGETS ${name} DESTINATION bin)
endfunction()

host_tool(rescopy ${UTILS}/rescopy/rescopy.cpp)
host_tool(keymapconv ${UTILS}/keymapconv/keymapconv.cpp)

host_tool(addattrib ${UTILS}/fsutils/addattrib.c)
target_compile_definitions(addattrib PRIVATE O_NOTRAVERSE=0)
set_target_properties(addattrib PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)

# catcomp: a patched copy (built-in English strings; opens its output for
# writing), compiled next to the original's headers
set(CATCOMP_SRC ${CMAKE_CURRENT_BINARY_DIR}/catcomp.cpp)
add_custom_command(OUTPUT ${CATCOMP_SRC}
  COMMAND ${CMAKE_COMMAND} -E copy ${UTILS}/catalog/catcomp.cpp ${CATCOMP_SRC}
  COMMAND ${PATCH} --no-backup-if-mismatch -s ${CATCOMP_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/patches/catcomp-host.patch
  DEPENDS ${UTILS}/catalog/catcomp.cpp ${CMAKE_CURRENT_SOURCE_DIR}/patches/catcomp-host.patch
  COMMENT "Patching catcomp for the build machine")
host_tool(catcomp ${CATCOMP_SRC} ${UTILS}/catalog/ccatalog.cpp)
target_include_directories(catcomp PRIVATE ${UTILS}/catalog)
