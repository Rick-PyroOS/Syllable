# Appearance preferences
syllable_program(prefs_appearance OUTPUT_NAME Appearance SOURCES main.cpp mainwindow.cpp
  OPTIONS -Wall LIBS libsyllable STRIP INSTALL applications/preferences)
syllable_resources(prefs_appearance FILES icon48x48.png)
syllable_catalogs(prefs_appearance NAME Appearance DIR resources LANGS da de el es fr hu lt nl pt-br ru sv EMBED)
