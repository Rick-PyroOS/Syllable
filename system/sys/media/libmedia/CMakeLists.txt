# libmedia: the media framework (codecs, inputs, outputs, the sound player)
syllable_library(libmedia SONAME libmedia.so.2 LINKS libmedia.so
  SOURCES codec.cpp input.cpp output.cpp manager.cpp inputselector.cpp soundplayer.cpp stage.cpp
  OPTIONS -ffast-math LIBS libsyllable m STRIP INSTALL system/libraries)
