# HD Audio output (for the kernel's hda driver)
syllable_library(hd_audio SONAME hd_audio.so SOURCES hda_channel.cpp hda.cpp
  LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
syllable_attribute(system/extensions/media/hd_audio.so os::Category AudioDriver)
