# print-queue: needs CUPS, which is not a port here yet; built once a
# syllable::cups target exists
if(TARGET syllable::cups)
  syllable_program(print_queue OUTPUT_NAME print-queue SOURCES print-queue.cpp
    LIBS libsyllable syllable::cups STRIP INSTALL system/programs)
  file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png)
  syllable_resources(print_queue FILES ${_res})
  syllable_catalogs(print_queue NAME PrintQueue DIR resources LANGS bg da de el es fr hu nl ru sv EMBED)
else()
  message(STATUS "print-queue: skipped (no CUPS port)")
endif()
