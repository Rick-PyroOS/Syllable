# Dock preferences
syllable_program(prefs_dock OUTPUT_NAME Dock SOURCES Prefs.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
syllable_resources(prefs_dock FILES icon48x48.png icon24x24.png)
syllable_catalogs(prefs_dock NAME Dock DIR resources LANGS da de el es fr hu lt nl pt-br ru sv EMBED)
