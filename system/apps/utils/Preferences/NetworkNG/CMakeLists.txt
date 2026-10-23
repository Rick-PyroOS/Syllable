# NetworkNG: the newer network preferences.  Its Makefile installs it as
# applications/preferences/Network, the path Preferences/Network also uses;
# installed as NetworkNG here so the two do not overwrite each other.
syllable_program(prefs_networkng OUTPUT_NAME NetworkNG SOURCES main.cpp Hostname.cpp Nameserver.cpp Hosts.cpp App.cpp MainWin.cpp ListViewEditRow.cpp StringWriter.cpp StringReader.cpp Interface.cpp IFaceWin.cpp
  LIBS libsyllable STRIP INSTALL applications/preferences)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.png)
syllable_resources(prefs_networkng FILES ${_res})
syllable_catalogs(prefs_networkng NAME Network DIR resources LANGS ru EMBED)
