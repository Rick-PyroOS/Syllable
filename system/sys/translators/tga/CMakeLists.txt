# Targa translator (libsyllable add-on)
syllable_library(tgatrans SONAME tgatrans.so SOURCES tgatrans.cpp
  LIBS libsyllable INSTALL system/extensions/translators)
