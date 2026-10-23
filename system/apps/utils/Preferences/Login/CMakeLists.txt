# Login preferences
syllable_program(prefs_login OUTPUT_NAME Login SOURCES main.cpp login_window.cpp
  LIBS libsyllable crypt STRIP)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.png)
syllable_resources(prefs_login FILES ${_res})
syllable_catalogs(prefs_login NAME Login DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
# installed as "Login Details", setuid root (it changes passwords)
install(PROGRAMS $<TARGET_FILE:prefs_login> DESTINATION applications/preferences RENAME "Login Details"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID)
