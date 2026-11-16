# JPEG translator (libsyllable add-on)
syllable_library(jpegtrans SONAME jpegtrans.so SOURCES jpegtrans.cpp
  LIBS libsyllable syllable::jpeg INSTALL system/extensions/translators)
