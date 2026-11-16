# Raw (PCM) codec
syllable_library(raw SONAME raw.so SOURCES raw_codec.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
