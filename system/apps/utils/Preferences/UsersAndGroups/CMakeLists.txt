# Users preferences
syllable_program(prefs_users OUTPUT_NAME Users SOURCES main.cpp users_window.cpp user_propertiesdlg.cpp changepwddlg.cpp group_propertiesdlg.cpp autologin_view.cpp
  LIBS libsyllable crypt STRIP)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.png)
syllable_resources(prefs_users FILES ${_res})
syllable_catalogs(prefs_users NAME UsersAndGroups DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
install(PROGRAMS $<TARGET_FILE:prefs_users> DESTINATION applications/Preferences RENAME "Users & Groups")
