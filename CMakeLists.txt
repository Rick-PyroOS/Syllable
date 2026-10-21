# Syllable (Pyro)
#
# Configure and build inside the Syllable-Build-System os image (the cross
# toolchain lives there):
#
#   cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/syllable-i586.cmake
#   cmake --build build
#   cmake --install build          # into build/stage, the boot volume's tree
#
# The build never writes into the source tree.  See
# Syllable-Build-System/docs/cmake-plan.md for the plan this follows.

cmake_minimum_required(VERSION 3.20)
project(Syllable LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Syllable")
  message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/syllable-i586.cmake")
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(ExternalProject)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/stage CACHE PATH "Boot volume tree" FORCE)
endif()

# ---- Host tools: built first, for the build machine ---------------------------

set(SYLLABLE_HOST_DIR ${CMAKE_BINARY_DIR}/host)
ExternalProject_Add(hosttools
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/tools/host
  BINARY_DIR ${CMAKE_BINARY_DIR}/host-build
  INSTALL_DIR ${SYLLABLE_HOST_DIR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
             -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++
             -DCMAKE_BUILD_TYPE=Release
  CMAKE_GENERATOR ${CMAKE_GENERATOR}
  BUILD_ALWAYS ON          # its own build is incremental; this just re-checks it
  BUILD_BYPRODUCTS ${SYLLABLE_HOST_DIR}/bin/rescopy ${SYLLABLE_HOST_DIR}/bin/catcomp
                   ${SYLLABLE_HOST_DIR}/bin/addattrib ${SYLLABLE_HOST_DIR}/bin/keymapconv
                   ${SYLLABLE_HOST_DIR}/bin/bintohex)
foreach(tool rescopy catcomp addattrib keymapconv bintohex)
  string(TOUPPER ${tool} T)
  set(SYLLABLE_${T} ${SYLLABLE_HOST_DIR}/bin/${tool})
endforeach()

include(Syllable)                                   # helpers (use the host tools above)

# ---- Components ------------------------------------------------------------------
# Ports first: later components compile against what they stage
include(SyllablePorts)                              # glibc layout + syllable_port()
include(Ports)                                      # bash, coreutils, zlib, FreeType, image libraries

add_subdirectory(system)                            # everything else, area by area

# Ports that need a component of the tree (defined after the ports)
add_dependencies(curl libdl)                        # static libcrypto's dlopen()

# Layout, configuration, headers and boot scripts
include(Layout)
add_subdirectory(system/sys/include/syllable)
add_subdirectory(system/other/config/etc)
add_subdirectory(system/other/config/default)
add_subdirectory(system/image)                      # etc, home, config, boot scripts

# ---- Disk image ------------------------------------------------------------------
include(Image)
