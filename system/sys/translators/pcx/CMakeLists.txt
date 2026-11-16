# PCX translator (libsyllable add-on)
syllable_library(pcxtrans SONAME pcxtrans.so SOURCES pcxtrans.cpp
  LIBS libsyllable INSTALL system/extensions/translators)
