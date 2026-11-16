# The dock (taskbar)
syllable_program(dock OUTPUT_NAME Dock SOURCES Dock.cpp DockMenu.cpp
  OPTIONS -fexceptions -ffast-math LIBS libsyllable INSTALL system/programs)
syllable_resources(dock FILES logout.png Syllable.png window.png folder.png)
syllable_catalogs(dock NAME Dock DIR resources LANGS da de el es fr hu nl ru sl sv EMBED)
