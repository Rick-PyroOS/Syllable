# The Syllable kernel.  Phase 1: libkernel.so only (what -kernel links
# against); kernel.so itself moves over in phase 2.

set(EXPORTS ${CMAKE_CURRENT_BINARY_DIR}/kernel.exports.c)
add_custom_command(OUTPUT ${EXPORTS}
  COMMAND ${CMAKE_COMMAND} -DKSYMS=${CMAKE_CURRENT_SOURCE_DIR}/ksyms.c -DOUT=${EXPORTS}
          -P ${PROJECT_SOURCE_DIR}/cmake/scripts/kernel-exports.cmake
  DEPENDS ksyms.c ${PROJECT_SOURCE_DIR}/cmake/scripts/kernel-exports.cmake)

set(LIBKERNEL ${CMAKE_CURRENT_BINARY_DIR}/libkernel.so)
add_custom_command(OUTPUT ${LIBKERNEL}
  COMMAND ${CMAKE_C_COMPILER} -nostdlib -w ${EXPORTS} -o ${LIBKERNEL}
  DEPENDS ${EXPORTS}
  COMMENT "Linking libkernel.so")
add_custom_target(libkernel ALL DEPENDS ${LIBKERNEL})
install(PROGRAMS ${LIBKERNEL} DESTINATION system/libraries)
