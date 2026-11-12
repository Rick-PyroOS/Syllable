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

# The version of Syllable (Pyro): "version" in package.json, MAJOR.MINOR.PATCH
# with an optional -SUFFIX (0.0.1, 0.1.0-beta1).  SYLLABLE_VERSION is all of
# it, PROJECT_VERSION its numbers.  Editing package.json reconfigures.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/package.json SYLLABLE_PACKAGE_JSON)
string(JSON SYLLABLE_VERSION ERROR_VARIABLE version_error GET "${SYLLABLE_PACKAGE_JSON}" version)
if(version_error OR NOT SYLLABLE_VERSION MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-[A-Za-z0-9.]+)?$")
  message(FATAL_ERROR "package.json: \"version\" must be MAJOR.MINOR.PATCH[-SUFFIX], not '${SYLLABLE_VERSION}' ${version_error}")
endif()
set(syllable_version_numbers ${CMAKE_MATCH_1})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package.json)

project(Syllable VERSION ${syllable_version_numbers} LANGUAGES C CXX)
message(STATUS "Syllable (Pyro) ${SYLLABLE_VERSION}")

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
add_dependencies(perl libdl)                        # DynaLoader (cmake/ports/10-compilers.cmake)
add_dependencies(doxygen libpthread)                # QThread (cmake/ports/10-compilers.cmake)
add_dependencies(git libpthread)                    # threads (cmake/ports/50-git.cmake)

# Layout, configuration, headers and boot scripts
include(Layout)
include(Documentation)                              # Doxygen's developer documentation
add_subdirectory(system/sys/include/syllable)
add_subdirectory(system/other/config/etc)
add_subdirectory(system/other/config/default)
add_subdirectory(system/image)                      # etc, home, config, boot scripts

# The libraries, for programs built outside the tree (./sb compile)
include(SyllableSDK)

# ---- Disk image ------------------------------------------------------------------
include(Image)
