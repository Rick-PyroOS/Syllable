# Mouse preferences
syllable_program(prefs_mouse OUTPUT_NAME Mouse SOURCES main.cpp appwindow.cpp
  LIBS libsyllable STRIP INSTALL applications/Preferences)
syllable_resources(prefs_mouse ADD icon24x24.png=images/icon24x24.png icon48x48.png=images/icon48x48.png)
syllable_catalogs(prefs_mouse NAME mouse DIR resources LANGS ru sv EMBED)
