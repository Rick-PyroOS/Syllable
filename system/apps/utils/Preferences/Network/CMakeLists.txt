# Network preferences (start.sh runs it with --detect)
syllable_program(network-prefs OUTPUT_NAME Network
  SOURCES main.cpp mainwindow.cpp modifyconnection.cpp configuration.cpp detectmsgwindow.cpp validator.cpp
  LIBS libsyllable STRIP INSTALL applications/Preferences)
syllable_resources(network-prefs FILES icon48x48.png)
syllable_catalogs(network-prefs NAME Network DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
