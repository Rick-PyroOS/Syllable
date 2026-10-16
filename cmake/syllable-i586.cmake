# CMake toolchain file: cross-compile Syllable (Pyro) for i586 with the
# compilers in the Syllable-Build-System toolchain image.
#
#   cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/syllable-i586.cmake
#
# SYLLABLE_COMPILER picks the compiler the system is built with:
#   4.1.2  GCC 4.1.2 + binutils 2.17, what Syllable 0.6.x was built with (default)
#   9.5.0  GCC 9.5.0 + binutils 2.35

set(CMAKE_SYSTEM_NAME Syllable)
set(CMAKE_SYSTEM_PROCESSOR i586)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

set(SYLLABLE_TOOLCHAIN_ROOT /opt/syllable CACHE PATH "Syllable cross toolchain")
set(SYLLABLE_COMPILER 4.1.2 CACHE STRING "GCC that builds the system (4.1.2 or 9.5.0)")
set_property(CACHE SYLLABLE_COMPILER PROPERTY STRINGS 4.1.2 9.5.0)

if(SYLLABLE_COMPILER STREQUAL "4.1.2")
  set(_bin ${SYLLABLE_TOOLCHAIN_ROOT}/gcc-4.1.2/bin)
elseif(SYLLABLE_COMPILER STREQUAL "9.5.0")
  set(_bin ${SYLLABLE_TOOLCHAIN_ROOT}/bin)
else()
  message(FATAL_ERROR "SYLLABLE_COMPILER must be 4.1.2 or 9.5.0, not ${SYLLABLE_COMPILER}")
endif()
set(_t ${_bin}/i586-pc-syllable)

set(CMAKE_C_COMPILER   ${_t}-gcc)
set(CMAKE_CXX_COMPILER ${_t}-g++)
set(CMAKE_ASM_COMPILER ${_t}-gcc)
set(CMAKE_AR           ${_t}-ar     CACHE FILEPATH "")
set(CMAKE_RANLIB       ${_t}-ranlib CACHE FILEPATH "")
set(CMAKE_STRIP        ${_t}-strip  CACHE FILEPATH "")
set(CMAKE_OBJCOPY      ${_t}-objcopy CACHE FILEPATH "")
set(CMAKE_LINKER       ${_t}-ld     CACHE FILEPATH "")

# glibc 2.7 and the kernel headers (the compilers default to this sysroot)
set(SYLLABLE_SYSROOT ${SYLLABLE_TOOLCHAIN_ROOT}/sysroot)
set(CMAKE_FIND_ROOT_PATH ${SYLLABLE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
