# Helpers for building Syllable components.  Each carries the flags the
# component's original Makefile used, so a CMakeLists.txt only lists sources,
# extra includes and where the result is installed (relative to the boot
# volume: system/drivers/dev/..., system/programs, ...).
#
#   syllable_driver(NAME SOURCES ... INSTALL dir [OUTPUT_NAME n] [INCLUDES ...]
#                   [DEFINES ...] [OPTIONS ...])
#       Kernel driver or filesystem: compiled and linked with -kernel
#       (Syllable's GCC: -D__KERNEL__, linked as a shared object against
#       libkernel.so).  Sees the kernel's header view (below).
#
#   syllable_program(NAME SOURCES ... INSTALL dir [OUTPUT_NAME n] [INCLUDES ...]
#                    [DEFINES ...] [OPTIONS ...] [LIBS ...])
#       Userland program.
#
# Header views.  Userland code includes the tree's headers as they are
# (system/sys/include/syllable).  Kernel code was written when <syllable/...>
# was the atheos/ headers (sys/include's own deps target did
# `ln -sf atheos syllable`); the tree now has a separate, newer syllable/ for
# userland, so kernel targets get a directory in which syllable -> atheos,
# searched first.

set(SYLLABLE_HEADERS ${PROJECT_SOURCE_DIR}/system/sys/include/syllable)
set(SYLLABLE_KERNEL_HEADER_VIEW ${CMAKE_BINARY_DIR}/kernel-headers)
file(MAKE_DIRECTORY ${SYLLABLE_KERNEL_HEADER_VIEW})
file(CREATE_LINK ${SYLLABLE_HEADERS}/atheos ${SYLLABLE_KERNEL_HEADER_VIEW}/syllable SYMBOLIC)

# Where libkernel.so (the kernel's exported symbols, linked by -kernel) is built
set(SYLLABLE_KERNEL_LIBDIR ${CMAKE_BINARY_DIR}/system/sys/kernel/kernel)

set(_SYLLABLE_ONE INSTALL OUTPUT_NAME)
set(_SYLLABLE_MULTI SOURCES INCLUDES DEFINES OPTIONS LIBS)

# Settings shared by every kind of target (A_* from the caller's parse)
macro(_syllable_common name)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${A_INCLUDES})
  target_compile_definitions(${name} PRIVATE ${A_DEFINES})
  target_compile_options(${name} PRIVATE ${A_OPTIONS})
  target_link_libraries(${name} PRIVATE ${A_LIBS})
  if(A_OUTPUT_NAME)
    set_target_properties(${name} PROPERTIES OUTPUT_NAME ${A_OUTPUT_NAME})
  endif()
  if(A_INSTALL)
    install(TARGETS ${name} DESTINATION ${A_INSTALL})
  endif()
endmacro()

function(syllable_driver name)
  cmake_parse_arguments(A "" "${_SYLLABLE_ONE}" "${_SYLLABLE_MULTI}" ${ARGN})
  add_executable(${name} ${A_SOURCES})
  _syllable_common(${name})
  target_include_directories(${name} BEFORE PRIVATE ${SYLLABLE_KERNEL_HEADER_VIEW})
  target_include_directories(${name} SYSTEM PRIVATE ${SYLLABLE_HEADERS})
  target_compile_options(${name} PRIVATE -kernel -fno-PIC)
  target_link_options(${name} PRIVATE -kernel -L${SYLLABLE_KERNEL_LIBDIR})
  add_dependencies(${name} libkernel)
endfunction()

function(syllable_program name)
  cmake_parse_arguments(A "" "${_SYLLABLE_ONE}" "${_SYLLABLE_MULTI}" ${ARGN})
  add_executable(${name} ${A_SOURCES})
  _syllable_common(${name})
  target_include_directories(${name} SYSTEM PRIVATE ${SYLLABLE_HEADERS})
  target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
endfunction()
