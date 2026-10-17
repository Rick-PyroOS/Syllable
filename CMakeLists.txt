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
# They move over from their Makefiles component by component.

# Kernel and the drivers needed to boot from an ATA disk
add_subdirectory(system/sys/kernel/kernel)
add_subdirectory(system/sys/kernel/drivers/bus/pci)
add_subdirectory(system/sys/kernel/drivers/bus/ata)
add_subdirectory(system/sys/kernel/drivers/hcd/ata_pci)
add_subdirectory(system/sys/kernel/drivers/disk/bios)
add_subdirectory(system/sys/kernel/drivers/ps2)
add_subdirectory(system/sys/kernel/fs/afs)

# Libraries
include(SyllablePorts)                              # glibc layout + syllable_port()
add_subdirectory(system/libs/libdl)
add_subdirectory(system/libs/libpthread)

# Userland
add_subdirectory(system/apps/utils/systools)        # init (the rest needs libsyllable)
include(Ports)                                      # bash, coreutils

# The desktop
add_subdirectory(system/sys/appserver/libsyllable)
add_subdirectory(system/sys/appserver/appserver/server)
add_subdirectory(system/sys/appserver/appserver/drivers/input/keyboard)
add_subdirectory(system/sys/appserver/appserver/drivers/input/ps2mouse)
add_subdirectory(system/sys/appserver/appserver/drivers/decorators/photon)
add_subdirectory(system/sys/appserver/registrar)
add_subdirectory(system/sys/translators/png)
add_subdirectory(system/apps/desktop/desktop)
add_subdirectory(system/apps/desktop/dock)
add_subdirectory(system/apps/utils/aterm)
add_subdirectory(system/apps/utils/dlogin)
add_subdirectory(system/other/fonts)
add_subdirectory(system/other/keymaps)
add_subdirectory(system/other/icons)

# Layout, configuration, headers and boot scripts
include(Layout)
add_subdirectory(system/sys/include/syllable)
add_subdirectory(system/other/init)
add_subdirectory(system/other/config/etc)
add_subdirectory(system/other/config/default)

# ---- Disk image ------------------------------------------------------------------
include(Image)
