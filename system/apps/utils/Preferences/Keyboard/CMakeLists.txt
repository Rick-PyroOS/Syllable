# Keyboard preferences
syllable_program(prefs_keyboard OUTPUT_NAME Keyboard SOURCES main.cpp application.cpp mainwindow.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.png)
syllable_resources(prefs_keyboard FILES ${_res} ADD keymaps.db=resources/keymaps.db)
syllable_catalogs(prefs_keyboard NAME Keyboard DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
