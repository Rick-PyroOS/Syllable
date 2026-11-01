# Clock dock plugin
syllable_library(dock_clock SONAME Clock.so SOURCES ColorSelector.cpp Settings.cpp Clock.cpp colorbutton.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icon48x48.png)
syllable_resources(dock_clock FILES ${_res})
