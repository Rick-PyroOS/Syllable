# Mixer dock plugin
syllable_library(dock_mixer SONAME Mixer.so SOURCES Mixer.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.png)
syllable_resources(dock_mixer FILES ${_res})
