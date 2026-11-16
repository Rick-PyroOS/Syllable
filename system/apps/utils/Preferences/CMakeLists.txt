foreach(prefs Appearance DateTime Desktop Dock FileTypes Font Keyboard Locale Login Media Mouse
              Network NetworkNG Printers Screen UsersAndGroups)
  add_subdirectory(${prefs})
endforeach()
