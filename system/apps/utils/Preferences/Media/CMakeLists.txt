# Media preferences, installed as "Audio & Video" (start.sh runs it with
# --setupdefaults on first boot)
syllable_program(media-prefs OUTPUT_NAME Media SOURCES main.cpp mainwindow.cpp
  LIBS libsyllable libmedia STRIP)
syllable_resources(media-prefs FILES default.png icon48x48.png plugins.png sounds.png)
syllable_catalogs(media-prefs NAME Media DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
install(PROGRAMS $<TARGET_FILE:media-prefs> DESTINATION applications/preferences RENAME "Audio & Video")
