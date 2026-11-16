# Printer dock plugin
syllable_library(dock_printer SONAME Printer.so SOURCES Printer.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.png)
syllable_resources(dock_printer FILES ${_res})
