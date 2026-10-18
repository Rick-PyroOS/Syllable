# Audio CD input
syllable_library(cdda SONAME cdda.so SOURCES cdda_input.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia m STRIP INSTALL system/extensions/media)
