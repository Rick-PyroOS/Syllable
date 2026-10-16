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
include(Syllable)

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
                   ${SYLLABLE_HOST_DIR}/bin/addattrib ${SYLLABLE_HOST_DIR}/bin/keymapconv)
foreach(tool rescopy catcomp addattrib keymapconv)
  string(TOUPPER ${tool} T)
  set(SYLLABLE_${T} ${SYLLABLE_HOST_DIR}/bin/${tool})
endforeach()

# ---- Components ------------------------------------------------------------------
# Phase 1 samples; the rest moves over component by component.

add_subdirectory(system/sys/kernel/kernel)          # libkernel.so (kernel.so: phase 2)
add_subdirectory(system/sys/kernel/drivers/ps2)
add_subdirectory(system/apps/utils/systools)        # init (the rest needs libsyllable)
