# ColdFishRemote dock plugin
syllable_library(dock_coldfishremote SONAME ColdFishRemote.so SOURCES ColdFishRemote.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/dock)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(dock_coldfishremote FILES ${_res})
syllable_catalogs(dock_coldfishremote NAME ColdFishRemote DIR resources LANGS da de el es fr hu nl pl pt-br ru sv EMBED)
