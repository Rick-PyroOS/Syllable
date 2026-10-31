syllable_library(assistant_application SONAME application.so SOURCES provider.cpp
  OPTIONS -ffast-math LIBS libsyllable STRIP INSTALL system/extensions/assistant)
