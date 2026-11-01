# TIFF translator (libsyllable add-on)
syllable_library(tifftrans SONAME tifftrans.so SOURCES tifftrans.cpp
  LIBS libsyllable syllable::tiff INSTALL system/extensions/translators)
