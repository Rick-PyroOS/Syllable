# CD-ROM input
syllable_library(cdrom SONAME cdrom.so SOURCES cdrom.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
