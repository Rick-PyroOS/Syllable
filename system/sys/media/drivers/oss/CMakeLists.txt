# OSS-compatible audio output (for the kernel's sound drivers)
syllable_library(oss SONAME oss.so SOURCES mixerchannel.cpp mixerview.cpp oss_output.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
syllable_attribute(system/extensions/media/oss.so os::Category AudioDriverCompat)
