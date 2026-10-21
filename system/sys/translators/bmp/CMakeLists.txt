# BMP translator (libsyllable add-on)
syllable_library(bmptrans SONAME bmptrans.so SOURCES bmptrans.cpp
  LIBS libsyllable INSTALL system/extensions/translators)
