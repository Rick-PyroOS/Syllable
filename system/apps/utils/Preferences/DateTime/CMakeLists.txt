# DateTime preferences
syllable_program(prefs_datetime OUTPUT_NAME DateTime SOURCES main.cpp mainwindow.cpp datepicker.cpp timepicker.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/timezone.info ${CMAKE_CURRENT_SOURCE_DIR}/*.png)
syllable_resources(prefs_datetime FILES ${_res})
syllable_catalogs(prefs_datetime NAME DateTime DIR resources LANGS da de el es fr hu lt nl pt-br ru sv EMBED)
