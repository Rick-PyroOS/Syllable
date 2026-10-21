# File browser (system/programs/FileBrowser)
syllable_program(file_browser OUTPUT_NAME FileBrowser
  SOURCES main.cpp application.cpp mainwindow.cpp toolbar.cpp
  OPTIONS -fexceptions -ffast-math LIBS libsyllable INSTALL system/programs)
syllable_resources(file_browser FILES home.png up.png breaker.png view.png icon24x24.png icon48x48.png reload.png back.png)
syllable_catalogs(file_browser NAME FileBrowser DIR resources LANGS da de el es fr hu nl ru sv EMBED)
