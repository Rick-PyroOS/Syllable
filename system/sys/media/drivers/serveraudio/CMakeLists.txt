# Media server audio output
syllable_library(serveraudio SONAME serveraudio.so SOURCES server_output.cpp
  LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
