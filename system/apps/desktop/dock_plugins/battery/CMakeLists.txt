# Battery dock plugin
syllable_library(dock_battery SONAME Battery.so SOURCES Battery.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../include
  OPTIONS -ffast-math LIBS libsyllable libmedia STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(dock_battery FILES ${_res})
