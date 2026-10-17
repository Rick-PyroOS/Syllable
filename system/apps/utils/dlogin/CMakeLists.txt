# The graphical login
syllable_program(dlogin
  SOURCES main.cpp application.cpp mainwindow.cpp loginview.cpp loginimageview.cpp
          commonfuncs.cpp shutdown.cpp keymap.cpp appsettings.cpp
  OPTIONS -fexceptions -ffast-math LIBS libsyllable crypt INSTALL system/programs)
syllable_resources(dlogin FILES images/background.png images/default.png)
syllable_catalogs(dlogin NAME dlogin DIR resources LANGS da de el es fr hu nl ru sl sv EMBED)
