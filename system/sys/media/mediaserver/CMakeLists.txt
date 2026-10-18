# The media server (started by start.sh as /system/mediaserver)
syllable_program(mediaserver SOURCES mediaserver.cpp mediacontrols.cpp barview.cpp
  LIBS libsyllable libmedia STRIP INSTALL system)
syllable_resources(mediaserver FILES icon24x24.png icon48x48.png)
