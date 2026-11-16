# Printers preferences: needs CUPS, which is not a port here yet; built once
# a syllable::cups target exists
if(TARGET syllable::cups)
  syllable_program(prefs_printers OUTPUT_NAME Printers SOURCES printers.cpp print_view.cpp config.cpp model_window.cpp
    LIBS libsyllable syllable::cups STRIP INSTALL applications/preferences)
  file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png)
  syllable_resources(prefs_printers FILES ${_res})
  syllable_catalogs(prefs_printers NAME Printers DIR resources LANGS bg da de el es fr hu nl ru sv EMBED)
else()
  message(STATUS "Printers preferences: skipped (no CUPS port)")
endif()
