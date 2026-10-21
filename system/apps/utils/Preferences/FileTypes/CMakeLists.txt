# FileTypes preferences
syllable_program(prefs_filetypes OUTPUT_NAME FileTypes SOURCES main.cpp application.cpp mainwindow.cpp newtype.cpp
  LIBS libsyllable STRIP INSTALL applications/Preferences)
syllable_resources(prefs_filetypes FILES icon48x48.png)
syllable_catalogs(prefs_filetypes NAME FileTypes DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
