# Clipper dock plugin
syllable_library(dock_clipper SONAME Clipper.so SOURCES Clipper.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(dock_clipper FILES ${_res})
