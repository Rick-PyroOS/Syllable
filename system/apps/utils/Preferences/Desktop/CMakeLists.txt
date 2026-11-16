# Desktop preferences
syllable_program(prefs_desktop OUTPUT_NAME Desktop SOURCES Prefs.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
syllable_resources(prefs_desktop FILES icon48x48.png)
syllable_catalogs(prefs_desktop NAME Desktop DIR resources LANGS da de el es fr hu lt nl pt-br ru sv EMBED)
