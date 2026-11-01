# A program that is not in the tree yet, built against a configured and built
# Syllable tree (Syllable-Build-System's ./sb compile runs this):
#
#   cmake -S cmake/sdk -B <dir> -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/syllable-i586.cmake
#         -DSYLLABLE_BUILD=<the tree's build> -DSYLLABLE_APP=<program folder or source file>
#
# A folder is added as it is: its CMakeLists.txt uses the tree's helpers
# (syllable_program(), syllable_resources()...) and libraries (libsyllable,
# syllable::png...).  A single source file becomes a program installed in
# usr/sb/bin, linked with libsyllable.  `cmake --install` puts the result in
# the install prefix as the boot volume would have it.

cmake_minimum_required(VERSION 3.20)
project(SyllableProgram LANGUAGES C CXX)

if(NOT SYLLABLE_BUILD OR NOT EXISTS ${SYLLABLE_BUILD}/sdk/targets.cmake)
  message(FATAL_ERROR "SYLLABLE_BUILD (${SYLLABLE_BUILD}) is not a built Syllable tree")
endif()
if(NOT SYLLABLE_APP)
  message(FATAL_ERROR "SYLLABLE_APP: the program's folder or source file")
endif()

get_filename_component(SYLLABLE_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH ${SYLLABLE_SOURCE_DIR}/cmake)

# The tree's settings: debug info as the tree was built
load_cache(${SYLLABLE_BUILD} READ_WITH_PREFIX _tree_ SYLLABLE_DEBUG_INFO SYLLABLE_JOBS)
set(SYLLABLE_DEBUG_INFO ${_tree_SYLLABLE_DEBUG_INFO} CACHE BOOL "" FORCE)
# SYLLABLE_SDK_DEBUG (sb compile --debug): debug info whatever the tree has, and
# no optimisation (below, after the program's own options)
option(SYLLABLE_SDK_DEBUG "The program built for debugging: -O0, debug info, not stripped" OFF)
if(SYLLABLE_SDK_DEBUG)
  set(SYLLABLE_DEBUG_INFO ON CACHE BOOL "" FORCE)
endif()

# The host tools and staging sysroot the tree built
set(SYLLABLE_HOST_DIR ${SYLLABLE_BUILD}/host)
foreach(tool rescopy catcomp addattrib keymapconv bintohex)
  string(TOUPPER ${tool} T)
  set(SYLLABLE_${T} ${SYLLABLE_HOST_DIR}/bin/${tool})
endforeach()
set(SYLLABLE_STAGING ${SYLLABLE_BUILD}/sysroot/usr)
set(SYLLABLE_BUILDER_PORTS ${SYLLABLE_SOURCE_DIR}/system/apps/utils/Builder/packages)
# the tree's helpers depend on these targets, which are already built there
add_custom_target(hosttools)
add_custom_target(libkernel)

include(Syllable)
set(SYLLABLE_KERNEL_LIBDIR ${SYLLABLE_BUILD}/system/sys/kernel/kernel)
# a folder that is also in the tree defines its own libraries again
set(SYLLABLE_SDK_OWN)
if(IS_DIRECTORY ${SYLLABLE_APP})
  get_filename_component(SYLLABLE_SDK_OWN ${SYLLABLE_APP} ABSOLUTE)
endif()
include(${SYLLABLE_BUILD}/sdk/targets.cmake)
# libraries the tree's libraries need, when the linker follows them
link_directories(${SYLLABLE_BUILD}/stage/system/libraries)
add_link_options(-Wl,-rpath-link=${SYLLABLE_BUILD}/stage/system/libraries)

if(IS_DIRECTORY ${SYLLABLE_APP})
  add_subdirectory(${SYLLABLE_APP} app)
else()
  get_filename_component(_name ${SYLLABLE_APP} NAME_WE)
  syllable_program(${_name} SOURCES ${SYLLABLE_APP} LIBS libsyllable INSTALL usr/sb/bin)
endif()

# --debug: -O0 last on every target the program's folder (and its subfolders)
# made, so it wins over their own -O2
if(SYLLABLE_SDK_DEBUG)
  function(_sdk_debug dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(t ${targets})
      get_target_property(type ${t} TYPE)
      if(type MATCHES "EXECUTABLE|LIBRARY")
        target_compile_options(${t} PRIVATE -O0 -fno-omit-frame-pointer)
      endif()
    endforeach()
    get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(d ${subdirs})
      _sdk_debug(${d})
    endforeach()
  endfunction()
  _sdk_debug(${CMAKE_CURRENT_SOURCE_DIR})
endif()
