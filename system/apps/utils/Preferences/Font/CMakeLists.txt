# Font preferences
syllable_program(prefs_font OUTPUT_NAME Font SOURCES main.cpp mainwindow.cpp
  LIBS libsyllable STRIP INSTALL applications/Preferences)
syllable_resources(prefs_font FILES icon48x48.png)
syllable_catalogs(prefs_font NAME Font DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
