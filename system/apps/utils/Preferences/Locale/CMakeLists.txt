# Locale preferences
syllable_program(prefs_locale OUTPUT_NAME Locale SOURCES main.cpp mainwindow.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.png ${CMAKE_CURRENT_SOURCE_DIR}/*.png)
syllable_resources(prefs_locale FILES ${_res} ADD languages.db=resources/languages.db)
syllable_catalogs(prefs_locale NAME Locale DIR resources LANGS da de el es hu nl pl pt-br ru sv EMBED)
