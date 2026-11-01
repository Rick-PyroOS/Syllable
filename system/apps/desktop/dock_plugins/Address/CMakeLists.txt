# Address dock plugin
syllable_library(dock_address SONAME Address.so SOURCES Address.cpp Settings.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.png ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.html)
syllable_resources(dock_address FILES ${_res})
