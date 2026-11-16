# Settings editor
syllable_program(settings_editor OUTPUT_NAME SettingsEditor
  SOURCES app.cpp editstring.cpp editwin.cpp mainwin.cpp msgtotext.cpp strreq.cpp
  LIBS libsyllable STRIP INSTALL applications/SettingsEditor)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(settings_editor FILES ${_res})
syllable_catalogs(settings_editor NAME SettingsEditor DIR resources EXT catfish LANGS ru EMBED)
