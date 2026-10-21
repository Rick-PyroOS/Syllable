# Camera dock plugin
syllable_library(dock_camera SONAME Camera.so SOURCES Camera.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(dock_camera FILES ${_res})
