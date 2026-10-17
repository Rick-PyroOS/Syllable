# The desktop: icons and wallpaper behind every window
syllable_program(desktop SOURCES main.cpp application.cpp desktop.cpp
  OPTIONS -fexceptions -ffast-math LIBS libsyllable INSTALL system/programs)
syllable_resources(desktop FILES icon48x48.png)
syllable_catalogs(desktop NAME desktop DIR resources LANGS da de el es fr hu nl ru sl sv EMBED)
