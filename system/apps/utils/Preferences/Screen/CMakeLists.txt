# Screen preferences
syllable_program(prefs_screen OUTPUT_NAME Screen SOURCES main.cpp mainwindow.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
syllable_resources(prefs_screen FILES icon48x48.png)
syllable_catalogs(prefs_screen NAME Screen DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
