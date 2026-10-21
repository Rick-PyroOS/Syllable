# GIF translator (libsyllable add-on)
syllable_library(giftrans SONAME giftrans.so SOURCES giftrans.cpp
  LIBS libsyllable syllable::gif INSTALL system/extensions/translators)
